#!/usr/bin/env python
# Copyright (c) 2018 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

import sys; assert sys.version_info < (3,), ur"This script does not run under Python 3. Please use Python 2.7.x."

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import (
    assert_equal, assert_true, assert_not_equal, start_nodes,
    get_coinbase_address,
    wait_and_assert_operationid_status
)
from decimal import Decimal


class WalletImportExportTest (BitcoinTestFramework):
    
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir, [[
            '-nuparams=5ba81b19:201', # Overwinter
            '-nuparams=76b809bb:203', # Sapling
            '-experimentalfeatures', '-zmergetoaddress',
        ]] * 4)

    def verify_diversified_set(self, noderef, base_addr, alladdrs):
        qalladdrs = noderef.z_getalldiversifiedaddresses(base_addr)
        qalladdrs.sort()
        alladdrs.sort()
        assert_equal(qalladdrs, alladdrs)

    def run_test(self):
        sapling_address = self.nodes[0].z_getnewaddress('sapling')
        addr2 = self.nodes[0].z_getnewdiversifiedaddress(sapling_address)
        
        alladdrs = self.nodes[0].z_getalldiversifiedaddresses(sapling_address)
        
        # Assert that both addresses are diviersified
        assert_equal(len(alladdrs), 2, "There should be 2 diversified addresses now")
        assert_true(sapling_address in alladdrs, "Original should be present")
        assert_true(addr2 in alladdrs, "Diversified address should be present")

        # Query it from the other address
        self.verify_diversified_set(self.nodes[0], sapling_address, [sapling_address, addr2])

        # Add a third diversified address
        addr3 = self.nodes[0].z_getnewdiversifiedaddress(sapling_address)
        self.verify_diversified_set(self.nodes[0], sapling_address, [sapling_address, addr2, addr3])


        # Now, add an entirely new address, and make sure it shows up separately.
        sapling_address2 = self.nodes[0].z_getnewaddress('sapling')
        alladdrs = self.nodes[0].z_getalldiversifiedaddresses(sapling_address)
        newaddrs = self.nodes[0].z_getalldiversifiedaddresses(sapling_address2)

        assert_equal(len(alladdrs), 3)
        assert_equal(len(newaddrs), 1)

        # Assert that the private keys match
        privkey = self.nodes[0].z_exportkey(sapling_address)
        privkey2 = self.nodes[0].z_exportkey(addr2)
        assert_equal(privkey, privkey2, "Private keys should match")

        newprivkey = self.nodes[0].z_exportkey(sapling_address2)
        assert_not_equal(privkey2, newprivkey)

        # Generate a new address and another set of diversified addresses based on that, to make
        # Sure that there are now 2 sets of diversified addresses
        baseaddr2 = self.nodes[0].z_getnewaddress('sapling')
        baseadd2div1 = self.nodes[0].z_getnewdiversifiedaddress(baseaddr2)
        baseadd2div2 = self.nodes[0].z_getnewdiversifiedaddress(baseaddr2)

        # Verify that the original set of diversified addresses still match as well as the new set
        self.verify_diversified_set(self.nodes[0], sapling_address, [sapling_address, addr2, addr3])
        self.verify_diversified_set(self.nodes[0], baseaddr2, [baseaddr2, baseadd2div1, baseadd2div2])
        

        # Sanity-check the test harness
        assert_equal(self.nodes[0].getblockcount(), 200)
        # Activate sapling
        self.nodes[3].generate(5)   
        self.sync_all()

        # Try sending to the diversified addresses and make sure it is recieved
        coinbase_taddr = get_coinbase_address(self.nodes[3])
        
        recipients = []
        recipients.append({"address": addr2, "amount": Decimal('10')})
        myopid = self.nodes[3].z_sendmany(coinbase_taddr, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[3], myopid)

        self.nodes[3].generate(1)
        self.sync_all()

        assert_equal(self.nodes[0].z_getbalance(addr2), Decimal("10"))

        # Then send from addr2 to addr3, both of which are diversified
        recipients = []
        recipients.append({"address": addr3, "amount": Decimal('1')})
        myopid = self.nodes[0].z_sendmany(addr2, recipients, 1, 0)
        wait_and_assert_operationid_status(self.nodes[0], myopid)

        self.nodes[0].generate(1)
        self.sync_all()
        
        # Now, balances should be 9 and 1
        assert_equal(self.nodes[0].z_getbalance(addr2), Decimal("9"))
        assert_equal(self.nodes[0].z_getbalance(addr3), Decimal("1"))

        # Import the private key into a new node, and ensure that we get the funds as well
        self.nodes[2].z_importkey(privkey)
        assert_equal(self.nodes[2].z_getbalance(addr2), Decimal("9"))
        assert_equal(self.nodes[2].z_getbalance(addr3), Decimal("1"))


        # We should be able to generate 100s of diversified addresses
        node2saplingaddress = self.nodes[1].z_getnewaddress("sapling")
        node2privkey = self.nodes[1].z_exportkey(node2saplingaddress)

        for i in range(0, 99):
            self.nodes[1].z_getnewdiversifiedaddress(node2saplingaddress)
        allnode2divaddrs = self.nodes[1].z_getalldiversifiedaddresses(node2saplingaddress)
        assert_equal(len(allnode2divaddrs), 100)

        #... and their private keys should match
        for i in range(0, 100):
            assert_equal(self.nodes[1].z_exportkey(allnode2divaddrs[i]), node2privkey)

        # Can't generate diversified address for an address that doesn't exist in the wallet
        try:
            self.nodes[0].z_getnewdiversifiedaddress(node2saplingaddress)
            raise AssertionError("Should have failed")
        except JSONRPCException as e:
            assert_equal("Wallet does not hold private zkey for this zaddr", e.error['message'])

        # Make sure that diversified addresses are a Sapling only feature
        sproutaddress = self.nodes[0].z_getnewaddress("sprout")
        try:
            self.nodes[0].z_getnewdiversifiedaddress(sproutaddress)
            raise AssertionError("Should have failed")
        except JSONRPCException as e:
            assert_equal("Invalid Sapling zaddr", e.error['message'])

        try:
            self.nodes[0].z_getalldiversifiedaddresses(sproutaddress)
            raise AssertionError("Should have failed")
        except JSONRPCException as e:
            assert_equal("Invalid Sapling zaddr", e.error['message'])



if __name__ == '__main__':
    WalletImportExportTest().main()