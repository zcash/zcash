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

ZCASH_CLI = './src/zcash-cli'
USAGE = """
Requirements:
- find
- xz
- %s (edit ZCASH_CLI in this script to alter the path)
- A running mainnet zcashd using the default datadir with -txindex=1

Example usage:

make -C src/leveldb/
virtualenv venv
. venv/bin/activate
pip install --global-option=build_ext --global-option="-L$(pwd)/src/leveldb/" --global-option="-I$(pwd)/src/leveldb/include/" plyvel
pip install progressbar2
LD_LIBRARY_PATH=src/leveldb python qa/zcash/create_benchmark_archive.py
""" % ZCASH_CLI

def check_deps():
    if subprocess.call(['which', 'find', 'xz', ZCASH_CLI], stdout=subprocess.PIPE):
        print USAGE
        sys.exit()

def encode_varint(n):
    v = bytearray()
    l = 0
    while True:
        v.append((n & 0x7F) | (0x80 if l else 0x00))
        if (n <= 0x7F):
            break
        n = (n >> 7) - 1
        l += 1
    return bytes(v)[::-1]

def decode_varint(v):
    n = 0
    for ch in range(len(v)):
        n = (n << 7) | (ord(v[ch]) & 0x7F)
        if (ord(v[ch]) & 0x80):
            n += 1
        else:
            return n

def compress_amount(n):
    if n == 0:
        return 0
    e = 0
    while (((n % 10) == 0) and e < 9):
        n /= 10
        e += 1
    if e < 9:
        d = (n % 10)
        assert(d >= 1 and d <= 9)
        n /= 10
        return 1 + (n*9 + d - 1)*10 + e
    else:
        return 1 + (n - 1)*10 + 9

OP_DUP = 0x76
OP_EQUAL = 0x87
OP_EQUALVERIFY = 0x88
OP_HASH160 = 0xa9
OP_CHECKSIG = 0xac
def to_key_id(script):
    if len(script) == 25 and \
            script[0] == OP_DUP and \
            script[1] == OP_HASH160 and \
            script[2] == 20 and \
            script[23] == OP_EQUALVERIFY and \
            script[24] == OP_CHECKSIG:
        return script[3:23]
    return bytes()

def to_script_id(script):
    if len(script) == 23 and \
            script[0] == OP_HASH160 and \
            script[1] == 20 and \
            script[22] == OP_EQUAL:
        return script[2:22]
    return bytes()

def to_pubkey(script):
    if len(script) == 35 and \
            script[0] == 33 and \
            script[34] == OP_CHECKSIG and \
            (script[1] == 0x02 or script[1] == 0x03):
        return script[1:34]
    if len(script) == 67 and \
            script[0] == 65 and \
            script[66] == OP_CHECKSIG and \
            script[1] == 0x04:
        return script[1:66] # assuming is fully valid
    return bytes()

def compress_script(script):
    result = bytearray()

    key_id = to_key_id(script)
    if key_id:
        result.append(0x00)
        result.extend(key_id)
        return bytes(result)

    script_id = to_script_id(script)
    if script_id:
        result.append(0x01)
        result.extend(script_id)
        return bytes(result)

    pubkey = to_pubkey(script)
    if pubkey:
        result.append(0x00)
        result.extend(pubkey[1:33])
        if pubkey[0] == 0x02 or pubkey[0] == 0x03:
            result[0] = pubkey[0]
            return bytes(result)
        elif pubkey[0] == 0x04:
            result[0] = 0x04 | (pubkey[64] & 0x01)
            return bytes(result)

    size = len(script) + 6
    result.append(encode_varint(size))
    result.extend(script)
    return bytes(result)

def deterministic_filter(tarinfo):
    tarinfo.uid = tarinfo.gid = 0
    tarinfo.uname = tarinfo.gname = "root"
    tarinfo.mtime = calendar.timegm(time.strptime('2017-05-17', '%Y-%m-%d'))
    tarinfo.mode |= stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP
    tarinfo.mode &= ~stat.S_IWGRP
    if tarinfo.isdir():
        tarinfo.mode |= \
            stat.S_IXUSR | \
            stat.S_IXGRP | \
            stat.S_IXOTH
    else:
        tarinfo.mode &= \
            ~stat.S_IXUSR & \
            ~stat.S_IXGRP & \
            ~stat.S_IXOTH
    return tarinfo

