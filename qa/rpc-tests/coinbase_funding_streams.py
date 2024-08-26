#!/usr/bin/env python3
# Copyright (c) 2020-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.blocktools import create_block, create_coinbase
from test_framework.mininode import (
    fundingstream,
    nuparams,
    uint256_from_reversed_hex,
    CTxOut,
    COIN,
)
from test_framework.script import (
    CScript,
    OP_CHECKSIG, OP_DUP, OP_EQUAL, OP_EQUALVERIFY, OP_HASH160,
)
from test_framework.util import (
    assert_equal,
    bitcoind_processes,
    connect_nodes,
    start_node,
    BLOSSOM_BRANCH_ID,
    CANOPY_BRANCH_ID,
    NU6_BRANCH_ID,
)

from base58 import b58decode_check

def redeem_script(addr):
    raw_addr = b58decode_check(addr)
    if len(raw_addr) == 22:
        addr_hash = raw_addr[2:]
        if raw_addr.startswith(b'\x1C\xBD') or raw_addr.startswith(b'\x1C\xBA'):
            return CScript([OP_HASH160, addr_hash, OP_EQUAL])
        elif raw_addr.startswith(b'\x1C\xB8') or raw_addr.startswith(b'\x1D\x25'):
            return CScript([OP_DUP, OP_HASH160, addr_hash, OP_EQUALVERIFY, OP_CHECKSIG])

    raise ValueError("unrecognized address type")

class CoinbaseFundingStreamsTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.cache_behavior = 'clean'

    def start_node_with(self, index, extra_args=[]):
        args = [
            nuparams(BLOSSOM_BRANCH_ID, 2),
            nuparams(CANOPY_BRANCH_ID, 3),
            nuparams(NU6_BRANCH_ID, 4),
            "-nurejectoldversions=false",
            "-allowdeprecated=getnewaddress",
            "-allowdeprecated=z_getnewaddress",
            "-allowdeprecated=z_getbalance",
            "-allowdeprecated=z_gettotalbalance",
        ]
        return start_node(index, self.options.tmpdir, args + extra_args)

    def setup_network(self, split=False):
        self.nodes = []
        self.nodes.append(self.start_node_with(0))
        self.nodes.append(self.start_node_with(1))
        connect_nodes(self.nodes[1], 0)
        self.is_network_split = False
        self.sync_all()

    def run_test(self):
        # Generate a shielded address for node 1 for miner rewards,
        miner_addr = self.nodes[1].z_getnewaddress('sapling')

        # Generate Sapling and transparent addresses (belonging to node 0) for funding stream rewards.
        # For transparent funding stream outputs, strictly speaking the protocol spec only specifies
        # paying to a P2SH multisig, but P2PKH funding addresses also work in zcashd.
        fs_sapling = self.nodes[0].z_getnewaddress('sapling')
        fs_transparent = self.nodes[0].getnewaddress()

        print("Activating Blossom")
        self.nodes[1].generate(2)
        self.sync_all()
        # We won't need to mine on node 1 from this point onward.

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 2)

        # Restart both nodes with funding streams.
        self.nodes[0].stop()
        bitcoind_processes[0].wait()
        self.nodes[1].stop()
        bitcoind_processes[1].wait()
        new_args = [
            "-mineraddress=%s" % miner_addr,
            "-minetolocalwallet=0",
            fundingstream(0, 3, 4, [fs_sapling]), # FS_ZIP214_BP: 7%
            fundingstream(1, 3, 4, [fs_sapling]), # FS_ZIP214_ZF: 5%
            fundingstream(2, 3, 4, [fs_sapling]), # FS_ZIP214_MG: 8%
            fundingstream(3, 4, 8, [fs_transparent, fs_transparent]),   # FS_FPF_ZCG: 8%
            fundingstream(4, 4, 8, ["DEFERRED_POOL", "DEFERRED_POOL"]), # FS_DEFERRED: 12%
        ]
        self.nodes[0] = self.start_node_with(0, new_args)
        self.nodes[1] = self.start_node_with(1, new_args)
        connect_nodes(self.nodes[1], 0)
        self.sync_all()

        print("Activating Heartwood & Canopy")
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 3)

        # All miner addresses belong to node 1; check balances
        # Miner rewards are uniformly 5 zec per block (all test heights are below the first halving)
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), Decimal('15'))
        assert_equal(Decimal(walletinfo['balance']), Decimal('0'))
        assert_equal(Decimal(self.nodes[1].z_getbalance(miner_addr, 0)), Decimal('5'))
        assert_equal(Decimal(self.nodes[1].z_getbalance(miner_addr)), Decimal('5'))

        # Check that the node 0 private balance has been augmented by the funding stream payment.
        assert_equal(Decimal(self.nodes[0].z_getbalance(fs_sapling, 0)), Decimal('1.25'))
        assert_equal(Decimal(self.nodes[0].z_getbalance(fs_sapling)), Decimal('1.25'))
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['private']), Decimal('1.25'))
        assert_equal(Decimal(self.nodes[0].z_gettotalbalance()['total']), Decimal('1.25'))

        print("Activating NU5 & NU6")
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 4)

        # check that miner payments made it to node 1's wallet
        walletinfo = self.nodes[1].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), Decimal('15'))
        assert_equal(Decimal(walletinfo['balance']), Decimal('0'))
        assert_equal(Decimal(self.nodes[1].z_getbalance(miner_addr, 0)), Decimal('10'))
        assert_equal(Decimal(self.nodes[1].z_getbalance(miner_addr)), Decimal('10'))

        # Check that the node 0 *immature* transparent balance has been augmented by the funding
        # stream payment:
        # * 0.5 ZEC per block to fs_transparent
        # * 0.75 ZEC per block to the lockbox
        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), Decimal('0.5'))
        assert_equal(Decimal(walletinfo['shielded_balance']), Decimal('1.25'))

        # Check that the node 0 lockbox balance has been augmented by the funding stream payment.
        lastBlock = self.nodes[0].getblock('4')
        def check_lockbox(blk, expected):
            lockbox = next(elem for elem in blk['valuePools'] if elem['id'] == "lockbox")
            assert_equal(Decimal(lockbox['chainValue']), expected)
        check_lockbox(lastBlock, Decimal('0.75'))

        # Invalidate the last block so that we can steal its `hashBlockCommitments` field.
        self.nodes[0].invalidateblock(lastBlock['hash'])
        self.nodes[1].invalidateblock(lastBlock['hash'])
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 3)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), Decimal('0'))
        assert_equal(Decimal(walletinfo['shielded_balance']), Decimal('1.25'))

        coinbaseTx = create_coinbase(
            4,
            after_blossom=True,
            outputs=[CTxOut(int(0.5 * COIN), redeem_script(fs_transparent))],
            lockboxvalue=int(0.75 * COIN),
        )
        block = create_block(
            uint256_from_reversed_hex(lastBlock['previousblockhash']),
            coinbaseTx,
            lastBlock['time'],
            hashBlockCommitments=uint256_from_reversed_hex(lastBlock['blockcommitments']),
        )

        # A coinbase transaction that claims less than the maximum amount should be invalid after NU6.
        block.vtx[0].vout[0].nValue -= 1  # too little
        block.vtx[0].rehash()
        block.rehash()
        block.solve()
        assert_equal(self.nodes[0].submitblock(block.serialize().hex()), 'bad-cb-not-exact')

        # A coinbase transaction that claims more than the maximum amount should also be invalid
        # (with a different error).
        block.vtx[0].vout[0].nValue += 2  # too much
        block.vtx[0].rehash()
        block.rehash()
        block.solve()
        assert_equal(self.nodes[0].submitblock(block.serialize().hex()), 'bad-cb-amount')

        # A coinbase transaction that claims exactly the maximum amount should be valid.
        block.vtx[0].vout[0].nValue -= 1  # just right
        block.vtx[0].rehash()
        block.rehash()
        block.solve()
        assert_equal(self.nodes[0].submitblock(block.serialize().hex()), None)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 4)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), Decimal('0.5'))
        assert_equal(Decimal(walletinfo['shielded_balance']), Decimal('1.25'))

        check_lockbox(self.nodes[0].getblock('4'), Decimal('0.75'))

        print("Mining past the end of the funding streams")
        self.nodes[0].generate(4)
        self.sync_all()

        assert_equal(self.nodes[1].getblockchaininfo()['blocks'], 8)

        walletinfo = self.nodes[0].getwalletinfo()
        assert_equal(Decimal(walletinfo['immature_balance']), 4 * Decimal('0.5'))
        assert_equal(Decimal(walletinfo['shielded_balance']), Decimal('1.25'))

        check_lockbox(self.nodes[0].getblock('7'), 4 * Decimal('0.75'))
        check_lockbox(self.nodes[0].getblock('8'), 4 * Decimal('0.75'))

if __name__ == '__main__':
    CoinbaseFundingStreamsTest().main()

