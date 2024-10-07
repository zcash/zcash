#!/usr/bin/env python3
# Copyright (c) 2020-2024 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    assert_greater_than,
    assert_raises_message,
    connect_nodes_bi,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)
from test_framework.authproxy import JSONRPCException
from test_framework.mininode import COIN
from test_framework.zip317 import conventional_fee, ZIP_317_FEE

from decimal import Decimal

# Test wallet address behaviour across network upgrades
class WalletZSendmanyTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.cache_behavior = 'sprout'

    def setup_network(self, split=False):
        self.nodes = start_nodes(3, self.options.tmpdir, extra_args=[[
            nuparams(NU5_BRANCH_ID, 238),
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            '-allowdeprecated=z_gettotalbalance',
        ]] * self.num_nodes)
        connect_nodes_bi(self.nodes,0,1)
        connect_nodes_bi(self.nodes,1,2)
        connect_nodes_bi(self.nodes,0,2)
        self.is_network_split=False
        self.sync_all()

    # Check that an account has expected balances in only the expected pools.
    # Remember that empty pools are omitted from the output.
    def _check_balance_for_rpc(self, rpcmethod, node, account, expected, minconf):
        rpc = getattr(self.nodes[node], rpcmethod)
        actual = rpc(account, minconf)
        assert_equal(set(expected), set(actual['pools']))
        total_balance = 0
        for pool in expected:
            assert_equal(expected[pool] * COIN, actual['pools'][pool]['valueZat'])
            total_balance += expected[pool]
        assert_equal(actual['minimum_confirmations'], minconf)
        return total_balance

    # Check that an account has expected balances in only the expected pools, and that
    # they are held only in `address`.
    def check_balance(self, node, account, address, expected, minconf=1):
        acct_balance = self._check_balance_for_rpc('z_getbalanceforaccount', node, account, expected, minconf)
        z_getbalance = self.nodes[node].z_getbalance(address, minconf)
        assert_equal(acct_balance, z_getbalance)
        fvk = self.nodes[node].z_exportviewingkey(address)
        self._check_balance_for_rpc('z_getbalanceforviewingkey', node, fvk, expected, minconf)

    def run_test(self):
        # z_sendmany is expected to fail if tx size breaks limit
        n0sapling = self.nodes[0].z_getnewaddress()

        recipients = []
        num_t_recipients = 1000
        num_z_recipients = 2100
        amount_per_recipient = Decimal('0.00000001')
        errorString = ''
        for i in range(0, num_t_recipients):
            newtaddr = self.nodes[2].getnewaddress()
            recipients.append({"address": newtaddr, "amount": amount_per_recipient})
        for i in range(0, num_z_recipients):
            newzaddr = self.nodes[2].z_getnewaddress()
            recipients.append({"address": newzaddr, "amount": amount_per_recipient})

        try:
            self.nodes[0].z_sendmany(n0sapling, recipients)
        except JSONRPCException as e:
            errorString = e.error['message']
        assert("size of raw transaction would be larger than limit" in errorString)

        # add zaddr to node 2
        n2saddr = self.nodes[2].z_getnewaddress()

        # add taddr to node 2
        n2taddr = self.nodes[2].getnewaddress()

        # send from node 0 to node 2 taddr
        mytxid = self.nodes[0].sendtoaddress(n2taddr, Decimal('10'))

        self.sync_all()
        self.nodes[0].generate(10)
        self.sync_all()

        # send node 2 taddr to zaddr
        zsendmanynotevalue = Decimal('7.0')
        fee = conventional_fee(3)
        recipients = [{"address": n2saddr, "amount": zsendmanynotevalue}]
        opid = self.nodes[2].z_sendmany(n2taddr, recipients, 1, fee, 'AllowFullyTransparent')
        mytxid = wait_and_assert_operationid_status(self.nodes[2], opid)

        self.sync_all()
        n2sprout_balance = Decimal('50.00000000')

        # check shielded balance status with getwalletinfo
        wallet_info = self.nodes[2].getwalletinfo()
        assert_equal(Decimal(wallet_info["shielded_unconfirmed_balance"]), zsendmanynotevalue)
        assert_equal(Decimal(wallet_info["shielded_balance"]), n2sprout_balance)

        self.nodes[2].generate(10)
        self.sync_all()

        n0t_balance = self.nodes[0].getbalance()
        n2t_balance = Decimal('210.00000000') - zsendmanynotevalue - fee
        assert_equal(Decimal(self.nodes[2].getbalance()), n2t_balance)
        assert_equal(Decimal(self.nodes[2].getbalance("*")), n2t_balance)

        # check zaddr balance with z_getbalance
        n2saddr_balance = zsendmanynotevalue
        assert_equal(self.nodes[2].z_getbalance(n2saddr), n2saddr_balance)

        # check via z_gettotalbalance
        resp = self.nodes[2].z_gettotalbalance()
        assert_equal(Decimal(resp["transparent"]), n2t_balance)
        assert_equal(Decimal(resp["private"]), n2sprout_balance + n2saddr_balance)
        assert_equal(Decimal(resp["total"]), n2t_balance + n2sprout_balance + n2saddr_balance)

        # check confirmed shielded balance with getwalletinfo
        wallet_info = self.nodes[2].getwalletinfo()
        assert_equal(Decimal(wallet_info["shielded_unconfirmed_balance"]), Decimal('0.0'))
        assert_equal(Decimal(wallet_info["shielded_balance"]), n2sprout_balance + n2saddr_balance)

        # there should be at least one Sapling output
        mytxdetails = self.nodes[2].getrawtransaction(mytxid, 1)
        assert_greater_than(len(mytxdetails["vShieldedOutput"]), 0)
        # the Sapling output should take in all the public value
        assert_equal(mytxdetails["valueBalance"], -zsendmanynotevalue)

        # try sending with a memo to a taddr, which should fail
        recipients = [{"address": self.nodes[0].getnewaddress(), "amount": Decimal('1'), "memo": "DEADBEEF"}]
        opid = self.nodes[2].z_sendmany(n2saddr, recipients, 1, ZIP_317_FEE, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', 'Failed to build transaction: Memos cannot be sent to transparent addresses.')

        fee = conventional_fee(4)
        recipients = [
            {"address": self.nodes[0].getnewaddress(), "amount": Decimal('1')},
            {"address": self.nodes[2].getnewaddress(), "amount": Decimal('1')},
        ];

        opid = self.nodes[2].z_sendmany(n2saddr, recipients, 1, fee, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n2saddr_balance -= Decimal('2') + fee
        n0t_balance += Decimal('1')
        n2t_balance += Decimal('1')

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        n0t_balance += Decimal('10') # newly mature

        assert_equal(Decimal(self.nodes[0].getbalance()), n0t_balance)
        assert_equal(Decimal(self.nodes[0].getbalance("*")), n0t_balance)
        assert_equal(Decimal(self.nodes[2].getbalance()), n2t_balance)
        assert_equal(Decimal(self.nodes[2].getbalance("*")), n2t_balance)
        assert_equal(self.nodes[2].z_getbalance(n2saddr), n2saddr_balance)

        # Get a new unified account on node 0 & generate a UA
        n0account = self.nodes[0].z_getnewaccount()['account']
        n0ua0 = self.nodes[0].z_getaddressforaccount(n0account)['address']
        n0ua0_balance = Decimal('0')
        self.check_balance(0, 0, n0ua0, {})

        # Prepare to fund the UA from coinbase
        source = get_coinbase_address(self.nodes[2])
        fee = conventional_fee(3)
        recipients = [{"address": n0ua0, "amount": Decimal('10') - fee}]

        # If we attempt to spend with the default privacy policy, z_sendmany
        # fails because it needs to spend transparent coins in a transaction
        # involving a Unified Address.
        unified_address_msg = 'Could not send to a shielded receiver of a unified address without spending funds from a different pool, which would reveal transaction amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` or weaker if you wish to allow this transaction to proceed anyway.'
        revealed_senders_msg = 'Insufficient funds: have 0.00, need 10.00; note that coinbase outputs will not be selected if you specify ANY_TADDR, any transparent recipients are included, or if the `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker.'
        opid = self.nodes[2].z_sendmany(source, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', unified_address_msg)

        # We can't create a transaction with an unknown privacy policy.
        assert_raises_message(
            JSONRPCException,
            'Unknown privacy policy name \'ZcashIsAwesome\'',
            self.nodes[2].z_sendmany,
            source, recipients, 1, fee, 'ZcashIsAwesome')

        # If we set any policy that does not include AllowRevealedSenders,
        # z_sendmany also fails.
        for (policy, msg) in [
            ('FullPrivacy', unified_address_msg),
            ('AllowRevealedAmounts', revealed_senders_msg),
            ('AllowRevealedRecipients', revealed_senders_msg),
        ]:
            opid = self.nodes[2].z_sendmany(source, recipients, 1, fee, policy)
            wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', msg)

        # By setting the correct policy, we can create the transaction.
        opid = self.nodes[2].z_sendmany(source, recipients, 1, fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n2t_balance -= Decimal('10.0')
        n0ua0_balance += Decimal('10') - fee

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()
        n0t_balance += Decimal('10.0') # newly mature

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(Decimal(self.nodes[0].getbalance()), n0t_balance)
        assert_equal(Decimal(self.nodes[2].getbalance()), n2t_balance)

        # Send some funds to a specific legacy taddr that we can spend from
        fee = conventional_fee(3)
        recipients = [{"address": n2taddr, "amount": Decimal('5')}]

        # If we attempt to spend with the default privacy policy, z_sendmany
        # returns an error because it needs to create a transparent recipient in
        # a transaction involving a Unified Address.
        revealed_recipients_msg = "This transaction would have transparent recipients, which is not enabled by default because it will publicly reveal transaction recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedRecipients` or weaker if you wish to allow this transaction to proceed anyway."
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[0], opid, 'failed', revealed_recipients_msg)

        # If we set any policy that does not include AllowRevealedRecipients,
        # z_sendmany also returns an error.
        for policy in [
            'FullPrivacy',
            'AllowRevealedAmounts',
            'AllowRevealedSenders',
            'AllowLinkingAccountAddresses',
        ]:
            opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee, policy)
            wait_and_assert_operationid_status(self.nodes[0], opid, 'failed', revealed_recipients_msg)

        # By setting the correct policy, we can create the transaction.
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[0], opid)
        n2t_balance += Decimal('5')
        n0ua0_balance -= Decimal('5') + fee

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(Decimal(self.nodes[2].getbalance()), n2t_balance)

        # Send some funds to a legacy sapling address that we can spend from
        fee = conventional_fee(2)
        recipients = [{"address": n2saddr, "amount": Decimal('3')}]
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[0], opid)
        n2saddr_balance += Decimal('3')
        n0ua0_balance -= Decimal('3') + fee

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(self.nodes[2].z_getbalance(n2saddr), n2saddr_balance)

        # Send funds back from the legacy taddr to the UA. This requires
        # AllowRevealedSenders, but we can also use any weaker policy that
        # includes it.
        fee = conventional_fee(3)
        recipients = [{"address": n0ua0, "amount": Decimal('4')}]
        opid = self.nodes[2].z_sendmany(n2taddr, recipients, 1, fee, 'AllowFullyTransparent')
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n0ua0_balance += Decimal('4')
        n2t_balance -= Decimal('4') + fee

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(Decimal(self.nodes[2].getbalance()), n2t_balance)

        # Send funds back from the legacy zaddr to the UA
        fee = conventional_fee(2)
        recipients = [{"address": n0ua0, "amount": Decimal('2')}]
        opid = self.nodes[2].z_sendmany(n2saddr, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n0ua0_balance += Decimal('2')
        n2saddr_balance -= Decimal('2') + fee

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(self.nodes[2].z_getbalance(n2saddr), n2saddr_balance)

        #
        # Test that z_sendmany avoids UA linkability unless we allow it.
        #

        # Generate a new account with two new addresses.
        n1account = self.nodes[1].z_getnewaccount()['account']
        n1ua0 = self.nodes[1].z_getaddressforaccount(n1account)['address']
        n1ua1 = self.nodes[1].z_getaddressforaccount(n1account)['address']

        # Send funds to the transparent receivers of both addresses.
        for ua in [n1ua0, n1ua1]:
            taddr = self.nodes[1].z_listunifiedreceivers(ua)['p2pkh']
            self.nodes[0].sendtoaddress(taddr, 2)

        n0sapling_balance = n0ua0_balance
        n1ua0_balance = Decimal('2')
        n1ua1_balance = Decimal('2')
        n1t_balance = n1ua0_balance + n1ua1_balance

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0sapling_balance})

        # The account should see all funds.
        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'transparent': {'valueZat': n1t_balance * COIN},
        })

        # The addresses should see only the transparent funds sent to them.
        assert_equal(Decimal(self.nodes[1].z_getbalance(n1ua0)), n1ua0_balance)
        assert_equal(Decimal(self.nodes[1].z_getbalance(n1ua1)), n1ua1_balance)

        # If we try to send 3 ZEC less fee from n1ua0, it will fail with insufficient funds.
        fee = conventional_fee(4)
        amount = Decimal('3') - fee
        recipients = [{"address": n0ua0, "amount": amount}]
        linked_addrs_with_coinbase_note_msg = 'Insufficient funds: have 0.00, need 3.00; note that coinbase outputs will not be selected if you specify ANY_TADDR, any transparent recipients are included, or if the `privacyPolicy` parameter is not set to `AllowRevealedSenders` or weaker. (This transaction may require selecting transparent coins that were sent to multiple addresses, which is not enabled by default because it would create a public link between those addresses. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowLinkingAccountAddresses` or weaker if you wish to allow this transaction to proceed anyway.)'
        linked_addrs_without_coinbase_note_msg = 'Insufficient funds: have 2.00, need 3.00. (This transaction may require selecting transparent coins that were sent to multiple addresses, which is not enabled by default because it would create a public link between those addresses. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowLinkingAccountAddresses` or weaker if you wish to allow this transaction to proceed anyway.)'
        revealed_amounts_msg = 'Could not send to a shielded receiver of a unified address without spending funds from a different pool, which would reveal transaction amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` or weaker if you wish to allow this transaction to proceed anyway.'
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', revealed_amounts_msg)

        # If we try it again with any policy that is too strong, it also fails.
        for (policy, msg) in [
            ('FullPrivacy', revealed_amounts_msg),
            ('AllowRevealedAmounts', linked_addrs_with_coinbase_note_msg),
            ('AllowRevealedRecipients', linked_addrs_with_coinbase_note_msg),
            ('AllowRevealedSenders', linked_addrs_without_coinbase_note_msg),
            ('AllowFullyTransparent', linked_addrs_without_coinbase_note_msg),
        ]:
            opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, policy)
            wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', msg)

        # If we try to send just a bit less than we have, it will fail, complaining about dust
        opid = self.nodes[1].z_sendmany(n1ua0,
                                        [{"address": n0ua0, "amount": Decimal('3.9999999') - fee}],
                                        1, fee, 'AllowLinkingAccountAddresses')
        wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', 'Insufficient funds: have 4.00, need 0.00000044 more to avoid creating invalid change output 0.0000001 (dust threshold is 0.00000054).')

        # Once we provide a sufficiently weak policy, the transaction succeeds.
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, 'AllowLinkingAccountAddresses')
        wait_and_assert_operationid_status(self.nodes[1], opid)
        n0ua0_balance += amount
        # Change should be sent to the Sapling change address (because NU5 is not active).
        n1sapling_balance = n1t_balance - amount - fee
        del n0sapling_balance
        del n1t_balance

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        self.check_balance(0, 0, n0ua0, {'sapling': n0ua0_balance})
        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'sapling': {'valueZat': n1sapling_balance * COIN},
        })

        # z_getbalance behaves inconsistently between transparent and shielded
        # addresses: for a shielded address it gives the account balance.
        assert_equal(Decimal(self.nodes[1].z_getbalance(n1ua0)), n1sapling_balance)
        assert_equal(Decimal(self.nodes[1].z_getbalance(n1ua1)), n1sapling_balance)

        #
        # Test Orchard-only UA before NU5
        #

        n0orchard_only = self.nodes[0].z_getaddressforaccount(n0account, ["orchard"])['address']
        recipients = [{"address": n0orchard_only, "amount": Decimal('1')}]
        for (policy, msg) in [
            ('FullPrivacy', 'Could not send to a shielded receiver of a unified address without spending funds from a different pool, which would reveal transaction amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` or weaker if you wish to allow this transaction to proceed anyway.'),
            ('AllowRevealedAmounts', 'This transaction would send to a transparent receiver of a unified address, which is not enabled by default because it will publicly reveal transaction recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedRecipients` or weaker if you wish to allow this transaction to proceed anyway.'),
            ('AllowRevealedRecipients', 'Could not send to an Orchard-only receiver despite a lax privacy policy, because NU5 has not been activated yet.'),
        ]:
            opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, policy)
            wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', msg)

        #
        # Test NoPrivacy policy
        #

        # Send some legacy transparent funds to n1ua0, creating Sapling outputs.
        source = get_coinbase_address(self.nodes[2])
        fee = conventional_fee(3)
        recipients = [{"address": n1ua0, "amount": Decimal('10') - fee}]
        # This requires the AllowRevealedSenders policy, but we specify only AllowRevealedAmounts...
        opid = self.nodes[2].z_sendmany(source, recipients, 1, fee, 'AllowRevealedAmounts')
        wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', revealed_senders_msg)
        # ... which we can always override with the NoPrivacy policy.
        opid = self.nodes[2].z_sendmany(source, recipients, 1, fee, 'NoPrivacy')
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n1sapling_balance += Decimal('10') - fee

        self.sync_all()
        self.nodes[2].generate(1)
        self.sync_all()

        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'sapling': {'valueZat': n1sapling_balance * COIN},
        })

        # Send some funds from node 1's account to a transparent address.
        fee = conventional_fee(3)
        recipients = [{"address": n2taddr, "amount": Decimal('5')}]
        # This requires the AllowRevealedRecipients policy...
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', revealed_recipients_msg)
        # ... which we can always override with the NoPrivacy policy.
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, 'NoPrivacy')
        wait_and_assert_operationid_status(self.nodes[1], opid)
        n1sapling_balance -= Decimal('5') + fee

        # Activate NU5

        self.sync_all()
        self.nodes[1].generate(10)
        self.sync_all()

        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'sapling': {'valueZat': n1sapling_balance * COIN},
        })

        #
        # Test sending Sprout funds to Orchard-only UA
        #

        sproutAddr = self.nodes[2].listaddresses()[0]['sprout']['addresses'][0]
        recipients = [{"address": n0orchard_only, "amount": Decimal('100')}]
        for (policy, msg) in [
            ('FullPrivacy', 'Could not send to a shielded receiver of a unified address without spending funds from a different pool, which would reveal transaction amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` or weaker if you wish to allow this transaction to proceed anyway.'),
            ('AllowRevealedAmounts', 'This transaction would send to a transparent receiver of a unified address, which is not enabled by default because it will publicly reveal transaction recipients and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedRecipients` or weaker if you wish to allow this transaction to proceed anyway.'),
            ('AllowRevealedRecipients', 'Could not send to an Orchard-only receiver despite a lax privacy policy, because you are sending from the Sprout pool and there is no transaction version that supports both Sprout and Orchard.'),
            ('NoPrivacy', 'Could not send to an Orchard-only receiver despite a lax privacy policy, because you are sending from the Sprout pool and there is no transaction version that supports both Sprout and Orchard.'),
        ]:
            opid = self.nodes[2].z_sendmany(sproutAddr, recipients, 1, ZIP_317_FEE, policy)
            wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', msg)

        #
        # Test AllowRevealedAmounts policy
        #

        # Sending some funds to the Orchard pool in n0account ...
        n0ua1 = self.nodes[0].z_getaddressforaccount(n0account, ["orchard"])['address']
        fee = conventional_fee(4)
        recipients = [{"address": n0ua1, "amount": Decimal('5')}]

        # Should fail under default and 'FullPrivacy' policies ...
        revealed_amounts_msg = 'Could not send to a shielded receiver of a unified address without spending funds from a different pool, which would reveal transaction amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedAmounts` or weaker if you wish to allow this transaction to proceed anyway.'
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', revealed_amounts_msg)

        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, 'FullPrivacy')
        wait_and_assert_operationid_status(self.nodes[1], opid, 'failed', revealed_amounts_msg)

        # Should succeed under 'AllowRevealedAmounts'. The change will go to Orchard.
        opid = self.nodes[1].z_sendmany(n1ua0, recipients, 1, fee, 'AllowRevealedAmounts')
        wait_and_assert_operationid_status(self.nodes[1], opid)
        n0sapling_balance = n0ua0_balance
        n0orchard_balance = Decimal('5')
        n1orchard_balance = n1sapling_balance - Decimal('5') - fee
        del n0ua0_balance
        del n1sapling_balance

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalanceforaccount(n0account)['pools'], {
            'sapling': {'valueZat': n0sapling_balance * COIN},
            'orchard': {'valueZat': n0orchard_balance * COIN},
        })
        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'orchard': {'valueZat': n1orchard_balance * COIN},
        })

        # A total that requires selecting from both pools should fail under default and
        # FullPrivacy policies...

        fee = conventional_fee(3)
        recipients = [{"address": n1ua0, "amount": Decimal('15')}]
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee)
        wait_and_assert_operationid_status(self.nodes[0], opid, 'failed', revealed_amounts_msg)

        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee, 'FullPrivacy')
        wait_and_assert_operationid_status(self.nodes[0], opid, 'failed', revealed_amounts_msg)

        # Should succeed under 'AllowRevealedAmounts'
        # All funds should be received to the Orchard pool, and all change should
        # be optimistically shielded.
        fee = conventional_fee(6)
        opid = self.nodes[0].z_sendmany(n0ua0, recipients, 1, fee, 'AllowRevealedAmounts')
        wait_and_assert_operationid_status(self.nodes[0], opid)
        n0orchard_balance += n0sapling_balance - Decimal('15') - fee
        n1orchard_balance += Decimal('15')
        del n0sapling_balance

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalanceforaccount(n0account)['pools'], {
            'orchard': {'valueZat': n0orchard_balance * COIN},
        })
        assert_equal(self.nodes[1].z_getbalanceforaccount(n1account)['pools'], {
            'orchard': {'valueZat': n1orchard_balance * COIN},
        })

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        #
        # Test transparent change
        #

        fee = conventional_fee(3)
        recipients = [{"address": n0ua1, "amount": Decimal('4')}]
        # Should fail because this generates transparent change, but we don’t have
        # `AllowRevealedRecipients`
        opid = self.nodes[2].z_sendmany(n2taddr, recipients, 1, fee, 'AllowRevealedSenders')
        wait_and_assert_operationid_status(self.nodes[2], opid, 'failed', "This transaction would have transparent change, which is not enabled by default because it will publicly reveal the change address and amounts. THIS MAY AFFECT YOUR PRIVACY. Resubmit with the `privacyPolicy` parameter set to `AllowRevealedRecipients` or weaker if you wish to allow this transaction to proceed anyway.")

        # Should succeed once we include `AllowRevealedRecipients`
        opid = self.nodes[2].z_sendmany(n2taddr, recipients, 1, fee, 'AllowFullyTransparent')
        wait_and_assert_operationid_status(self.nodes[2], opid)
        n0orchard_balance += Decimal('4')

        self.sync_all()
        self.nodes[1].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalanceforaccount(n0account)['pools'], {
            'orchard': {'valueZat': n0orchard_balance * COIN},
        })

if __name__ == '__main__':
    WalletZSendmanyTest().main()
