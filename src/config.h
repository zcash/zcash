// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <boost/assign/list_of.hpp>
#include <boost/unordered_map.hpp>
#include <string>

using boost::assign::map_list_of;

enum ConfigOption {
    CONF_HELP,
    CONF_VERSION,
    CONF_ALERTS,
    CONF_ALERT_NOTIFY,
    CONF_BLOCK_NOTIFY,
    CONF_CHECKBLOCKS,
    CONF_CHECKLEVEL,
    CONF_CONF,
    CONF_DAEMON,
    CONF_DATADIR,
    CONF_EXPORT_DIR,
    CONF_DBCACHE,
    CONF_LOAD_BLOCK,
    CONF_MAX_ORPHAN_TX,
    CONF_MEMPOOL_TX_LIMIT,
    CONF_PAR,
    CONF_PID,
    CONF_PRUNE,
    CONF_REINDEX,
    CONF_SYSPERMS,
    CONF_TXINDEX,
    CONF_ADD_NODE,
    CONF_BANSCORE,
    CONF_BANTIME,
    CONF_BIND,
    CONF_CONNECT,
    CONF_DISCOVER,
    CONF_DNS,
    CONF_DNS_SEED,
    CONF_EXTERNALIP,
    CONF_FORCE_DNS_SEED,
    CONF_LISTEN,
    CONF_LISTEN_ONION,
    CONF_MAX_CONN,
    CONF_MAX_RCV_BUF,
    CONF_MAX_SEND_BUF,
    CONF_MEMPOOL_EVICTION_MM,
    CONF_MEMPOOL_TX_COST_LIM,
    CONF_ONION,
    CONF_ONLY_NET,
    CONF_PERMIT_BARE_MULTISIG,
    CONF_PEER_BLOOM_FILTERS,
    CONF_ENFORCE_NODE_BLOOM,
    CONF_PORT,
    CONF_PROXY,
    CONF_PROXY_RANDOMIZE,
    CONF_SEED_NODE,
    CONF_TIMEOUT,
    CONF_TOR_CONTROL,
    CONF_TOR_PASSWORD,
    CONF_WHITEBIND,
    CONF_WHITELIST,
    CONF_DISABLE_WALLET,
    CONF_KEYPOOL,
    CONF_MIGRATION,
    CONF_MIG_DEST_ADDRESS,
    CONF_MIN_FEE,
    CONF_TX_PAY_FEE,
    CONF_RESCAN,
    CONF_SALVAGE_WALLET,
    CONF_SEND_FREE_TXS,
    CONF_SPEND_ZERO_CONF_CHANGE,
    CONF_CONFIRM_TARGET,
    CONF_TX_EXP_DELTA,
    CONF_TX_MAX_FEE,
    CONF_UPGRADE_WALLET,
    CONF_WALLET,
    CONF_WALLET_BROADCAST,
    CONF_WALLET_NOTIFY,
    CONF_ZAP_WALLET_TXS,
    CONF_CHECKPOINTS,
    CONF_DB_LOG_SIZE,
    CONF_DISABLE_SAFE_MODE,
    CONF_TEST_SAFE_MODE,
    CONF_DROP_MSG_TEST,
    CONF_FUZZ_MSG_TEST,
    CONF_FLUSH_WALLET,
    CONF_STOP_AFTER_BLOCK_IMPORT,
    CONF_NUPARAMS,
    CONF_DEBUG,
    CONF_EXP_FEATURES,
    CONF_HELP_DEBUG,
    CONF_LOGIPS,
    CONF_LOG_TIMESTAMPS,
    CONF_LIMIT_FREE_RELAY,
    CONF_RELAY_PRIORITY,
    CONF_MAX_SIG_CACHE_SIZE,
    CONF_MAX_TIP_AGE,
    CONF_MIN_RELAY_FEE,
    CONF_PRINT_TO_CONSOLE,
    CONF_PRINT_PRIORITY,
    CONF_PRIV_DB,
    CONF_REGTEST,
    CONF_TESTNET,
    CONF_DATA_CARRIER,
    CONF_DATA_CARRIER_SIZE,
    CONF_BLOCK_MIN_SIZE,
    CONF_BLOCK_MAX_SIZE,
    CONF_BLOCK_PRIORITY_SIZE,
    CONF_BLOCK_VERSION,
    CONF_GEN,
    CONF_GEN_PROC_LIM,
    CONF_EQUIHASH_SOLVER,
    CONF_MINER_ADDRESS,
    CONF_MINE_TO_LOCAL_WALLET,
    CONF_SERVER,
    CONF_REST,
    CONF_RPC_BIND,
    CONF_RPC_USER,
    CONF_RPC_PASSWORD,
    CONF_RPC_PORT,
    CONF_RPC_ALLOW_IP,
    CONF_RPC_THREADS,
    CONF_RPC_WORKQUEUE,
    CONF_RPC_CLIENT_TIMEOUT,
    CONF_RPC_SERVER_TIMEOUT,
    CONF_SHOW_METRICS,
    CONF_METRICS_UI,
    CONF_METRICS_REFRESH_TIME,

