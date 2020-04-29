// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "main.h"
#include "rpc/server.h"

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <vector>
#include <univalue.h>
#include <boost/filesystem.hpp>

// TODO Fix import bug
#define ARRAYLEN(array)     (sizeof(array)/sizeof((array)[0]))

using namespace std;

extern UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails);

UniValue listforks(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "listforks\n"
            "\nResults:\n"
            "[\n"
            "\t{\n"
            "\t\t\"height\" : \"height\",   (numeric) the height at which the fork occured\n"
            "\t\t\"blocks\" : []            (array of objects) full data on blocks starting the fork\n"
            "\t}\n"
            "\t...\n"
            "]\n"
        );

    LOCK(cs_main);

    // open block file
    boost::filesystem::path sBlockCSVPath = GetDataDir() / "blocks_v1.csv";
    FILE *fBlockCSVFile = fopen(sBlockCSVPath.string().c_str(), "r");
    if (fBlockCSVFile == NULL)
    {
        throw JSONRPCError(RPC_UNREACHABLE_FILE, "Cannot open file {datadir}/blocks_v1.csv");
    }

    // height -> hashes[]
    map<string, vector<string>> mapBlocksCSV;

    // read file
    char sBlockCSVBuffer[128];
    while (fgets(sBlockCSVBuffer, sizeof(sBlockCSVBuffer), fBlockCSVFile) != NULL)
    {
        // skip header line
        // always starts with 'Height'
        if (sBlockCSVBuffer[0] == 'H')
        {
            continue;
        }
        int c_i = 0;

        // read height
        int h_i = 0;
        char sHeightBuffer[10];
        while (sBlockCSVBuffer[c_i] != ',')
        {
            sHeightBuffer[h_i++] = sBlockCSVBuffer[c_i++];
        }
        sHeightBuffer[h_i] = '\0';
        c_i ++;

        // read hash
        int s_i = 0;
        char sHashBuffer[65];
        while (sBlockCSVBuffer[c_i] != ',')
        {
            sHashBuffer[s_i++] = sBlockCSVBuffer[c_i++];
        }
        sHashBuffer[s_i] = '\0';

        mapBlocksCSV[string(sHeightBuffer)].push_back(string(sHashBuffer));
    }
    fclose(fBlockCSVFile);

    // find forks
    UniValue oResult(UniValue::VARR);
    for (map<string,vector<string>>::iterator it = mapBlocksCSV.begin(); it != mapBlocksCSV.end(); it++)
    {
        string sHeight = it->first;
        vector<string> vHashes = it->second;

        int nHashes = vHashes.size();
        if (nHashes > 1)
        {
            UniValue oFork(UniValue::VOBJ);
            oFork.push_back(Pair("height", sHeight));

            UniValue oBlocks(UniValue::VARR);
            for (int i = 0; i < nHashes; i ++)
            {
                uint256 hash(uint256S(vHashes[i]));
                if (mapBlockIndex.count(hash) == 0)
                {
                    // hash not found in global index
                    UniValue oBlock(UniValue::VOBJ);
                    oBlock.push_back(Pair("hash", vHashes[i]));
                    oBlock.push_back(Pair("error", "No block found with hash"));
                    oBlocks.push_back(oBlock);
                    continue;
                }

                // read block
                CBlock block;
                CBlockIndex* pblockindex = mapBlockIndex[hash];

                if (fHavePruned && !(pblockindex->nStatus & BLOCK_HAVE_DATA) && pblockindex->nTx > 0)
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Block not available (pruned data)");

                if(!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus()))
                    throw JSONRPCError(RPC_INTERNAL_ERROR, "Can't read block from disk");

                oBlocks.push_back(blockToJSON(block, pblockindex, false));
            }

            oFork.push_back(Pair("blocks", oBlocks));
            oResult.push_back(oFork);
        }
    }

    return oResult;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "fork",               "listforks",              &listforks,              true         },
    { "fork",               "detectdoublespends",     &detectdoublespends,     true         },
};

void RegisterForkRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
