#!/usr/bin/env python3
# blocktools.py - utilities for manipulating blocks and transactions
# Copyright (c) 2015-2016 The Bitcoin Core developers
# Copyright (c) 2017-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from hashlib import blake2b

from .mininode import (
    CBlock, CTransaction, CTxIn, CTxOut, COutPoint,
    BLOSSOM_POW_TARGET_SPACING_RATIO,
)
from .script import CScript, OP_0, OP_EQUAL, OP_HASH160, OP_TRUE, OP_CHECKSIG

# Create a block (with regtest difficulty)
def create_block(hashprev, coinbase, nTime=None, nBits=None, hashBlockCommitments=None):
    block = CBlock()
    if nTime is None:
        import time
        block.nTime = int(time.time()+600)
    else:
        block.nTime = nTime
    block.hashPrevBlock = hashprev
    if hashBlockCommitments is None:
        # By default NUs up to Sapling are active from block 1, so we set this to the empty root.
        hashBlockCommitments = 0x3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb
    block.hashBlockCommitments = hashBlockCommitments
    if nBits is None:
        block.nBits = 0x200f0f0f # difficulty retargeting is disabled in REGTEST chainparams
    else:
        block.nBits = nBits
    block.vtx.append(coinbase)
    block.hashMerkleRoot = block.calc_merkle_root()
    block.hashAuthDataRoot = block.calc_auth_data_root()
    block.calc_sha256()
    return block

def derive_block_commitments_hash(chain_history_root, auth_data_root):
    digest = blake2b(
        digest_size=32,
        person=b'ZcashBlockCommit')
    digest.update(chain_history_root)
    digest.update(auth_data_root)
    digest.update(b'\x00' * 32)
    return digest.digest()

def serialize_script_num(value):
    r = bytearray(0)
    if value == 0:
        return r
    neg = value < 0
    absvalue = -value if neg else value
    while (absvalue):
        r.append(int(absvalue & 0xff))
        absvalue >>= 8
    if r[-1] & 0x80:
        r.append(0x80 if neg else 0)
    elif neg:
        r[-1] |= 0x80
    return r

# Create a coinbase transaction, assuming no miner fees.
# If pubkey is passed in, the coinbase output will be a P2PK output;
# otherwise an anyone-can-spend output.
def create_coinbase(height, pubkey=None, after_blossom=False, outputs=[], lockboxvalue=0):
    coinbase = CTransaction()
    coinbase.nExpiryHeight = height
    coinbase.vin.append(CTxIn(COutPoint(0, 0xffffffff),
                              CScript([height, OP_0]), 0xffffffff))
    coinbaseoutput = CTxOut()
    coinbaseoutput.nValue = int(12.5*100000000)
    if after_blossom:
        coinbaseoutput.nValue //= BLOSSOM_POW_TARGET_SPACING_RATIO
    halvings = height // 150 # regtest
    coinbaseoutput.nValue >>= halvings
    coinbaseoutput.nValue -= lockboxvalue

    if (pubkey != None):
        coinbaseoutput.scriptPubKey = CScript([pubkey, OP_CHECKSIG])
    else:
        coinbaseoutput.scriptPubKey = CScript([OP_TRUE])
    coinbase.vout = [ coinbaseoutput ]

    if len(outputs) == 0 and halvings == 0: # regtest
        froutput = CTxOut()
        froutput.nValue = coinbaseoutput.nValue // 5
        # regtest
        fraddr = bytearray([0x67, 0x08, 0xe6, 0x67, 0x0d, 0xb0, 0xb9, 0x50,
                            0xda, 0xc6, 0x80, 0x31, 0x02, 0x5c, 0xc5, 0xb6,
                            0x32, 0x13, 0xa4, 0x91])
        froutput.scriptPubKey = CScript([OP_HASH160, fraddr, OP_EQUAL])
        coinbaseoutput.nValue -= froutput.nValue
        coinbase.vout.append(froutput)

    coinbaseoutput.nValue -= sum(output.nValue for output in outputs)
    assert coinbaseoutput.nValue >= 0, coinbaseoutput.nValue
    coinbase.vout.extend(outputs)
    coinbase.calc_sha256()
    return coinbase

# Create a transaction with an anyone-can-spend output, that spends the
# nth output of prevtx.
def create_transaction(prevtx, n, sig, value_zats):
    tx = CTransaction()
    assert(n < len(prevtx.vout))
    tx.vin.append(CTxIn(COutPoint(prevtx.sha256, n), sig, 0xffffffff))
    tx.vout.append(CTxOut(value_zats, b""))
    tx.calc_sha256()
    return tx
