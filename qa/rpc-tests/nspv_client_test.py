import sys
sys.path.append('../../src/tui')

from lib import tuilib
import unittest
import time

'''
specify chain ticker (daemon should be up), wif which will be imported and address to which you want to broadcast
added 1 second sleep after each case to surely not face the nSPV server limitation (1 call/second)
'''

wif = ''
dest_address = ''
amount = '0.1'
chain = 'ILN'

if not wif or not dest_address:
    raise Exception("Please set test wif and address to send transactions to.")

rpc_proxy = tuilib.def_credentials(chain)


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
        result = rpc_proxy.nspv_listtransactions("RUp3xudmdTtxvaRnt3oq78FJBjotXy55uu")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["address"], "RUp3xudmdTtxvaRnt3oq78FJBjotXy55uu")
        rpc_proxy.nspv_logout()

    def test_nspv_getinfo(self):
        print("testing nspv_getinfo")
        result = rpc_proxy.nspv_getinfo()
        self.assertEqual(result["result"], "success")
        self.assertGreater(result["height"], 2689)
        time.sleep(1)

    def test_nspv_notarizations(self):
        print("testing nspv_notarizations")
        result = rpc_proxy.nspv_notarizations("2000")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["prev"]["notarized_height"], 1998)
        self.assertEqual(result["next"]["notarized_height"], 2008)  # check suspicious behaviour
        time.sleep(1)

    def test_nspv_hdrsproof(self):
        print("testing nspv_hdrsproof")
        result = rpc_proxy.nspv_hdrsproof("2000", "2100")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["numhdrs"], 113)
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
        result = rpc_proxy.nspv_listunspent("RUp3xudmdTtxvaRnt3oq78FJBjotXy55uu")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["address"], "RUp3xudmdTtxvaRnt3oq78FJBjotXy55uu")

    def test_nspv_spend(self):
        print("testing nspv_spend")
        result = rpc_proxy.nspv_login(wif)
        result = rpc_proxy.nspv_spend(dest_address, amount)
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["vout"][0]["valueZat"], 10000000)
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
        result = rpc_proxy.nspv_spentinfo("67ffe0eaecd6081de04675c492a59090b573ee78955c4e8a85b8ac0be0e8e418", "1")
        self.assertEqual(result["result"], "success")
        self.assertEqual(result["spentheight"], 2681)
        time.sleep(1)

    def test_nspv_txproof(self):
        print("testing nspv_txproof")
        result = rpc_proxy.nspv_txproof("67ffe0eaecd6081de04675c492a59090b573ee78955c4e8a85b8ac0be0e8e418", "2673")
        self.assertEqual(result["txid"], "67ffe0eaecd6081de04675c492a59090b573ee78955c4e8a85b8ac0be0e8e418")
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
