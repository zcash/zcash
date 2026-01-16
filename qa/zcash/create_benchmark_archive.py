#!/usr/bin/env python3
"""
@fileoverview Script to create a deterministic benchmark archive for Zcash testing.
This script fetches Zcash block and transaction data via zcash-cli, compresses UTXO data,
and packages it into a reproducible LevelDB/block archive (.tar.xz).
"""
import binascii
import calendar
import json
import plyvel
import progressbar
import os
import stat
import subprocess
import sys
import tarfile
import time
from typing import Dict, Any, List, Optional, Tuple, Iterable, Union

# --- CONFIGURATION ---
ZCASH_CLI = './src/zcash-cli'
DEFAULT_DATE = '2017-05-17'

# Installation and usage instructions for the user.
USAGE = f"""
Requirements:
- find, xz
- {ZCASH_CLI} (ensure path is correct)
- A running mainnet zcashd using the default datadir with -txindex=1

Example usage:

make -C src/leveldb/
virtualenv venv
. venv/bin/activate
# Note: Using python_abi tag for better compatibility if available, but keeping original flags for plyvel:
pip install --global-option=build_ext --global-option="-L$(pwd)/src/leveldb/out-shared" \\
  --global-option="-I$(pwd)/src/leveldb/include/" plyvel==1.3.0
pip install progressbar2
LD_LIBRARY_PATH=src/leveldb/out-shared python qa/zcash/create_benchmark_archive.py
"""