    CONF_STDIN,
    CONF_JSON,
    CONF_TXID,
    CONF_CREATE,
    CONF_NODEBUG,
    CONF_CHECK_BLOCK_INDEX,
    CONF_RPC_WAIT,
    CONF_RPC_SSL,
    CONF_RPC_COOKIE,
    CONF_ZMQ,
    CONF_MOCKTIME,
    CONF_ALERT_SEND,
    CONF_ALERT_PRINT,
    CONF_DEV_SET_POOL_ZERO,
    CONF_REG_TEST_COINBASE,
    CONF_UACOMMENT,
    CONF_CHECK_MEMPOOL,
    CONF_DEV_ENCRYPT_WALLET,
    CONF_DEV_SETPOOL_ZERO,
    CONF_PAYMENT_DISCLOSURE,
    CONF_INSIGHT_EXPLORER,
    
};

const boost::unordered_map<std::string,ConfigOption> FlagToConfigOption = map_list_of
    ("-?", CONF_HELP)
    ("-h", CONF_HELP)
    ("-help", CONF_HELP)
    ("-version", CONF_VERSION)

    ("-alerts", CONF_ALERTS)
    ("-alertnotify", CONF_ALERT_NOTIFY)
    ("-blocknotify", CONF_BLOCK_NOTIFY)
    ("-checkblocks", CONF_CHECKBLOCKS)
    ("-checklevel", CONF_CHECKLEVEL)
    ("-conf", CONF_CONF)
    ("-daemon", CONF_DAEMON)
    ("-datadir", CONF_DATADIR)
    ("-exportdir", CONF_EXPORT_DIR)
    ("-dbcache", CONF_DBCACHE)
    ("-loadblock", CONF_LOAD_BLOCK)
    ("-maxorphantx", CONF_MAX_ORPHAN_TX)
    ("-mempooltxinputlimit", CONF_MEMPOOL_TX_LIMIT)
    ("-par", CONF_PAR)
    ("-pid", CONF_PID)
    ("-prune", CONF_PRUNE)
    ("-reindex", CONF_REINDEX)
    ("-sysperms", CONF_SYSPERMS)
    ("-txindex", CONF_TXINDEX)
    ("-addnode", CONF_ADD_NODE)
    ("-banscore", CONF_BANSCORE)
    ("-bantime", CONF_BANTIME)
    ("-bind",CONF_BIND)
    ("-connect", CONF_CONNECT)
    ("-discover", CONF_DISCOVER)
    ("-dns", CONF_DNS)
    ("-dnsseed", CONF_DNS_SEED)
    ("-externalip", CONF_EXTERNALIP)
    ("-forcednsseed", CONF_FORCE_DNS_SEED)
    ("-listen", CONF_LISTEN)
    ("-listenonion", CONF_LISTEN_ONION)
    ("-maxconnections", CONF_MAX_CONN)
    ("-maxreceivebuffer", CONF_MAX_RCV_BUF)
    ("-maxsendbuffer", CONF_MAX_SEND_BUF)
    ("-mempoolevictionmemoryminutes", CONF_MEMPOOL_EVICTION_MM)
    ("-mempooltxcostlimit", CONF_MEMPOOL_TX_COST_LIM)
    ("-onion", CONF_ONION)
    ("-onlynet", CONF_ONLY_NET)
    ("-permitbaremultisig", CONF_PERMIT_BARE_MULTISIG)
    ("-peerbloomfilters", CONF_PEER_BLOOM_FILTERS)
    ("-enforcenodebloom", CONF_ENFORCE_NODE_BLOOM)
    ("-port", CONF_PORT)
    ("-proxy", CONF_PROXY)
    ("-proxyrandomize", CONF_PROXY_RANDOMIZE)
    ("-seednode", CONF_SEED_NODE)
    ("-timeout", CONF_TIMEOUT)
    ("-torcontrol", CONF_TOR_CONTROL)
    ("-torpassword", CONF_TOR_PASSWORD)
    ("-whitebind", CONF_WHITEBIND)
    ("-whitelist", CONF_WHITELIST)
    ("-disablewallet", CONF_DISABLE_WALLET)
    ("-keypool", CONF_KEYPOOL)
    ("-migration", CONF_MIGRATION)
    ("-migrationdestaddress", CONF_MIG_DEST_ADDRESS)
    ("-mintxfee", CONF_MIN_FEE)
    ("-paytxfee", CONF_TX_PAY_FEE)
    ("-rescan", CONF_RESCAN)
    ("-salvagewallet", CONF_SALVAGE_WALLET)
    ("-sendfreetransactions", CONF_SEND_FREE_TXS)
    ("-spendzeroconfchange", CONF_SPEND_ZERO_CONF_CHANGE)
    ("-txconfirmtarget", CONF_CONFIRM_TARGET)
    ("-txexpirydelta", CONF_TX_EXP_DELTA)
    ("-maxtxfee", CONF_TX_MAX_FEE)
    ("-upgradewallet", CONF_UPGRADE_WALLET)
    ("-wallet", CONF_WALLET)
    ("-walletbroadcast", CONF_WALLET_BROADCAST)
    ("-walletnotify", CONF_WALLET_NOTIFY)
    ("-zapwallettxes", CONF_ZAP_WALLET_TXS)
    ("-checkpoints", CONF_CHECKPOINTS)
    ("-dblogsize", CONF_DB_LOG_SIZE)
    ("-disablesafemode", CONF_DISABLE_SAFE_MODE)
    ("-testsafemode", CONF_TEST_SAFE_MODE)
    ("-dropmessagestest", CONF_DROP_MSG_TEST)
    ("-fuzzmessagestest", CONF_FUZZ_MSG_TEST)
    ("-flushwallet", CONF_FLUSH_WALLET)
    ("-stopafterblockimport", CONF_STOP_AFTER_BLOCK_IMPORT)
    ("-nuparams", CONF_NUPARAMS)
    ("-debug", CONF_DEBUG)
    ("-experimentalfeatures", CONF_EXP_FEATURES)
    ("-help-debug", CONF_HELP_DEBUG)
    ("-logips", CONF_LOGIPS)
    ("-logtimestamps", CONF_LOG_TIMESTAMPS)
    ("-limitfreerelay", CONF_LIMIT_FREE_RELAY)
    ("-relaypriority", CONF_RELAY_PRIORITY)
    ("-maxsigcachesize", CONF_MAX_SIG_CACHE_SIZE)
    ("-maxtipage", CONF_MAX_TIP_AGE)
    ("-minrelaytxfee", CONF_MIN_RELAY_FEE)
    ("-printtoconsole", CONF_PRINT_TO_CONSOLE)
    ("-printpriority", CONF_PRINT_PRIORITY)
    ("-privdb", CONF_PRIV_DB)
    ("-regtest", CONF_REGTEST)
    ("-testnet", CONF_TESTNET)
    ("-datacarrier", CONF_DATA_CARRIER)
    ("-datacarriersize", CONF_DATA_CARRIER_SIZE)
    ("-blockminsize", CONF_BLOCK_MIN_SIZE)
    ("-blockmaxsize", CONF_BLOCK_MAX_SIZE)
    ("-blockprioritysize", CONF_BLOCK_PRIORITY_SIZE)
    ("-blockversion", CONF_BLOCK_VERSION)
    ("-gen", CONF_GEN)
    ("-genproclimit", CONF_GEN_PROC_LIM)
    ("-equihashsolver", CONF_EQUIHASH_SOLVER)
    ("-mineraddress", CONF_MINER_ADDRESS)
    ("-minetolocalwallet", CONF_MINE_TO_LOCAL_WALLET)
    ("-server", CONF_SERVER)
    ("-rest", CONF_REST)
    ("-rpcbind", CONF_RPC_BIND)
    ("-rpcuser", CONF_RPC_USER)
    ("-rpcpassword", CONF_RPC_PASSWORD)
    ("-rpcport", CONF_RPC_PORT)
    ("-rpcallowip", CONF_RPC_ALLOW_IP)
    ("-rpcthreads", CONF_RPC_THREADS)
    ("-rpcworkqueue", CONF_RPC_WORKQUEUE)
    ("-rpcclienttimeout", CONF_RPC_CLIENT_TIMEOUT)
    ("-rpcservertimeout", CONF_RPC_SERVER_TIMEOUT)
    ("-showmetrics", CONF_SHOW_METRICS)
    ("-metricsui", CONF_METRICS_UI)
    ("-metricsrefreshtime", CONF_METRICS_REFRESH_TIME)

    ("-stdin", CONF_STDIN)
    ("-json", CONF_JSON)
    ("-txid", CONF_TXID)
    ("-create", CONF_CREATE)
    ("-nodebug", CONF_NODEBUG)
    ("-checkblockindex", CONF_CHECK_BLOCK_INDEX)
    ("-rpcwait", CONF_RPC_WAIT)
    ("-rpcssl", CONF_RPC_SSL)
    ("-rpccookiefile", CONF_RPC_COOKIE)
    ("-zmq", CONF_ZMQ)
    ("-mocktime", CONF_MOCKTIME)
    ("-sendalert", CONF_ALERT_SEND)
    ("-printalert", CONF_ALERT_PRINT)
    ("-developersetpoolsizezero", CONF_DEV_SET_POOL_ZERO)
    ("-regtestprotectcoinbase", CONF_REG_TEST_COINBASE)
    ("-uacomment", CONF_UACOMMENT)
    ("-checkmempool", CONF_CHECK_MEMPOOL)
    ("-developerencryptwallet", CONF_DEV_ENCRYPT_WALLET)
    ("-developersetpoolsizezero", CONF_DEV_SETPOOL_ZERO)
    ("-paymentdisclosure", CONF_PAYMENT_DISCLOSURE)
    ("-insightexplorer", CONF_INSIGHT_EXPLORER);