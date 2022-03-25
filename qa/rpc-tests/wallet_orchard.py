#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

# Test wallet behaviour with the Orchard protocol
class WalletOrchardTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.num_nodes = 4

    def setup_nodes(self):
        return start_nodes(self.num_nodes, self.options.tmpdir, [[
            '-experimentalfeatures',
            '-orchardwallet',
            nuparams(NU5_BRANCH_ID, 210),
        ]] * self.num_nodes)

    def run_test(self):
        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)

        # Get a new orchard-only unified address
        acct1 = self.nodes[1].z_getnewaccount()['account']
        addrRes1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])
        assert_equal(acct1, addrRes1['account'])
        assert_equal(addrRes1['receiver_types'], ['orchard'])
        ua1 = addrRes1['address']

        # Verify that we have only an Orchard component
        receiver_types = self.nodes[0].z_listunifiedreceivers(ua1)
        assert_equal(set(['orchard']), set(receiver_types))

        # Verify balance
        assert_equal({'pools': {}, 'minimum_confirmations': 1}, self.nodes[1].z_getbalanceforaccount(acct1))

        # Send some sapling funds to node 2 for later spending after we split the network
        acct2 = self.nodes[2].z_getnewaccount()['account']
        addrRes2 = self.nodes[2].z_getaddressforaccount(acct2, ['sapling', 'orchard'])
        assert_equal(acct2, addrRes2['account'])
        ua2 = addrRes2['address']
        saplingAddr2 = self.nodes[2].z_listunifiedreceivers(ua2)['sapling']

        recipients = [{"address": saplingAddr2, "amount": Decimal('10')}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the tx & activate NU5
        self.sync_all()
        self.nodes[0].generate(10)
        self.sync_all()

        # Check the value sent to saplingAddr2 was received in node 2's account
        assert_equal(
                {'pools': {'sapling': {'valueZat': Decimal('1000000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        # Node 0 shields some funds
        # t-coinbase -> Orchard
        recipients = [{"address": ua1, "amount": Decimal('10')}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('1000000000')}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # Split the network
        self.split_network()

        # Send another tx to ua1
        recipients = [{"address": ua1, "amount": Decimal('10')}]
        myopid = self.nodes[0].z_sendmany(get_coinbase_address(self.nodes[0]), recipients, 1, 0, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        # Mine the tx & generate a majority chain on the 0/1 side of the split
        self.sync_all()
        self.nodes[0].generate(10)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('2000000000')}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # On the other side of the split, send some funds to node 3
        acct3 = self.nodes[3].z_getnewaccount()['account']
        addrRes3 = self.nodes[3].z_getaddressforaccount(acct3, ['sapling', 'orchard'])
        assert_equal(acct3, addrRes3['account'])
        ua3 = addrRes3['address']

        recipients = [{"address": ua3, "amount": Decimal('1')}]
        myopid = self.nodes[2].z_sendmany(ua2, recipients, 1, 0)
        rollback_tx = wait_and_assert_operationid_status(self.nodes[2], myopid)

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        # The remaining change from ua2's Sapling note has been sent to the
        # account's internal Orchard change address.
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('900000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('100000000')}}, 'minimum_confirmations': 1},
                self.nodes[3].z_getbalanceforaccount(acct3))

        # Check that the mempools are empty
        for i in range(self.num_nodes):
            assert_equal(set([]), set(self.nodes[i].getrawmempool()))

        # Reconnect the nodes; nodes 2 and 3 will re-org to node 0's chain.
        print("Re-joining the network so that nodes 2 and 3 reorg")
        self.join_network()

        # split 0/1's chain should have won, so their wallet balance should be consistent
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('2000000000')}}, 'minimum_confirmations': 1},
                self.nodes[1].z_getbalanceforaccount(acct1))

        # split 2/3's chain should have been rolled back, so their txn should have been
        # un-mined and returned to the mempool
        assert_equal(set([rollback_tx]), set(self.nodes[2].getrawmempool()))

        # acct2's sole Orchard note is spent by a transaction in the mempool, so our
        # confirmed balance is currently 0
        assert_equal(
                {'pools': {}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        # acct2's incoming change (unconfirmed, still in the mempool) is 9 zec
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('900000000')}}, 'minimum_confirmations': 0},
                self.nodes[2].z_getbalanceforaccount(acct2, 0))

        # The transaction was un-mined, so acct3 should have no confirmed balance
        assert_equal(
                {'pools': {}, 'minimum_confirmations': 1},
                self.nodes[3].z_getbalanceforaccount(acct3))

        # acct3's unconfirmed balance is 1 zec
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('100000000')}}, 'minimum_confirmations': 0},
                self.nodes[3].z_getbalanceforaccount(acct3, 0))

        # Manually resend the transaction in node 2's mempool
        self.nodes[2].resendwallettransactions()

        # Sync the network
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The un-mined transaction should now have been re-mined
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('900000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('100000000')}}, 'minimum_confirmations': 1},
                self.nodes[3].z_getbalanceforaccount(acct3))

        # Split the network again
        self.split_network()

        # Spend some of acct3's funds on the 2/3 side of the split
        recipients = [{"address": ua2, "amount": Decimal('0.5')}]
        myopid = self.nodes[3].z_sendmany(ua3, recipients, 1, 0)
        rollback_tx = wait_and_assert_operationid_status(self.nodes[3], myopid)

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('950000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        # Generate a majority chain on the 0/1 side of the split, then 
        # re-join the network.
        self.nodes[1].generate(10)
        self.join_network()

        # split 2/3's chain should have been rolled back, so their txn should have been
        # un-mined and returned to the mempool
        assert_equal(set([rollback_tx]), set(self.nodes[3].getrawmempool()))

        # acct2's balance is back to not contain the Orchard->Orchard value
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('900000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

        # acct3's sole Orchard note is spent by a transaction in the mempool, so our
        # confirmed balance is currently 0
        assert_equal(
                {'pools': {}, 'minimum_confirmations': 1},
                self.nodes[3].z_getbalanceforaccount(acct3))

        # Manually resend the transaction in node 3's mempool
        self.nodes[2].resendwallettransactions()

        # Sync the network
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # The un-mined transaction should now have been re-mined
        assert_equal(
                {'pools': {'orchard': {'valueZat': Decimal('950000000')}}, 'minimum_confirmations': 1},
                self.nodes[2].z_getbalanceforaccount(acct2))

if __name__ == '__main__':
    WalletOrchardTest().main()
