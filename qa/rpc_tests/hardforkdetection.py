#!/usr/bin/env python2

#
# Test hard fork detection
#

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from test_framework.util import assert_equal, start_node

import os

class HardForkDetectionTest(BitcoinTestFramework):

    alert_filename = None  # Set by setup_network

    def setup_network(self):
        self.nodes = []
        self.alert_filename = os.path.join(self.options.tmpdir, "alert.txt")
        with open(self.alert_filename, 'w'):
            pass  # Just open then close to create zero-length file
        self.nodes.append(start_node(0, self.options.tmpdir,
                            ["-blockversion=2", "-alertnotify=echo %s >> \"" + self.alert_filename + "\""]))

    def assert_safemode_off(self):
        self.nodes[0].getbalance()

    def assert_safemode_on(self, requiredMessage):
        errorString = ""
        try:
            self.nodes[0].getbalance()
        except JSONRPCException,e:
            errorString = e.error['message']

        assert_equal("Safe mode:" in errorString, True)
        assert_equal(requiredMessage in errorString, True)

    def run_test(self):
        # Generate 10 blocks
        self.nodes[0].generate(100)

        # Invalidate all of them.
        for block_height in range(100, 0, -1):
            block_hash = self.nodes[0].getblockhash(block_height)
            self.nodes[0].invalidateblock(block_hash)

        # Check that safe mode is on.
        self.assert_safemode_on("We do not appear to fully agree with our peers!")

        # Check that an -alertnotify was triggered.
        with open(self.alert_filename, 'r') as f:
            alert_text = f.read()

        if len(alert_text) == 0:
            raise AssertionError("-alertnotify did not warn of detected hard fork")

        # If our chain keeps growing, but the hard forking chain remains longer,
        # safe mode should stay on.
        self.nodes[0].generate(50)
        self.assert_safemode_on("We do not appear to fully agree with our peers!")

        # If we're on the longer side of the hard fork, safe mode should get
        # turned off.
        self.nodes[0].generate(50)
        self.assert_safemode_off()


if __name__ == '__main__':
    HardForkDetectionTest().main()
