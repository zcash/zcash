#!/usr/bin/env python3
# Copyright (c) 2014 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

#
# Test rpc http basics
#

from test_framework.test_framework import ZcashTestFramework
from test_framework.util import  start_nodes, str_to_b64str

from http.client import HTTPConnection
from urllib.parse import urlparse

class HTTPBasicsTest (ZcashTestFramework):
    def setup_nodes(self):
        return start_nodes(4, self.options.tmpdir)

    def run_test(self):

        #################################################
        # lowlevel check for http persistent connection #
        #################################################
        url = urlparse(self.nodes[0].url)
        authpair = url.username + ':' + url.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out1, True)
        self.assertEqual(conn.sock!=None, True) # according to http/1.1 connection must still be open!

        # send 2nd request without closing connection
        conn.request('POST', '/', '{"method": "getchaintips"}', headers)
        out2 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out2, True) # must also response with a correct json-rpc message
        self.assertEqual(conn.sock!=None, True) # according to http/1.1 connection must still be open!
        conn.close()

        # same should be if we add keep-alive because this should be the std. behaviour
        headers = {"Authorization": "Basic " + str_to_b64str(authpair), "Connection": "keep-alive"}

        conn = HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out1, True)
        self.assertEqual(conn.sock!=None, True) # according to http/1.1 connection must still be open!

        # send 2nd request without closing connection
        conn.request('POST', '/', '{"method": "getchaintips"}', headers)
        out2 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out2, True) # must also response with a correct json-rpc message
        self.assertEqual(conn.sock!=None, True) # according to http/1.1 connection must still be open!
        conn.close()

        # now do the same with "Connection: close"
        headers = {"Authorization": "Basic " + str_to_b64str(authpair), "Connection":"close"}

        conn = HTTPConnection(url.hostname, url.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out1, True)
        self.assertEqual(conn.sock!=None, False) # now the connection must be closed after the response

        # node1 (2nd node) is running with disabled keep-alive option
        urlNode1 = urlparse(self.nodes[1].url)
        authpair = urlNode1.username + ':' + urlNode1.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = HTTPConnection(urlNode1.hostname, urlNode1.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out1, True)

        # node2 (third node) is running with standard keep-alive parameters which means keep-alive is on
        urlNode2 = urlparse(self.nodes[2].url)
        authpair = urlNode2.username + ':' + urlNode2.password
        headers = {"Authorization": "Basic " + str_to_b64str(authpair)}

        conn = HTTPConnection(urlNode2.hostname, urlNode2.port)
        conn.connect()
        conn.request('POST', '/', '{"method": "getbestblockhash"}', headers)
        out1 = conn.getresponse().read()
        self.assertEqual(b'"error":null' in out1, True)
        self.assertEqual(conn.sock!=None, True) # connection must be closed because zcashd should use keep-alive by default

if __name__ == '__main__':
    HTTPBasicsTest().main()
