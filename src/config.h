// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <boost/assign/list_of.hpp>
#include <boost/unordered_map.hpp>
#include <string>

using boost::assign::map_list_of;

enum ConfigOption {
    CONF_ADDNODE,

    CONF_SERVER,
    CONF_REST,
    CONF_RPC_BIND,
    CONF_RPC_USER,
    CONF_RPC_PASSWORD,
    CONF_RPC_PORT,
    CONF_RPC_ALLOW_IP,
    CONF_RPC_THREADS,
    CONF_RPC_WORKQUEUE,
    CONF_RPC_SERVER_TIMEOUT,
};

const boost::unordered_map<std::string,ConfigOption> FlagToConfigOption = map_list_of
    ("-addnode", CONF_ADDNODE)
    ("-server", CONF_SERVER)
    ("-rest", CONF_REST)
    ("-rpcbind", CONF_RPC_BIND)
    ("-rpcuser", CONF_RPC_USER)
    ("-rpcpassword", CONF_RPC_PASSWORD)
    ("-rpcport", CONF_RPC_PORT)
    ("-rpcallowip", CONF_RPC_ALLOW_IP)
    ("-rpcthreads", CONF_RPC_THREADS)
    ("-rpcworkqueue", CONF_RPC_WORKQUEUE)
    ("-rpcservertimeout", CONF_RPC_SERVER_TIMEOUT);