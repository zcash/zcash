import sys
sys.path.append('../../src/tui')

from lib import tuilib
import unittest
import time

'''
specify chain ticker (daemon should be up), wif which will be imported and address to which you want to broadcast
change chain parameters if needed or add a new chain to test below
added 1 second sleep after each case to surely not face the nSPV server limitation (1 call/second)
'''

wif = ''
dest_address = ''
amount = '0.01'
chain = 'ILN'

if not wif or not dest_address:
    raise Exception("Please set test wif and address to send transactions to")

rpc_proxy = tuilib.def_credentials(chain)

chain_params = {"KMD": {
                        'tx_list_address': 'RGShWG446Pv24CKzzxjA23obrzYwNbs1kA',
                        'min_chain_height': 1468080,
                        'notarization_height': '1468000',
                        'prev_notarization_h': 1467980,
                        'next_notarization_h': 1468020,
                        'hdrs_proof_low': '1468100',
                        'hdrs_proof_high': '1468200',
                        'numhdrs_expected': 151,
                        'tx_proof_id': 'f7beb36a65bc5bcbc9c8f398345aab7948160493955eb4a1f05da08c4ac3784f',
                        'tx_spent_height': 1456212,
                        'tx_proof_height': '1468520',
                       },
                "ILN": {
                        'tx_list_address': 'RUp3xudmdTtxvaRnt3oq78FJBjotXy55uu',
                        'min_chain_height': 3689,
                        'notarization_height': '2000',
                        'prev_notarization_h': 1998,
                        'next_notarization_h': 2008,
                        'hdrs_proof_low': '2000',
                        'hdrs_proof_high': '2100',
                        'numhdrs_expected': 113,
                        'tx_proof_id': '67ffe0eaecd6081de04675c492a59090b573ee78955c4e8a85b8ac0be0e8e418',
                        'tx_spent_height': 2681,
                        'tx_proof_height': '2690',
                       }
                }


class TestNspvClient(unittest.TestCase):

    def test_nspv_mempool(self):
        print("testing nspv_mempool")
        result = rpc_proxy.nspv_mempool("0", dest_address, "0")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["address"], dest_address)
        self.assertEqual(result["isCC"], 0)
        time.sleep(1)

    def test_nspv_listtransactions(self):
        print("testing nspv_listtransactions")
        rpc_proxy.nspv_login(wif)
        result = rpc_proxy.nspv_listtransactions()
        self.assertEqual(result["result"], "success")
        time.sleep(1)
        result = rpc_proxy.nspv_listtransactions(chain_params.get(chain).get("tx_list_address"))
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["address"], chain_params.get(chain).get("tx_list_address"))
        rpc_proxy.nspv_logout()

    def test_nspv_getinfo(self):
        print("testing nspv_getinfo")
        result = rpc_proxy.nspv_getinfo()
        self.assertEqual(result["result"], "success")
        self.assertGreater(result["height"], chain_params.get(chain).get("min_chain_height"))
        time.sleep(1)

    def test_nspv_notarizations(self):
        print("testing nspv_notarizations")
        result = rpc_proxy.nspv_notarizations(chain_params.get(chain).get("notarization_height"))
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["prev"]["notarized_height"], chain_params.get(chain).get("prev_notarization_h"))
        self.assertEqual(result["next"]["notarized_height"], chain_params.get(chain).get("next_notarization_h"))
        time.sleep(1)

    def test_nspv_hdrsproof(self):
        print("testing nspv_hdrsproof")
        result = rpc_proxy.nspv_hdrsproof(chain_params.get(chain).get("hdrs_proof_low"),
                                          chain_params.get(chain).get("hdrs_proof_high"))
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["numhdrs"], chain_params.get(chain).get("numhdrs_expected"))
        time.sleep(1)

    def test_nspv_login(self):
        print("testing nspv_login")
        result = rpc_proxy.nspv_login(wif)
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["status"], "wif will expire in 777 seconds")
        time.sleep(1)

    def test_nspv_listunspent(self):
        print("testing nspv_listunspent")
        result = rpc_proxy.nspv_listunspent()
        self.assertEqual(result["result"], "success")
        time.sleep(1)
        result = rpc_proxy.nspv_listunspent(chain_params.get(chain).get("tx_list_address"))
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["address"], chain_params.get(chain).get("tx_list_address"))

    def test_nspv_spend(self):
        print("testing nspv_spend")
        result = rpc_proxy.nspv_login(wif)
        result = rpc_proxy.nspv_spend(dest_address, amount)
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["vout"][0]["valueZat"], 1000000)
        time.sleep(1)

    def test_nspv_broadcast(self):
        print("testing nspv_broadcast")
        result = rpc_proxy.nspv_login(wif)
        broadcast_hex = rpc_proxy.nspv_spend(dest_address, amount)["hex"]
        time.sleep(1)
        result = rpc_proxy.nspv_broadcast(broadcast_hex)
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["retcode"], 1)
        self.assertEqual(result["expected"], result["broadcast"])
        print("Broadcast txid: " + result["broadcast"])
        time.sleep(1)

    def test_nspv_logout(self):
        print("testing nspv_logout")
        rpc_proxy.nspv_login(wif)
        time.sleep(1)
        rpc_proxy.nspv_logout()
        time.sleep(1)
        result = rpc_proxy.nspv_spend(dest_address, amount)
        self.assertEqual(result["result"], "error")
        self.assertEqual(result["error"], "wif expired")
        time.sleep(1)

    def test_nspv_spentinfo(self):
        print("testing nspv_spent_info")
        result = rpc_proxy.nspv_spentinfo(chain_params.get(chain).get("tx_proof_id"), "1")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["spentheight"], chain_params.get(chain).get("tx_spent_height"))
        time.sleep(1)

    def test_nspv_txproof(self):
        print("testing nspv_txproof")
        result = rpc_proxy.nspv_txproof(chain_params.get(chain).get("tx_proof_id"),
                                        chain_params.get(chain).get("tx_proof_height"))
        self.assertEqual(result["txid"], chain_params.get(chain).get("tx_proof_id"))
        time.sleep(1)

    def test_nspv_login_timout(self):
        print("testing auto-logout in 777 seconds")
        rpc_proxy.nspv_login(wif)
        time.sleep(778)
        result = rpc_proxy.nspv_spend(dest_address, amount)
        self.assertEqual(result["result"], "error")
        self.assertEqual(result["error"], "wif expired")
        time.sleep(1)


if __name__ == '__main__':
    unittest.main()