def check_dependencies():
    """Checks if required external binaries are available."""
    try:
        subprocess.run(['which', 'find', 'xz', ZCASH_CLI], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except subprocess.CalledProcessError:
        print(USAGE)
        sys.exit(1)

# --- BITCOIN/ZCASH SERIALIZATION HELPERS ---

def encode_varint(n: int) -> bytes:
    """
    Encodes an integer into a Bitcoin-style variable-length integer (varint).
    This implementation handles the specific compact encoding format.
    """
    v = bytearray()
    l = 0
    while True:
        # Standard varint logic: 7 bits data, 1 bit continuation (MSB set)
        v.append((n & 0x7F) | (0x80 if l else 0x00))
        if (n <= 0x7F):
            break
        n = (n >> 7) - 1 # Specific compact varint adjustment
        l += 1
    return bytes(v)[::-1] # Reverse for little-endian storage

def decode_varint(v: bytes) -> int:
    """
    Decodes a Bitcoin-style variable-length integer (varint) from bytes.
    Note: Assumes input bytes 'v' are already in the correct order for decoding.
    """
    n = 0
    # FIX: Use byte value directly instead of ord() which is redundant in Python 3
    for byte_val in v: 
        n = (n << 7) | (byte_val & 0x7F)
        if (byte_val & 0x80):
            n += 1
        else:
            return n
    return n # Fallback return if loop completes (should ideally not happen if varint is properly terminated)


def compress_amount(n: int) -> int:
    """Compresses a Satoshis amount using Bitcoin's compact amount encoding."""
    if n == 0:
        return 0
    
    e = 0
    # Count trailing zeros (exponent)
    while (((n % 10) == 0) and e < 9):
        n //= 10
        e += 1
    
    if e < 9:
        d = (n % 10)
        assert(d >= 1 and d <= 9)
        n //= 10
        return 1 + (n * 9 + d - 1) * 10 + e
    else:
        # Special case for 10^9 and higher multiples
        return 1 + (n - 1) * 10 + 9

# --- SCRIPT OPCODES (Standard Bitcoin Op Codes) ---
OP_DUP = 0x76
OP_EQUAL = 0x87
OP_EQUALVERIFY = 0x88
OP_HASH160 = 0xa9
OP_CHECKSIG = 0xac

# --- SCRIPT COMPRESSION HELPERS (Standard Bitcoin script types) ---

def to_key_id(script: bytes) -> bytes:
    """Checks for P2PKH script type and returns the key hash (20 bytes) if matched."""
    # P2PKH script: OP_DUP OP_HASH160 <20-byte hash> OP_EQUALVERIFY OP_CHECKSIG
    if len(script) == 25 and \
            script[0] == OP_DUP and \
            script[1] == OP_HASH160 and \
            script[2] == 20 and \
            script[23] == OP_EQUALVERIFY and \
            script[24] == OP_CHECKSIG:
        return script[3:23]
    return b''

def to_script_id(script: bytes) -> bytes:
    """Checks for P2SH script type and returns the script hash (20 bytes) if matched."""
    # P2SH script: OP_HASH160 <20-byte hash> OP_EQUAL
    if len(script) == 23 and \
            script[0] == OP_HASH160 and \
            script[1] == 20 and \
            script[22] == OP_EQUAL:
        return script[2:22]
    return b''

def to_pubkey(script: bytes) -> bytes:
    """
    Checks for P2PK script type and returns the full public key (compressed/uncompressed).
    NOTE: Assumes script validity for compression.
    """
    # Compressed Pubkey (33 bytes)
    if len(script) == 35 and \
            script[0] == 33 and \
            (script[1] == 0x02 or script[1] == 0x03) and \
            script[34] == OP_CHECKSIG:
        return script[1:34]
    # Uncompressed Pubkey (65 bytes)
    if len(script) == 67 and \
            script[0] == 65 and \
            script[66] == OP_CHECKSIG and \
            script[1] == 0x04:
        return script[1:66]
    return b''

def compress_script(script: bytes) -> bytes:
    """
    Compresses the scriptPubKey if it matches a standard format (P2PKH, P2SH, P2PK).
    Otherwise, returns the script prepended with its size.
    """
    # 1. P2PKH (0x00 prefix)
    key_id = to_key_id(script)
    if key_id:
        return b'\x00' + key_id

    # 2. P2SH (0x01 prefix)
    script_id = to_script_id(script)
    if script_id:
        return b'\x01' + script_id

    # 3. P2PK (0x02 - 0x05 prefixes)
    pubkey = to_pubkey(script)
    if pubkey:
        prefix = pubkey[0] # 0x02, 0x03 (compressed) or 0x04 (uncompressed)
        
        if prefix == 0x02 or prefix == 0x03:
            # Compressed: prefix byte + 32 bytes (Y coordinate omitted)
            return bytes([prefix]) + pubkey[1:34]
        
        elif prefix == 0x04:
            # Uncompressed: prefix byte 0x04 is removed; uses 0x04 or 0x05 based on Y parity
            # We assume it is fully valid and only need the X coordinate (32 bytes)
            # The last byte of the key (64th index) determines parity
            parity = pubkey[64] & 0x01
            # Compressed version of uncompressed key: 0x04 | parity (0x04 or 0x05)
            return bytes([0x04 | parity]) + pubkey[1:33] 

    # 4. Standard/Uncompressed script (size prefixed)
    # Original logic adds 6 to size, which is not standard compression, 
    # but kept for compatibility with the original implementation's assumed format:
    size = len(script) + 6
    return encode_varint(size) + script

# --- ARCHIVE CREATION HELPERS ---

def deterministic_filter(tarinfo: tarfile.TarInfo) -> tarfile.TarInfo:
    """
    Sets file metadata to deterministic, static values for reproducible archives.
    """
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = "root"
    # Static modification time to ensure reproducibility
    tarinfo.mtime = calendar.timegm(time.strptime(DEFAULT_DATE, '%Y-%m-%d'))
    
    # Set permissions: owner read/write, group read, others read
    tarinfo.mode &= ~stat.S_IWGRP # Clear group write permission
    tarinfo.mode |= stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP
    
    # Set execution permissions only for directories
    if tarinfo.isdir():
        tarinfo.mode |= stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH
    else:
        # Clear execute permissions for regular files
        tarinfo.mode &= ~stat.S_IXUSR & ~stat.S_IXGRP & ~stat.S_IXOTH
    return tarinfo

def run_zcash_cli(args: List[str]) -> str:
    """Executes zcash-cli command and handles potential errors."""
    try:
        # Using check=True will raise CalledProcessError on non-zero exit code
        output = subprocess.run([ZCASH_CLI] + args, 
                                capture_output=True, 
                                text=True,
                                check=True)
        return output.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running zcash-cli with args: {args}")
        print(f"Stderr: {e.stderr}")
        sys.exit(1)

# --- MAIN LOGIC ---

def create_benchmark_archive(blk_hash: str):
    """
    Creates a benchmark archive for a given block hash, including block data 
    and a LevelDB of required UTXO inputs.
    """
    print(f"\n--- Processing Block: {blk_hash} ---")
    
    # 1. Get Block Header (used for height and transaction list)
    blk_raw = run_zcash_cli(['getblock', blk_hash, '1'])
    blk: Dict[str, Any] = json.loads(blk_raw)
    
    print(f"Height: {blk['height']}")
    print(f"Transactions: {len(blk['tx'])}")

    archive_dir = 'benchmark'
    db_name = f"block-{blk['height']}-inputs"
    db_path = os.path.join(archive_dir, db_name)
    
    os.makedirs(archive_dir, exist_ok=True)
    
    # 2. Save Block Data
    block_data_raw = run_zcash_cli(['getblock', blk_hash, 'false'])
    with open(os.path.join(archive_dir, f"block-{blk['height']}.dat"), 'wb') as f:
        f.write(binascii.unhexlify(block_data_raw.strip()))

    # 3. Check for JoinSplit (Shielded) Transactions
    txs_data = [json.loads(run_zcash_cli(['getrawtransaction', tx_id, '1'])) for tx_id in blk['tx']]
    js_txs = sum(1 for tx in txs_data if len(tx.get('vjoinsplit', [])) > 0)
    
    if js_txs:
        print(f"Block contains {js_txs} JoinSplit-containing transactions. Skipping UTXO collection.")
        # Proceed directly to archive creation with only block file
    else:
        # 4. Collect and process unique UTXO inputs
        inputs = [(x['txid'], x['vout']) for tx in txs_data for x in tx['vin'] if 'txid' in x]
        print(f"Total inputs: {len(inputs)}")

        unique_inputs: Dict[str, List[int]] = {}
        for txid, vout in sorted(inputs):
            # Skip inputs from transactions within the same block (BIP 30 check)
            if txid in blk['tx']:
                continue 
            
            # Map txid to list of vouts being spent
            if txid in unique_inputs:
                unique_inputs[txid].append(vout)
            else:
                unique_inputs[txid] = [vout]
        print(f"Unique input transactions: {len(unique_inputs)}")

        # 5. Create LevelDB and write compressed UTXOs
        try:
            db = plyvel.DB(db_path, create_if_missing=True)
        except Exception as e:
            print(f"Error initializing LevelDB at {db_path}: {e}")
            print("Did you run 'make -C src/leveldb/' and set LD_LIBRARY_PATH correctly?")
            sys.exit(1)
            
        wb = db.write_batch()
        bar = progressbar.ProgressBar(redirect_stdout=True)
        print('Collecting and compressing input coins for block')
        
        # Collect block headers for height
        # Note: If multiple input txs have the same blockhash, we only call getblockheader once
        block_headers_cache: Dict[str, int] = {} 

        for txid in bar(unique_inputs.keys()):
            rawtx_json = run_zcash_cli(['getrawtransaction', txid, '1'])
            rawtx: Dict[str, Any] = json.loads(rawtx_json)

            # Determine the UTXO compression mask
            # This complex logic is responsible for efficiently encoding which outputs are unspent
            mask_code = 0
            mask_size = 0
            num_vouts = len(rawtx.get('vout', []))
            
            # Skip if no vouts (shouldn't happen for UTXO inputs)
            if num_vouts == 0: continue
            
            # The original logic for calculating mask_size/mask_code based on vout[2] and onwards is complex
            # and may be simplified by checking the max VOUT index being spent.
            # We retain the original loop structure for compatibility:
            b = 0
            while 2 + b * 8 < num_vouts:
                zero = True
                for i in range(8):
                    vout_index = 2 + b * 8 + i
                    if vout_index in unique_inputs[txid]:
                        zero = False
                        break
                
                if not zero:
                    mask_size = b + 1
                b += 1

            # Determine code for CTxOut. The code indicates special compression formats:
            coinbase = len(rawtx['vin']) == 1 and 'coinbase' in rawtx['vin'][0]
            first = num_vouts > 0 and 0 in unique_inputs[txid]
            second = num_vouts > 1 and 1 in unique_inputs[txid]
            
            # The original code's calculation of 'code' is complex and implementation-specific
            # We verify the components used:
            code_base = 8 * (mask_code - (0 if first or second else 1)) 
            code = code_base + (1 if coinbase else 0) + (2 if first else 0) + (4 if second else 0)

            coins = bytearray()
            
            # Serialized format components:
            # - VARINT(nVersion)
            coins.extend(encode_varint(rawtx['version']))
            # - VARINT(nCode)
            coins.extend(encode_varint(code))
            
            # - unspentness bitvector (for vout[2] and further)
            for b in range(mask_size):
                avail = 0
                for i in range(8):
                    vout_index = 2 + b * 8 + i
                    if vout_index in unique_inputs[txid]:
                        avail |= (1 << i)
                coins.append(avail)

            # - The non-spent CTxOuts (Compressed Output Data)
            for i in range(num_vouts):
                if i in unique_inputs[txid]:
                    # Zcash/Bitcoin amounts are stored in valueZat (Satoshis equivalent)
                    value_zat = int(rawtx['vout'][i]['valueZat'])
                    coins.extend(encode_varint(compress_amount(value_zat)))
                    
                    # Get scriptPubKey bytes
                    script_hex = rawtx['vout'][i]['scriptPubKey']['hex']
                    script_bytes = binascii.unhexlify(script_hex)
                    coins.extend(compress_script(script_bytes))
                    
            # - VARINT(nHeight) - The height of the transaction's block
            block_hash = rawtx['blockhash']
            if block_hash not in block_headers_cache:
                header_raw = run_zcash_cli(['getblockheader', block_hash])
                block_headers_cache[block_hash] = json.loads(header_raw)['height']
            
            block_height = block_headers_cache[block_hash]
            coins.extend(encode_varint(block_height))

            # Write to LevelDB batch
            # Key format: 'c' + reversed(txid_bytes)
            db_key = b'c' + bytes(binascii.unhexlify(txid)[::-1])
            db_val = bytes(coins)
            wb.put(db_key, db_val)

        # 6. Finalize LevelDB and close
        wb.write()
        db.close()
    
    # 7. Create Reproducible Archive
    # LevelDB creates LOG files, which must be removed before archiving for determinism.
    log_file = os.path.join(db_path, 'LOG')
    if os.path.exists(log_file):
      os.remove(log_file)
      
    # Find all files in the benchmark directory
    files = subprocess.check_output(['find', archive_dir]).decode('utf8').strip().split('\n')
    
    archive_name = f"block-{blk['height']}.tar"
    print(f"Creating deterministic archive: {archive_name}")
    
    with tarfile.open(archive_name, 'w') as tar:
        # Sort files to ensure deterministic archive content order
        for name in sorted(files):
            # Only add if the file exists (find might return parent dir)
            if os.path.exists(name):
              tar.add(name, recursive=False, filter=deterministic_filter)
              
    # Compress the archive using xz
    subprocess.check_call(['xz', '-6', archive_name])
    
    final_archive_name = f"{archive_name}.xz"
    print(f'Created archive {final_archive_name}')
    
    # Cleanup local directory
    subprocess.call(['rm', '-r', archive_dir])

if __name__ == '__main__':
    check_dependencies()
    # Execute for predefined block hashes
    create_benchmark_archive('0000000007cdb809e48e51dd0b530e8f5073e0a9e9bd7ae920fe23e874658c74')
    create_benchmark_archive('0000000000651d7fb11f0dd1be8dc87c29dca74cbf91140c6aafbacc09e7da04')
    create_benchmark_archive('00000000012e0b5a8fb0d67c995b92e7c097ddeeab1151de61c4f484611fde11')
