import requests, json, logging, time

# Configure logging to console and file
logging.basicConfig(level=logging.INFO,
                    format="%(asctime)s %(levelname)s: %(message)s",
                    handlers=[logging.StreamHandler(),
                              logging.FileHandler("rpc.log", mode='w')])
logger = logging.getLogger()

# RPC Configuration
RPC_USER = "admin"
RPC_PASS = "admin"
RPC_PORT = 18232
RPC_URL = f"http://127.0.0.1:{RPC_PORT}/"

# Basic RPC call function
def rpc_call(method, params=None):
    payload = json.dumps({"jsonrpc": "1.0", "id": "python", "method": method, "params": params or []})
    response = requests.post(RPC_URL, data=payload, auth=(RPC_USER, RPC_PASS))
    result = response.json()
    if response.status_code != 200 or result.get('error'):
        raise Exception(f"RPC error ({method}): {result.get('error')}")
    return result['result']

# PASS/FAIL counters
pass_count = 0
fail_count = 0

def run_test(name, func):
    global pass_count, fail_count
    try:
        func()
        logger.info(f"{name}: PASS")
        pass_count += 1
    except Exception as e:
        logger.error(f"{name}: FAIL - {e}")
        fail_count += 1

# Function to wait for asynchronous operations (e.g z_sendmany)
def wait_for_op(opid):
    while True:
        ops = rpc_call("z_getoperationstatus", [[opid]])
        if not ops:
            raise Exception(f"No status found for operation {opid}")
        status = ops[0].get("status")
        if status == "success":
            return
        elif status == "failed":
            raise Exception(f"Operation {opid} failed: {ops[0].get('error')}")
        time.sleep(1)

# --- Script Start ---

# 1. getinfo
info = rpc_call("getinfo")
run_test("getinfo", lambda: info)

# Create new account and unified address from scratch to ensure Sapling
acct = rpc_call("z_getnewaccount")["account"]
ua_info = rpc_call("z_getaddressforaccount", [acct])
ua_address = ua_info["address"]
receivers = rpc_call("z_listunifiedreceivers", [ua_address])
sapling_recv = receivers.get("sapling")
if not sapling_recv:
    raise Exception("No Sapling receiver found in the unified address!")


# 3. getbalance
run_test("getbalance", lambda: rpc_call("getbalance", ["*", 1]))

# 4. List accounts and addresses
run_test("z_listaccounts", lambda: rpc_call("z_listaccounts"))
list_addr = rpc_call("listaddresses")
run_test("listaddresses", lambda: list_addr)

# Extract TADDR and UA with diversified index 0
taddrs = []
uaddrs = []
first_taddr = None
first_ua = None

for src in list_addr:
    if "transparent" in src:
        taddrs.extend(src["transparent"].get("addresses", []))
    if "unified" in src:
        for ug in src["unified"]:
            for entry in ug.get("addresses", []):
                uaddrs.append(entry["address"])
                if first_ua is None and entry.get("diversifier_index", 1) == 0:
                    first_ua = entry["address"]

first_taddr = taddrs[0] if taddrs else None

# 5. z_getbalanceforaccount
run_test("z_getbalanceforaccount", lambda: rpc_call("z_getbalanceforaccount", [acct, 1]))

# 6. Send funds TADDR -> UA
#TODO

# 7. Send funds UA -> UA
if first_taddr and ua_address:
    opid2 = rpc_call("z_sendmany", [first_ua, [{"address": ua_address, "amount": 0.001}], 1, 0.0001])
    run_test("z_sendmany (UA -> UA)", lambda: wait_for_op(opid2))

# 8. z_listoperationids
run_test("z_listoperationids", lambda: rpc_call("z_listoperationids"))

# 9. z_getbalanceforviewingkey
if sapling_recv:
    vkey = rpc_call("z_exportviewingkey", [sapling_recv])
    run_test("z_getbalanceforviewingkey", lambda: rpc_call("z_getbalanceforviewingkey", [vkey, 1]))

# 10. z_gettotalbalance
run_test("z_gettotalbalance", lambda: rpc_call("z_gettotalbalance", [1]))

# 11. New account
acct_info = rpc_call("z_getnewaccount")
acct_id = acct_info.get("account")
run_test("z_getnewaccount", lambda: isinstance(acct_id, int))

# 12. Unified address
addr_info = rpc_call("z_getaddressforaccount", [acct_id])
ua = addr_info.get("address")
run_test("z_getaddressforaccount", lambda: bool(ua))

# 13. List accounts
accounts = rpc_call("z_listaccounts")
run_test("z_listaccounts contains the new account", lambda: any(a.get("account") == acct_id for a in accounts))

# 14. List addresses
listed = rpc_call("listaddresses")
found_ua = False
for src in listed:
    if "unified" in src:
        for entry in src["unified"]:
            if entry.get("account") == acct_id:
                found_ua = True
run_test("listaddresses (UA present)", lambda: found_ua)

# 15. Balance after send
bal_acc2 = rpc_call("z_getbalanceforaccount", [acct_id])
pools = bal_acc2.get("pools", {})
run_test("z_getbalanceforaccount after send", lambda: any(pools.get(pool, {}).get("valueZat", 0) > 0 for pool in pools))

# 16. Wallet | List unspent notes after a transaction
run_test("z_listunspent", lambda: rpc_call("z_listunspent"))

# 17. Wallet | Get wallet info
run_test("getwalletinfo", lambda: rpc_call("getwalletinfo"))

# 18. Network | Get peer info
run_test("getpeerinfo", lambda: rpc_call("getpeerinfo"))

# 19. Network | Get network info
run_test("getnetworkinfo", lambda: rpc_call("getnetworkinfo"))

# 20. Network | Get deprecation info
run_test("getdeprecationinfo", lambda: rpc_call("getdeprecationinfo"))

# 21. Network | Get connection count
run_test("getconnectioncount", lambda: rpc_call("getconnectioncount"))

# 22. Network | Get added node info
run_test("getaddednodeinfo", lambda: rpc_call("getaddednodeinfo", [True]))

# 23. Mining | Get block subsidy
run_test("getblocksubsidy", lambda: rpc_call("getblocksubsidy"))

# 24. Mining | Get block template
run_test("getblocktemplate", lambda: rpc_call("getblocktemplate", [{"rules": ["segwit"]}]))

# 25. Mining | Get mining info
run_test("getmininginfo", lambda: rpc_call("getmininginfo"))

# 26. Mining | Get network hash rate
run_test("getnetworkhashps", lambda: rpc_call("getnetworkhashps"))

# 27. Mining | Get network solution rate
run_test("getnetworksolps", lambda: rpc_call("getnetworksolps"))

# --- End ---
logger.info(f"\nFinal summary: {pass_count} PASS, {fail_count} FAIL")
