// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2018-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_RPC_REGISTER_H
#define BITCOIN_RPC_REGISTER_H

/** These are in one header file to avoid creating tons of single-function
 * headers for everything under src/rpc/ */
class CRPCTable;

/** Register block chain RPC commands */
void RegisterBlockchainRPCCommands(CRPCTable &rpcTable);
/** Register P2P networking RPC commands */
void RegisterNetRPCCommands(CRPCTable &rpcTable);
/** Register miscellaneous RPC commands */
void RegisterMiscRPCCommands(CRPCTable &rpcTable);
/** Register mining RPC commands */
void RegisterMiningRPCCommands(CRPCTable &rpcTable);
/** Register raw transaction RPC commands */
void RegisterRawTransactionRPCCommands(CRPCTable &rpcTable);

static inline void RegisterAllCoreRPCCommands(CRPCTable &rpcTable)
{
    RegisterBlockchainRPCCommands(rpcTable);
    RegisterNetRPCCommands(rpcTable);
    RegisterMiscRPCCommands(rpcTable);
    RegisterMiningRPCCommands(rpcTable);
    RegisterRawTransactionRPCCommands(rpcTable);
}

#endif // BITCOIN_RPC_REGISTER_H
