#!/usr/bin/env python2
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet behaviour with Sapling addresses
class WalletSaplingTest(BitcoinTestFramework):

    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:201', # Overwinter
            '-nuparams=76b809bb:203', # Sapling
            '-experimentalfeatures', '-zmergetoaddress',
        ]] * 4)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Activate Overwinter
        self.nodes[2].generate(1)
        self.sync_all()

        # Verify RPCs disallow Sapling value transfer if Sapling is not active
        tmp_taddr = self.nodes[3].getnewaddress()
        tmp_zaddr = self.nodes[3].z_getnewaddress('sapling')
        try:
            recipients = []
            recipients.append({"address": tmp_zaddr, "amount": Decimal('20')})
            self.nodes[3].z_sendmany(tmp_taddr, recipients, 1, 0)
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Invalid parameter, Sapling has not activated", e.error['message'])
        try:
            recipients = []
            recipients.append({"address": tmp_taddr, "amount": Decimal('20')})
            self.nodes[3].z_sendmany(tmp_zaddr, recipients, 1, 0)
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Invalid parameter, Sapling has not activated", e.error['message'])
        try:
            self.nodes[3].z_shieldcoinbase(tmp_taddr, tmp_zaddr)
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Invalid parameter, Sapling has not activated", e.error['message'])

        # Verify z_mergetoaddress RPC does not support Sapling yet
        try:
            self.nodes[3].z_mergetoaddress([tmp_taddr], tmp_zaddr)
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Invalid parameter, Sapling is not supported yet by z_mergetoadress", e.error['message'])
        try:
            self.nodes[3].z_mergetoaddress([tmp_zaddr], tmp_taddr)
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Invalid parameter, Sapling is not supported yet by z_mergetoadress", e.error['message'])

        # Activate Sapling
        self.nodes[2].generate(2)
        self.sync_all()

        taddr0 = self.nodes[0].getnewaddress()
        # Skip over the address containing node 1's coinbase
        self.nodes[1].getnewaddress()
        taddr1 = self.nodes[1].getnewaddress()
        saplingAddr0 = self.nodes[0].z_getnewaddress('sapling')
        saplingAddr1 = self.nodes[1].z_getnewaddress('sapling')

        # Verify addresses
        assert(saplingAddr0 in self.nodes[0].z_listaddresses())
        assert(saplingAddr1 in self.nodes[1].z_listaddresses())
        assert_equal(self.nodes[0].z_validateaddress(saplingAddr0)['type'], 'sapling')
        assert_equal(self.nodes[0].z_validateaddress(saplingAddr1)['type'], 'sapling')

        # Verify balance
        assert_equal(self.nodes[0].z_getbalance(saplingAddr0), Decimal('0'))
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal('0'))
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('0'))

        # Node 0 shields some funds
        # taddr -> Sapling
        #       -> taddr (change)
        recipients = []
        recipients.append({"address": saplingAddr0, "amount": Decimal('20')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()

        # Verify priority of tx is MAX_PRIORITY, defined as 1E+16 (10000000000000000)
        mempool = self.nodes[0].getrawmempool(True)
        assert(Decimal(mempool[mytxid]['startingpriority']) == Decimal('1E+16'))

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].z_getbalance(saplingAddr0), Decimal('20'))
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal('0'))
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('0'))

        # Node 0 sends some shielded funds to node 1
        # Sapling -> Sapling
        #         -> Sapling (change)
        recipients = []
        recipients.append({"address": saplingAddr1, "amount": Decimal('15')})
        myopid = self.nodes[0].z_sendmany(saplingAddr0, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()

        # Verify priority of tx is MAX_PRIORITY, defined as 1E+16 (10000000000000000)
        mempool = self.nodes[0].getrawmempool(True)
        assert(Decimal(mempool[mytxid]['startingpriority']) == Decimal('1E+16'))

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].z_getbalance(saplingAddr0), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal('15'))
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('0'))

        # Node 1 sends some shielded funds to node 0, as well as unshielding
        # Sapling -> Sapling
        #         -> taddr
        #         -> Sapling (change)
        recipients = []
        recipients.append({"address": saplingAddr0, "amount": Decimal('5')})
        recipients.append({"address": taddr1, "amount": Decimal('5')})
        myopid = self.nodes[1].z_sendmany(saplingAddr1, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()

        # Verify priority of tx is MAX_PRIORITY, defined as 1E+16 (10000000000000000)
        mempool = self.nodes[1].getrawmempool(True)
        assert(Decimal(mempool[mytxid]['startingpriority']) == Decimal('1E+16'))

        self.nodes[2].generate(1)
        self.sync_all()

        # Verify balance
        assert_equal(self.nodes[0].z_getbalance(saplingAddr0), Decimal('10'))
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal('5'))
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal('5'))

        # Verify existence of Sapling related JSON fields
        resp = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(resp['valueBalance'], Decimal('5'))
        assert(len(resp['vShieldedSpend']) == 1)
        assert(len(resp['vShieldedOutput']) == 2)
        assert('bindingSig' in resp)
        shieldedSpend = resp['vShieldedSpend'][0]
        assert('cv' in shieldedSpend)
        assert('anchor' in shieldedSpend)
        assert('nullifier' in shieldedSpend)
        assert('rk' in shieldedSpend)
        assert('proof' in shieldedSpend)
        assert('spendAuthSig' in shieldedSpend)
        shieldedOutput = resp['vShieldedOutput'][0]
        assert('cv' in shieldedOutput)
        assert('cmu' in shieldedOutput)
        assert('ephemeralKey' in shieldedOutput)
        assert('encCiphertext' in shieldedOutput)
        assert('outCiphertext' in shieldedOutput)
        assert('proof' in shieldedOutput)

        # Verify importing a spending key will update the nullifiers and witnesses correctly
        sk0 = self.nodes[0].z_exportkey(saplingAddr0)
        self.nodes[2].z_importkey(sk0, "yes")
        assert_equal(self.nodes[2].z_getbalance(saplingAddr0), Decimal('10'))
        sk1 = self.nodes[1].z_exportkey(saplingAddr1)
        self.nodes[2].z_importkey(sk1, "yes")
        assert_equal(self.nodes[2].z_getbalance(saplingAddr1), Decimal('5'))

        # Make sure we get a useful error when trying to send to both sprout and sapling
        node4_sproutaddr = self.nodes[3].z_getnewaddress('sprout')
        node4_saplingaddr = self.nodes[3].z_getnewaddress('sapling')
        try:
            self.nodes[1].z_sendmany(
                taddr1,
                [{'address': node4_sproutaddr, 'amount': 2.5}, {'address': node4_saplingaddr, 'amount': 2.4999}],
                1, 0.0001
            )
            raise AssertionError("Should have thrown an exception")
        except JSONRPCException as e:
            assert_equal("Cannot send to both Sprout and Sapling addresses using z_sendmany", e.error['message'])

if __name__ == '__main__':
    WalletSaplingTest().main()
