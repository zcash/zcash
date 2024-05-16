#!/usr/bin/env python3
# Copyright (c) 2021 The Zcash developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://www.opensource.org/licenses/mit-license.php .

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    start_nodes,
)


class ConvertTEXTest(BitcoinTestFramework):
    '''
    Test that the `z_converttex` RPC method correctly converts transparent
    addresses to ZIP 320 TEX addresses.
    '''

    def __init__(self):
        super().__init__()
        self.num_nodes = 1

    def setup_network(self, split=False):
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir)
        self.is_network_split = False

    def run_test(self):
        node = self.nodes[0]

        # From https://github.com/zcash-hackworks/zcash-test-vectors/blob/master/zcash_test_vectors/transparent/zip_0320.py
        # ["t_addr, tex_addr"],
        test_vectors = [
            ["tmQqjg2hqn5XMK9v1wtueg1CpzGbgTNGZQu", "texregtest15hhh9uprfp6krumprglg6qpx3928ulrnlxt9na"],
            ["tmGiqpWKPJdraF2PqBzPojzkRbDE4fPTyAF", "texregtest1fns2jk8xpjr7rqtaggn2zpmcdtfyj2jer8arm0"],
            ["tmEkTF6UovNsEQM9h1ehnA3byw6yhFCJWor", "texregtest1xulx2a0pgc84phkdtue67zwe26axtcvvyaf6yu"],
            ["tmGoyC4XZ1GNCdJGk96K6mT8jxDQEhzVbfR", "texregtest1fhvw29vvg37mep5kkhyew47rrqadjtyk4xzx8n"],
            ["tmG4gSmUZzCcyR6S5nBhEFrfmodmUjXXAZG", "texregtest1gk5swlnzf8m5hc9x82344aqv5s90k5x2dvqyvh"],
            ["tmKgnRCv6SjwEFgXhqPoADKp3HLFF67Seww", "texregtest1d4j4uz8wnl5zmuzdl7y3fykrkk2zarnccfs578"],
            ["tmTymg9bGECw8tR8WHepE45c4joNTUt1zth", "texregtest1epwdxsm94e9wh2zwad7j885gelxly2f8d4mqry"],
            ["tmMGGBngBJwgTYWCx23zWDY7QvLateZNqCC", "texregtest106exvc6ufugdwppy7vnuf2vwztf5qh9r4tpsfa"],
            ["tmUyukTGWjTM7Nw4j8zgbZZwPfM8enu9NvZ", "texregtest16ddmp690el6vrzajhftc3fqpmx4a3cgfqf97yn"],
            ["tmDsJSojZxU3sb3LGMs6nMC1SVSQhKD99My", "texregtest19kgadp7hwu08xlufnvr2r0p5aygw3ss60px28u"],
            ["tmWezycaJjPoXNJxK2zvzELjS65mzExnvVE", "texregtest1ukuxwrfrj20q65c25ssk3nkathsy8qneehv5vl"],
            ["tmMUEbcXX7tVwtRjaSaMVfzXu6PCeyMnsCH", "texregtest1srmppx752ux07mntsjmthpddy2enunfuh6mlej"],
            ["tmDjbRj8go7BrS8AxSjfjTBsCcHw7J45SCi", "texregtest19sw2lplpdswc4zvdtz3z8yt42wtz4recz5plym"],
            ["tmSSJADGK9bgafaY9eih17WRazi3KzNL7RT", "texregtest1kacdt4amhphqx6tf4gla7a4qg8h9ey5tauz24v"],
            ["tmSJ3JSRx2R71MfBksWzHNEfMxUgzMxDMxy", "texregtest1khs34j6m944un75ula8xxgvgdnjc4m2l0kfgpy"]
        ];

        for tv in test_vectors:
            tex = node.z_converttex(tv[0])
            assert_equal(tex, tv[1])

if __name__ == '__main__':
    ConvertTEXTest().main()