def create_benchmark_archive(blk_hash):
    blk = json.loads(subprocess.check_output([ZCASH_CLI, 'getblock', blk_hash]))
    print 'Height: %d' % blk['height']
    print 'Transactions: %d' % len(blk['tx'])

    os.mkdir('benchmark')
    with open('benchmark/block-%d.dat' % blk['height'], 'wb') as f:
        f.write(binascii.unhexlify(subprocess.check_output([ZCASH_CLI, 'getblock', blk_hash, 'false']).strip()))

    txs = [json.loads(subprocess.check_output([ZCASH_CLI, 'getrawtransaction', tx, '1'])
                     ) for tx in blk['tx']]

    js_txs = len([tx for tx in txs if len(tx['vjoinsplit']) > 0])
    if js_txs:
        print 'Block contains %d JoinSplit-containing transactions' % js_txs
        return

    inputs = [(x['txid'], x['vout']) for tx in txs for x in tx['vin'] if x.has_key('txid')]
    print 'Total inputs: %d' % len(inputs)

    unique_inputs = {}
    for i in sorted(inputs):
        if unique_inputs.has_key(i[0]):
            unique_inputs[i[0]].append(i[1])
        else:
            unique_inputs[i[0]] = [i[1]]
    print 'Unique input transactions: %d' % len(unique_inputs)

    db_path = 'benchmark/block-%d-inputs' % blk['height']
    db = plyvel.DB(db_path, create_if_missing=True)
    wb = db.write_batch()
    bar = progressbar.ProgressBar(redirect_stdout=True)
    print 'Collecting input coins for block'
    for tx in bar(unique_inputs.keys()):
        rawtx = json.loads(subprocess.check_output([ZCASH_CLI, 'getrawtransaction', tx, '1']))

        mask_size = 0
        mask_code = 0
        b = 0
        while 2+b*8 < len(rawtx['vout']):
            zero = True
            i = 0
            while i < 8 and 2+b*8+i < len(rawtx['vout']):
                if 2+b*8+i in unique_inputs[tx]:
                    zero = False
                i += 1
            if not zero:
                mask_size = b + 1
                mask_code += 1
            b += 1

        coinbase = len(rawtx['vin']) == 1 and 'coinbase' in rawtx['vin'][0]
        first = len(rawtx['vout']) > 0 and 0 in unique_inputs[tx]
        second = len(rawtx['vout']) > 1 and 1 in unique_inputs[tx]
        code = 8*(mask_code - (0 if first or second else 1)) + \
            (1 if coinbase else 0) + \
            (2 if first else 0) + \
            (4 if second else 0)

        coins = bytearray()
        # Serialized format:
        # - VARINT(nVersion)
        coins.extend(encode_varint(rawtx['version']))
        # - VARINT(nCode)
        coins.extend(encode_varint(code))
        # - unspentness bitvector, for vout[2] and further; least significant byte first
        for b in range(mask_size):
            avail = 0
            i = 0
            while i < 8 and 2+b*8+i < len(rawtx['vout']):
                if 2+b*8+i in unique_inputs[tx]:
                    avail |= (1 << i)
                i += 1
            coins.append(avail)
        # - the non-spent CTxOuts (via CTxOutCompressor)
        for i in range(len(rawtx['vout'])):
            if i in unique_inputs[tx]:
                coins.extend(encode_varint(compress_amount(int(rawtx['vout'][i]['valueZat']))))
                coins.extend(compress_script(
                    binascii.unhexlify(rawtx['vout'][i]['scriptPubKey']['hex'])))
        # - VARINT(nHeight)
        coins.extend(encode_varint(json.loads(
            subprocess.check_output([ZCASH_CLI, 'getblockheader', rawtx['blockhash']])
            )['height']))

        db_key = b'c' + bytes(binascii.unhexlify(tx)[::-1])
        db_val = bytes(coins)
        wb.put(db_key, db_val)

    wb.write()
    db.close()

    # Make reproducible archive
    os.remove('%s/LOG' % db_path)
    files = subprocess.check_output(['find', 'benchmark']).strip().split('\n')
    archive_name = 'block-%d.tar' % blk['height']
    tar = tarfile.open(archive_name, 'w')
    for name in sorted(files):
        tar.add(name, recursive=False, filter=deterministic_filter)
    tar.close()
    subprocess.check_call(['xz', '-6', archive_name])
    print 'Created archive %s.xz' % archive_name
    subprocess.call(['rm', '-r', 'benchmark'])

if __name__ == '__main__':
    check_deps()
    create_benchmark_archive('0000000007cdb809e48e51dd0b530e8f5073e0a9e9bd7ae920fe23e874658c74')
