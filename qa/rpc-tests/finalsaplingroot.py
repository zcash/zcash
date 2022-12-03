#!/usr/bin/env python3
# Copyright (c) 2018 The Zcash developers
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

# Verify block header field 'hashFinalSaplingRoot' (returned in rpc as 'finalsaplingroot')
# is updated when Sapling transactions with outputs (commitments) are mined into a block.
class FinalSaplingRootTest(BitcoinTestFramework):

    def __init__(self):
        super().__init__()
        self.num_nodes = 2
        self.cache_behavior = 'sprout'

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args=[[
            '-txindex', # Avoid JSONRPC error: No information available about transaction
            '-reindex', # Required due to enabling -txindex
            nuparams(NU5_BRANCH_ID, 210),
            '-debug',
            '-allowdeprecated=getnewaddress',
            '-allowdeprecated=z_getnewaddress',
            '-allowdeprecated=z_getbalance',
            ]] * self.num_nodes)
        connect_nodes_bi(self.nodes,0,1)
        self.is_network_split=False
        self.sync_all()

    def run_test(self):
        # Verify genesis block contains null field for what is now called the final sapling root field.
        blk = self.nodes[0].getblock("0")
        assert_equal(blk["finalsaplingroot"], NULL_FIELD)
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

        # Verify all generated blocks contain the empty root of the Sapling tree.
        blockcount = self.nodes[0].getblockcount()
        for height in range(1, blockcount + 1):
            blk = self.nodes[0].getblock(str(height))
            assert_equal(blk["finalsaplingroot"], SAPLING_TREE_EMPTY_ROOT)

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

        # Node 0 shields some funds
        taddr0 = get_coinbase_address(self.nodes[0])
        saplingAddr0 = self.nodes[0].z_getnewaddress('sapling')
        recipients = []
        recipients.append({"address": saplingAddr0, "amount": Decimal('10')})
        myopid = self.nodes[0].z_sendmany(taddr0, recipients, 1, 0, 'AllowRevealedSenders')
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        # Verify the final Sapling root has changed
        blk = self.nodes[0].getblock("201")
        root = blk["finalsaplingroot"]
        assert root is not SAPLING_TREE_EMPTY_ROOT
        assert root is not NULL_FIELD

        # Verify there is a Sapling output description (its commitment was added to tree)
        result = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(len(result["vShieldedOutput"]), 1)

        # Since there is a now sapling shielded input in the blockchain,
        # the sapling values should have changed
        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sapling"]["commitments"]["finalRoot"], root)
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert new_treestate["sapling"]["commitments"]["finalRoot"] != treestate["sapling"]["commitments"]["finalRoot"]
        assert new_treestate["sapling"]["commitments"]["finalState"] != treestate["sapling"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalState"]), 70)
        treestate = new_treestate

        # Mine an empty block and verify the final Sapling root does not change
        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()
        assert_equal(root, self.nodes[0].getblock("202")["finalsaplingroot"])

        # Mine a block with a transparent tx and verify the final Sapling root does not change
        taddr1 = self.nodes[1].getnewaddress()
        self.nodes[0].sendtoaddress(taddr1, Decimal("1.23"))

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(len(self.nodes[0].getblock("203")["tx"]), 2)
        assert_equal(self.nodes[1].z_getbalance(taddr1), Decimal("1.23"))
        assert_equal(root, self.nodes[0].getblock("203")["finalsaplingroot"])

        # Mine a block with a Sprout shielded tx and verify the final Sapling root does not change
        zaddr0 = self.nodes[0].listaddresses()[0]['sprout']['addresses'][0]
        assert_equal(self.nodes[0].z_getbalance(zaddr0), Decimal('50'))
        recipients = [{"address": taddr0, "amount": Decimal('12.34')}]
        opid = self.nodes[0].z_sendmany(zaddr0, recipients, 1, 0, 'AllowRevealedRecipients')
        wait_and_assert_operationid_status(self.nodes[0], opid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(len(self.nodes[0].getblock("204")["tx"]), 2)
        assert_equal(self.nodes[0].z_getbalance(zaddr0), Decimal("37.66"))
        assert_equal(root, self.nodes[0].getblock("204")["finalsaplingroot"])

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sapling"]["commitments"]["finalRoot"], root)
        assert_equal(new_treestate["sapling"], treestate["sapling"])
        assert new_treestate["sprout"]["commitments"]["finalRoot"] != treestate["sprout"]["commitments"]["finalRoot"]
        assert new_treestate["sprout"]["commitments"]["finalState"] != treestate["sprout"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["sprout"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["sprout"]["commitments"]["finalState"]), 204)
        treestate = new_treestate

        # Mine a block with a Sapling shielded recipient and verify the final Sapling root changes
        saplingAddr1 = self.nodes[1].z_getnewaddress("sapling")
        recipients = [{"address": saplingAddr1, "amount": Decimal('2.34')}]
        myopid = self.nodes[0].z_sendmany(saplingAddr0, recipients, 1, 0)
        mytxid = wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(len(self.nodes[0].getblock("205")["tx"]), 2)
        assert_equal(self.nodes[1].z_getbalance(saplingAddr1), Decimal("2.34"))
        assert root is not self.nodes[0].getblock("205")["finalsaplingroot"]

        # Verify there is a Sapling output description (its commitment was added to tree)
        result = self.nodes[0].getrawtransaction(mytxid, 1)
        assert_equal(len(result["vShieldedOutput"]), 2)  # there is Sapling shielded change

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert new_treestate["sapling"]["commitments"]["finalRoot"] != treestate["sapling"]["commitments"]["finalRoot"]
        assert new_treestate["sapling"]["commitments"]["finalState"] != treestate["sapling"]["commitments"]["finalState"]
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalRoot"]), 64)
        assert_equal(len(new_treestate["sapling"]["commitments"]["finalState"]), 136)
        treestate = new_treestate

        # Mine a block with a Sapling shielded sender and transparent recipient and verify the final Sapling root doesn't change
        taddr2 = self.nodes[0].getnewaddress()
        recipients = []
        recipients.append({"address": taddr2, "amount": Decimal('2.34')})
        myopid = self.nodes[1].z_sendmany(saplingAddr1, recipients, 1, 0, 'AllowRevealedRecipients')
        mytxid = wait_and_assert_operationid_status(self.nodes[1], myopid)

        self.sync_all()
        self.nodes[0].generate(1)
        self.sync_all()

        assert_equal(len(self.nodes[0].getblock("206")["tx"]), 2)
        assert_equal(self.nodes[0].z_getbalance(taddr2), Decimal("2.34"))

        blk = self.nodes[0].getblock("206")
        root = blk["finalsaplingroot"]
        assert_equal(root, self.nodes[0].getblock("205")["finalsaplingroot"])

        new_treestate = self.nodes[0].z_gettreestate(str(-1))
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert_equal(new_treestate["sapling"], treestate["sapling"])

        # Activate NU5; more testing should be added once we can mine orchard transactions.
        self.sync_all()
        self.nodes[0].generate(4)
        self.sync_all()
        new_treestate = self.nodes[0].z_gettreestate(str(-1))

        # sprout and sapling results should not change
        assert_equal(new_treestate["sprout"], treestate["sprout"])
        assert_equal(new_treestate["sapling"], treestate["sapling"])

        assert "skipHash" not in treestate["orchard"]
        assert_equal(new_treestate["orchard"]["commitments"]["finalRoot"], ORCHARD_TREE_EMPTY_ROOT)
        assert_equal(new_treestate["orchard"]["commitments"]["finalState"], "000000")
        pass


if __name__ == '__main__':
    FinalSaplingRootTest().main()
