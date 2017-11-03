# blocktools.py - utilities for manipulating blocks and transactions
#
# Distributed under the MIT/X11 software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#

from mininode import CBlock, CTransaction, CTxIn, CTxOut, COutPoint
from script import CScript, OP_0, OP_EQUAL, OP_HASH160

# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None, nBits=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    block.hashPrevBlock = hashprev
    if nBits is None:
        block.nBits = 0x200f0f0f # Will break after a difficulty adjustment...
    else:
        block.nBits = nBits
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.calc_sha256()
    return block

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(chr(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

counter=1
# Create an anyone-can-spend coinbase transaction, assuming no miner fees
def create_coinbase(heightAdjust = 0):
    global counter
    coinbase = CTransaction()
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff), 
                CScript([counter+heightAdjust, OP_0]), 0xffffffff))
    counter += 1
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = int(12.5*100000000)
    halvings = int((counter+heightAdjust)/150) # regtest
    coinbaseoutput.nValue >>= halvings
    coinbaseoutput.scriptPubKey = ""
    coinbase.vout = [ coinbaseoutput ]
    if halvings == 0: # regtest
        froutput = CTxOut()
        froutput.nValue = coinbaseoutput.nValue / 5
        # regtest
        fraddr = bytearray([0x67, 0x08, 0xe6, 0x67, 0x0d, 0xb0, 0xb9, 0x50,
                            0xda, 0xc6, 0x80, 0x31, 0x02, 0x5c, 0xc5, 0xb6,
                            0x32, 0x13, 0xa4, 0x91])
        froutput.scriptPubKey = CScript([OP_HASH160, fraddr, OP_EQUAL])
        coinbaseoutput.nValue -= froutput.nValue
        coinbase.vout = [ coinbaseoutput, froutput ]
    coinbase.calc_sha256()
    return coinbase

# Create a transaction with an anyone-can-spend output, that spends the
# nth output of prevtx.
def create_transaction(prevtx, n, sig, value):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value, ""))
    tx.calc_sha256()
    return tx
