#!/usr/bin/env python2
# Copyright (c) 2018 SuperNET developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.


from test_framework.test_framework import CryptoconditionsTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, assert_greater_than, \
    initialize_chain_clean, initialize_chain, start_nodes, start_node, connect_nodes_bi, \
    stop_nodes, sync_blocks, sync_mempools, wait_bitcoinds, rpc_port, assert_raises
from cryptoconditions import assert_success, assert_error, generate_random_string


class CryptoconditionsTokenTest(CryptoconditionsTestFramework):

    def run_token_tests(self):

        rpc  = self.nodes[0]
        rpc1 = self.nodes[1]

        result = rpc.tokenaddress()
        assert_success(result)
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result = rpc.tokenaddress(self.pubkey)
        assert_success(result)
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result = rpc.assetsaddress()
        assert_success(result)
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        result = rpc.assetsaddress(self.pubkey)
        assert_success(result)
        for x in result.keys():
            if x.find('ddress') > 0:
                assert_equal(result[x][0], 'R')

        # there are no tokens created yet
        result = rpc.tokenlist()
        assert_equal(result, [])

        # trying to create token with negative supply
        result = rpc.tokencreate("NUKE", "-1987420", "no bueno supply")
        assert_error(result)

        # creating token with name more than 32 chars
        result = rpc.tokencreate("NUKE123456789012345678901234567890", "1987420", "name too long")
        assert_error(result)

        # creating valid token
        result = rpc.tokencreate("DUKE", "1987.420", "Duke's custom token")
        assert_success(result)

        tokenid = self.send_and_mine(result['hex'], rpc)

        result = rpc.tokenlist()
        assert_equal(result[0], tokenid)

        # there are no token orders yet
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # getting token balance for non existing tokenid
        result = rpc.tokenbalance(self.pubkey)
        assert_error(result)

        # get token balance for token with pubkey
        result = rpc.tokenbalance(tokenid, self.pubkey)
        assert_success(result)
        assert_equal(result['balance'], 198742000000)
        assert_equal(result['tokenid'], tokenid)

        # get token balance for token without pubkey
        result = rpc.tokenbalance(tokenid)
        assert_success(result)
        assert_equal(result['balance'], 198742000000)
        assert_equal(result['tokenid'], tokenid)

        # this is not a valid assetid
        result = rpc.tokeninfo(self.pubkey)
        assert_error(result)

        # check tokeninfo for valid token
        result = rpc.tokeninfo(tokenid)
        assert_success(result)
        assert_equal(result['tokenid'], tokenid)
        assert_equal(result['owner'], self.pubkey)
        assert_equal(result['name'], "DUKE")
        assert_equal(result['supply'], 198742000000)
        assert_equal(result['description'], "Duke's custom token")

        # invalid numtokens ask
        result = rpc.tokenask("-1", tokenid, "1")
        assert_error(result)

        # invalid numtokens ask
        result = rpc.tokenask("0", tokenid, "1")
        assert_error(result)

        # invalid price ask
        result = rpc.tokenask("1", tokenid, "-1")
        assert_error(result)

        # invalid price ask
        result = rpc.tokenask("1", tokenid, "0")
        assert_error(result)

        # invalid tokenid ask
        result = rpc.tokenask("100", "deadbeef", "1")
        assert_error(result)

        # valid ask
        tokenask = rpc.tokenask("100", tokenid, "7.77")
        tokenaskhex = tokenask['hex']
        tokenaskid = self.send_and_mine(tokenask['hex'], rpc)
        result = rpc.tokenorders(tokenid)
        order = result[0]
        assert order, "found order"

        # invalid ask fillunits
        result = rpc.tokenfillask(tokenid, tokenaskid, "0")
        assert_error(result)

        # invalid ask fillunits
        result = rpc.tokenfillask(tokenid, tokenaskid, "-777")
        assert_error(result)

        # valid ask fillunits
        fillask = rpc.tokenfillask(tokenid, tokenaskid, "777")
        result = self.send_and_mine(fillask['hex'], rpc)
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # checking ask cancellation
        testorder = rpc.tokenask("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'], rpc)
        # from other node (ensuring that second node have enough balance to cover txfee
        # to get the actual error - not "not enough balance" one
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.sendtoaddress(rpc1.getnewaddress(), 1)
        rpc.generate(2)
        self.sync_all()
        result = rpc1.getbalance()
        assert_greater_than(result, 0.1)

        result = rpc1.tokencancelask(tokenid, testorderid)
        assert_error(result)

        # from valid node
        cancel = rpc.tokencancelask(tokenid, testorderid)
        self.send_and_mine(cancel["hex"], rpc)
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])


        # invalid numtokens bid
        result = rpc.tokenbid("-1", tokenid, "1")
        assert_error(result)

        # invalid numtokens bid
        result = rpc.tokenbid("0", tokenid, "1")
        assert_error(result)

        # invalid price bid
        result = rpc.tokenbid("1", tokenid, "-1")
        assert_error(result)

        # invalid price bid
        result = rpc.tokenbid("1", tokenid, "0")
        assert_error(result)

        # invalid tokenid bid
        result = rpc.tokenbid("100", "deadbeef", "1")
        assert_error(result)

        tokenbid = rpc.tokenbid("100", tokenid, "10")
        tokenbidhex = tokenbid['hex']
        tokenbidid = self.send_and_mine(tokenbid['hex'], rpc)
        result = rpc.tokenorders(tokenid)
        order = result[0]
        assert order, "found order"

        # invalid bid fillunits
        result = rpc.tokenfillbid(tokenid, tokenbidid, "0")
        assert_error(result)

        # invalid bid fillunits
        result = rpc.tokenfillbid(tokenid, tokenbidid, "-777")
        assert_error(result)

        # valid bid fillunits
        fillbid = rpc.tokenfillbid(tokenid, tokenbidid, "1000")
        result = self.send_and_mine(fillbid['hex'], rpc)
        txid   = result[0]
        assert txid, "found txid"

        # should be no token orders
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # checking bid cancellation
        testorder = rpc.tokenbid("100", tokenid, "7.77")
        testorderid = self.send_and_mine(testorder['hex'], rpc)

        # from other node
        result = rpc1.getbalance()
        assert_greater_than(result, 0.1)

        result = rpc1.tokencancelbid(tokenid, testorderid)
        assert_error(result)

        # from valid node
        cancel = rpc.tokencancelbid(tokenid, testorderid)
        self.send_and_mine(cancel["hex"], rpc)
        result = rpc.tokenorders(tokenid)
        assert_equal(result, [])

        # invalid token transfer amount (have to add status to CC code!)
        randompubkey = "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96"
        result = rpc.tokentransfer(tokenid,randompubkey,"0")
        assert_error(result)

        # invalid token transfer amount (have to add status to CC code!)
        result = rpc.tokentransfer(tokenid,randompubkey,"-1")
        assert_error(result)

        # valid token transfer
        sendtokens = rpc.tokentransfer(tokenid,randompubkey,"1")
        self.send_and_mine(sendtokens["hex"], rpc)
        result = rpc.tokenbalance(tokenid,randompubkey)
        assert_equal(result["balance"], 1)

    def run_test(self):
        print("Mining blocks...")
        rpc = self.nodes[0]
        rpc1 = self.nodes[1]
        # utxos from block 1 become mature in block 101
        if not self.options.noshutdown:
            rpc.generate(101)
        self.sync_all()
        rpc.getinfo()
        rpc1.getinfo()
        # this corresponds to -pubkey above
        print("Importing privkeys")
        rpc.importprivkey(self.privkey)
        rpc1.importprivkey(self.privkey1)
        self.run_token_tests()


if __name__ == '__main__':
    CryptoconditionsTokenTest().main()
