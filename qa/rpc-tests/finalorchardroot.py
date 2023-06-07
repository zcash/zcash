#!/usr/bin/env python3
# Copyright (c) 2022 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .


from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    NU5_BRANCH_ID,
    assert_equal,
    connect_nodes_bi,
    get_coinbase_address,
    nuparams,
    start_nodes,
    wait_and_assert_operationid_status,
)

from decimal import Decimal

SPROUT_TREE_EMPTY_ROOT = "59d2cde5e65c1414c32ba54f0fe4bdb3d67618125286e6a191317917c812c6d7"
SAPLING_TREE_EMPTY_ROOT = "3e49b5f954aa9d3545bc6c37744661eea48d7c34e3000d82b7f0010c30f4c2fb"
ORCHARD_TREE_EMPTY_ROOT = "ae2935f1dfd8a24aed7c70df7de3a668eb7a49b1319880dde2bbd9031ae5d82f"
NULL_FIELD = "0000000000000000000000000000000000000000000000000000000000000000"

# Verify block header field 'hashFinalOrchardRoot' (returned in rpc as 'finalorchardroot')
# is updated when Orchard transactions with outputs (commitments) are mined into a block.
class FinalOrchardRootTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.cache_behavior = 'sprout'

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-minrelaytxfee=0',
            '-txindex', # Avoid JSONRPC error: No information available about transaction
            '-reindex', # Required due to enabling -txindex
            nuparams(NU5_BRANCH_ID, 200),
            '-debug',
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            '-experimentalfeatures',
            '-lightwalletd'
            ]] * self.num_nodes)
        connect_nodes_bi(self.nodes,0,1)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        # Verify genesis block doesn't contain the final orchard root field.
        blk = self.nodes[0].getblock("0")
        assert "finalorchardroot" not in blk
        assert "orchard" not in blk["trees"]
        treestate = self.nodes[0].z_gettreestate("0")
        assert_equal(treestate["height"], 0)
        assert_equal(treestate["hash"], self.nodes[0].getblockhash(0))

        assert_equal(treestate["sprout"]["commitments"]["finalRoot"], SPROUT_TREE_EMPTY_ROOT)
        assert_equal(treestate["sprout"]["commitments"]["finalState"], "000000")
        assert "skipHash" not in treestate["sprout"]

        assert_equal(treestate["sapling"]["commitments"]["finalRoot"], NULL_FIELD)
        # There is no sapling state tree yet, and trying to find it in an earlier
        # block won't succeed (we're at genesis block), so skipHash is absent.
        assert "finalState" not in treestate["sapling"]
        assert "skipHash" not in treestate["sapling"]

        assert_equal(treestate["orchard"]["commitments"]["finalRoot"], NULL_FIELD)
        # There is no orchard state tree yet, and trying to find it in an earlier
        # block won't succeed (we're at genesis block), so skipHash is absent.
        assert "finalState" not in treestate["orchard"]
        assert "skipHash" not in treestate["orchard"]

        # Verify no generated blocks before NU5 contain the empty root of the Orchard tree.
        blockcount = self.nodes[0].getblockcount()
        for height in range(1, blockcount + 1):
            blk = self.nodes[0].getblock(str(height))
            assert "finalorchardroot" not in blk
            assert "orchard" not in blk["trees"]

            treestate = self.nodes[0].z_gettreestate(str(height))
            assert_equal(treestate["height"], height)
            assert_equal(treestate["hash"], self.nodes[0].getblockhash(height))

            if height < 100:
                assert "skipHash" not in treestate["sprout"]
                assert_equal(treestate["sprout"]["commitments"]["finalRoot"], SPROUT_TREE_EMPTY_ROOT)
                assert_equal(treestate["sprout"]["commitments"]["finalState"], "000000")

            assert "skipHash" not in treestate["sapling"]
            assert_equal(treestate["sapling"]["commitments"]["finalRoot"], SAPLING_TREE_EMPTY_ROOT)
            assert_equal(treestate["sapling"]["commitments"]["finalState"], "000000")

            assert "skipHash" not in treestate["orchard"]
            assert_equal(treestate["orchard"]["commitments"]["finalRoot"], NULL_FIELD)
            assert "finalState" not in treestate["orchard"]

        self.sync_all()
        self.nodes[0].generate(11)
        self.sync_all()

        # post-NU5

        for height in range(200, 211):
            blk = self.nodes[0].getblock(str(height))
            assert_equal(blk["finalorchardroot"], ORCHARD_TREE_EMPTY_ROOT)
            assert_equal(blk["trees"]["orchard"]["size"], 0)

            treestate = self.nodes[0].z_gettreestate(str(height))
            assert_equal(treestate["height"], height)
            assert_equal(treestate["hash"], self.nodes[0].getblockhash(height))

            if height < 100:
                assert "skipHash" not in treestate["sprout"]
                assert_equal(treestate["sprout"]["commitments"]["finalRoot"], SPROUT_TREE_EMPTY_ROOT)
                assert_equal(treestate["sprout"]["commitments"]["finalState"], "000000")

            assert "skipHash" not in treestate["sapling"]
            assert_equal(treestate["sapling"]["commitments"]["finalRoot"], SAPLING_TREE_EMPTY_ROOT)
            assert_equal(treestate["sapling"]["commitments"]["finalState"], "000000")

            assert "skipHash" not in treestate["orchard"]
            assert_equal(treestate["orchard"]["commitments"]["finalRoot"], ORCHARD_TREE_EMPTY_ROOT)
            assert_equal(treestate["orchard"]["commitments"]["finalState"], "000000")

        # Verify that there are no complete Orchard subtrees.
        subtrees = self.nodes[0].z_getsubtreesbyindex('orchard', 0)
        assert_equal(subtrees['pool'], 'orchard')
        assert_equal(subtrees['start_index'], 0)
        assert_equal(len(subtrees['subtrees']), 0)

        # Node 0 shields some funds
        taddr0 = get_coinbase_address(self.nodes[0])
        acct0 = self.nodes[0].z_getnewaccount()['account']
        addrRes0 = self.nodes[0].z_getaddressforaccount(acct0, ['orchard'])
        assert_equal(acct0, addrRes0['account'])
        assert_equal(addrRes0['receiver_types'], ['orchard'])
        orchardAddr0 = addrRes0['address']
        recipients = []
        recipients.append({"address": orchardAddr0, "amount": Decimal('10')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0, 'AllowRevealedSenders')
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify the final Orchard root has changed
        blk = self.nodes[0].getblock("211")
        root = blk["finalorchardroot"]
        assert root != ORCHARD_TREE_EMPTY_ROOT
        assert root != NULL_FIELD

        # Verify there is a Orchard output description (its commitment was added to tree)
        result = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(len(result["orchard"]["actions"]), 2)
        assert_equal(blk["trees"]["orchard"]["size"], 2)

        # Since there is a now orchard shielded input in the blockchain,
        # the orchard values should have changed
        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["orchard"]["commitments"]["finalRoot"], root)
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert new_treestate["orchard"]["commitments"]["finalRoot"] != treestate["orchard"]["commitments"]["finalRoot"]
        assert new_treestate["orchard"]["commitments"]["finalState"] != treestate["orchard"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalState"]), 196)
        treestate = new_treestate

        # Mine an empty block and verify the final Orchard root does not change
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        blk = self.nodes[0].getblock("212")
        assert_equal(root, blk["finalorchardroot"])
        assert_equal(blk["trees"]["orchard"]["size"], 2)

        # Mine a block with a transparent tx and verify the final Orchard root does not change
        taddr1 = self.nodes[1].getnewaddress()
        self.nodes[0].sendtoaddress(taddr1, Decimal("1.23"))

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(len(self.nodes[0].getblock("213")["tx"]), 2)
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal("1.23"))
        assert_equal(root, self.nodes[0].getblock("213")["finalorchardroot"])

        # Mine a block with a Sprout shielded tx and verify the final Orchard root does not change
        zaddr0 = self.nodes[0].listaddresses()[0]['sprout']['addresses'][0]
        assert_equal(self.nodes[0].z_getbalance(zaddr0), Decimal('50'))
        recipients = [{"address": taddr0, "amount": Decimal('12.34')}]
        opid = self.nodes[0].z_sendmany(zaddr0, recipients, 1, 0, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        blk = self.nodes[0].getblock("214")
        assert_equal(len(blk["tx"]), 2)
        assert_equal(self.nodes[0].z_getbalance(zaddr0), Decimal("37.66"))
        assert_equal(root, blk["finalorchardroot"])
        assert_equal(blk["trees"]["orchard"]["size"], 2)

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["orchard"]["commitments"]["finalRoot"], root)
        assert_equal(new_treestate["orchard"], treestate["orchard"])
        assert new_treestate["sprout"]["commitments"]["finalRoot"] != treestate["sprout"]["commitments"]["finalRoot"]
        assert new_treestate["sprout"]["commitments"]["finalState"] != treestate["sprout"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["sprout"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["sprout"]["commitments"]["finalState"]), 266)
        treestate = new_treestate

        # Mine a block with a Sapling shielded tx and verify the final Orchard root does not change
        saplingAddr1 = self.nodes[1].z_getnewaddress("sapling")
        recipients = [{"address": saplingAddr1, "amount": Decimal('2.34')}]
        myopid = self.nodes[0].z_sendmany(zaddr0, recipients, 1, 0, 'AllowRevealedAmounts')
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        blk = self.nodes[0].getblock("215")
        assert_equal(len(blk["tx"]), 2)
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal("2.34"))
        assert_equal(root, blk["finalorchardroot"])
        assert_equal(blk["trees"]["orchard"]["size"], 2)

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["orchard"]["commitments"]["finalRoot"], root)
        assert_equal(new_treestate["orchard"], treestate["orchard"])
        assert new_treestate["sapling"]["commitments"]["finalRoot"] != treestate["sapling"]["commitments"]["finalRoot"]
        assert new_treestate["sapling"]["commitments"]["finalState"] != treestate["sapling"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalState"]), 70)
        treestate = new_treestate

        # Mine a block with an Orchard shielded recipient and verify the final Orchard root changes
        acct1 = self.nodes[1].z_getnewaccount()['account']
        addrRes1 = self.nodes[1].z_getaddressforaccount(acct1, ['orchard'])
        assert_equal(acct1, addrRes1['account'])
        assert_equal(addrRes1['receiver_types'], ['orchard'])
        orchardAddr1 = addrRes1['address']
        recipients = [{"address": orchardAddr1, "amount": Decimal('2.34')}]
        myopid = self.nodes[0].z_sendmany(orchardAddr0, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        blk = self.nodes[0].getblock("216")
        assert_equal(len(blk["tx"]), 2)
        assert_equal(self.nodes[1].z_getbalance(orchardAddr1), Decimal("2.34"))
        assert root != blk["finalorchardroot"]

        # Verify there is a Orchard output description (its commitment was added to tree)
        result = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(len(result["orchard"]["actions"]), 2)  # there is Orchard shielded change
        assert_equal(blk["trees"]["orchard"]["size"], 4)

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert_equal(new_treestate["sapling"], treestate["sapling"])
        assert new_treestate["orchard"]["commitments"]["finalRoot"] != treestate["orchard"]["commitments"]["finalRoot"]
        assert new_treestate["orchard"]["commitments"]["finalState"] != treestate["orchard"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalState"]), 260)
        treestate = new_treestate

        # Mine a block with an Orchard shielded sender and transparent recipient and verify the final Orchard root changes (because actions)
        taddr2 = self.nodes[0].getnewaddress()
        recipients = []
        recipients.append({"address": taddr2, "amount": Decimal('2.34')})
        myopid = self.nodes[1].z_sendmany(orchardAddr1, recipients, 1, 0, 'AllowRevealedRecipients')
        mytxid = wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        blk = self.nodes[0].getblock("217")
        assert_equal(len(blk["tx"]), 2)
        assert_equal(self.nodes[0].z_getbalance(taddr2), Decimal("2.34"))
        assert root != blk["finalorchardroot"]

        # Verify there is a Orchard output description (its commitment was added to tree)
        result = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(len(result["orchard"]["actions"]), 2)  # there is Orchard shielded change
        assert_equal(blk["trees"]["orchard"]["size"], 6)

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert_equal(new_treestate["sapling"], treestate["sapling"])
        assert new_treestate["orchard"]["commitments"]["finalRoot"] != treestate["orchard"]["commitments"]["finalRoot"]
        assert new_treestate["orchard"]["commitments"]["finalState"] != treestate["orchard"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["orchard"]["commitments"]["finalState"]), 260)

        # Verify that there are still no complete subtrees (as we have not created 2^16 notes).
        subtrees = self.nodes[0].z_getsubtreesbyindex('orchard', 0)
        assert_equal(subtrees['pool'], 'orchard')
        assert_equal(subtrees['start_index'], 0)
        assert_equal(len(subtrees['subtrees']), 0)


if __name__ == '__main__':
    FinalOrchardRootTest().main()
