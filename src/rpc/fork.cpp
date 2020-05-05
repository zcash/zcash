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

        // check for duplicates
        bool fDup = false;
        for (int i = 0; i < mapBlocksCSV[string(sHeightBuffer)].size(); i ++)
        {
            if (mapBlocksCSV[string(sHeightBuffer)][i] == string(sHashBuffer))
            {
                fDup = true;
                break;
            }
        }
        if (!fDup)
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

UniValue detectdoublespends(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "detectdoublespends\n"
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

        // check for duplicates
        bool fDup = false;
        for (int i = 0; i < mapBlocksCSV[string(sHeightBuffer)].size(); i ++)
        {
            if (mapBlocksCSV[string(sHeightBuffer)][i] == string(sHashBuffer))
            {
                fDup = true;
                break;
            }
        }
        if (!fDup)
            mapBlocksCSV[string(sHeightBuffer)].push_back(string(sHashBuffer));
    }
    fclose(fBlockCSVFile);

    // TODO refactor
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
            
            uint256** vTXHashes = (uint256**)calloc(nHashes, sizeof(uint256*));         // [block][tx]
            int* nTXHashes = (int*)calloc(nHashes, sizeof(int));                        // [block]
            uint256*** vTXInHashes = (uint256***)calloc(nHashes, sizeof(uint256**));    // [block][tx][in]
            int** nTXInHashes = (int**)calloc(nHashes, sizeof(int*));                   // [block][tx]

            for (int i = 0; i < nHashes; i ++)
            {
                uint256 hash(uint256S(vHashes[i]));
                if (mapBlockIndex.count(hash) == 0)
                {
                    // hash not found in global index
                    nTXHashes[i] = -1;

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
                
                vTXHashes[i] = (uint256*)calloc(block.vtx.size() - 1, sizeof(uint256));
                nTXHashes[i] = block.vtx.size();
                vTXInHashes[i] = (uint256**)calloc(block.vtx.size() - 1, sizeof(uint256*));
                nTXInHashes[i] = (int*)calloc(block.vtx.size() - 1, sizeof(int));

                // start index at 1
                // ignore coinbase
                for (int tx_i = 1; tx_i < nTXHashes[i]; tx_i ++)
                {
                    vTXHashes[i][tx_i - 1] = block.vtx[tx_i].GetHash();

                    // inputs
                    vTXInHashes[i][tx_i - 1] = (uint256*)calloc(block.vtx[tx_i].vin.size(), sizeof(uint256));
                    nTXInHashes[i][tx_i - 1] = block.vtx[tx_i].vin.size();
                    for (int txin_i = 0; txin_i < nTXInHashes[i][tx_i - 1]; txin_i ++)
                    {
                        vTXInHashes[i][tx_i - 1][txin_i] = block.vtx[tx_i].vin[txin_i].prevout.hash;
                    }
                }
                nTXHashes[i] -= 1;
            }

            for (int block_i = 0; block_i < nHashes; block_i ++)
            {
                // no block found
                if (nTXHashes[block_i] == -1)
                    continue;

                UniValue oBlock(UniValue::VOBJ);
                oBlock.push_back(Pair("hash", vHashes[block_i]));

                UniValue oTXs(UniValue::VARR);
                for (int tx_i = 0; tx_i < nTXHashes[block_i]; tx_i ++)
                {
                    bool fUnique = true;
                    for (int block_j = 0; block_j < nHashes; block_j ++)
                    {
                        if (block_i == block_j)
                        {
                            continue;
                        }
                        for (int tx_j = 0; tx_j < nTXHashes[block_j]; tx_j ++)
                        {
                            if (vTXHashes[block_i][tx_i] == vTXHashes[block_j][tx_j])
                            {
                                fUnique = false;
                            }
                        }
                    }
                    if (fUnique)
                    {
                        UniValue oTX(UniValue::VOBJ);
                        oTX.push_back(Pair("hash", vTXHashes[block_i][tx_i].GetHex())); 
    
                        UniValue oTXIns(UniValue::VARR);
                        for (int txin_i = 0; txin_i < nTXInHashes[block_i][tx_i]; txin_i ++)
                        {
                            oTXIns.push_back(vTXInHashes[block_i][tx_i][txin_i].GetHex());
                        }
                        oTX.push_back(Pair("vin", oTXIns));

                        oTXs.push_back(oTX);
                    }
                }

                oBlock.push_back(Pair("unique_transactions", oTXs));

                oBlocks.push_back(oBlock);
            }

            oFork.push_back(Pair("blocks", oBlocks));
            oResult.push_back(oFork);

            // clean up
            free(nTXHashes);
            for (int i = 0; i < nHashes; i ++)
                free(vTXHashes[i]);
            free(vTXHashes);
            //TODO
        }
    }

    return oResult;
}

UniValue detectselfishmining(const UniValue& params, bool fHelp)
{
    if (fHelp)
        throw runtime_error(
            "detectselfishmining\n"
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

    // hash -> time
    map<string, double> mapBlockTimeCSV;
    double nAverageBlockTime = 0.0;
    double nLastBlockTime = 0.0;
    int nBlocks = 0;

    // read block file
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
        c_i++;

        // read time
        int t_i = 0;
        char sTimeBuffer[15];
        while (sBlockCSVBuffer[c_i] != '\n')
        {
            sTimeBuffer[t_i++] = sBlockCSVBuffer[c_i++];
        }
        sTimeBuffer[t_i] = '\0';

        // check for duplicates
        bool fDup = false;
        for (int i = 0; i < mapBlocksCSV[string(sHeightBuffer)].size(); i ++)
        {
            if (mapBlocksCSV[string(sHeightBuffer)][i] == string(sHashBuffer))
            {
                fDup = true;
                break;
            }
        }
        if (!fDup)
        {
            mapBlocksCSV[string(sHeightBuffer)].push_back(string(sHashBuffer));
            mapBlockTimeCSV[string(sHashBuffer)] = atof(sTimeBuffer);

            if (mapBlocksCSV[string(sHeightBuffer)].size() == 1)
            {
                if (nLastBlockTime == 0.0)
                {
                    nLastBlockTime = atof(sTimeBuffer);
                    continue;
                }
                // first block with height
                nBlocks ++;

                double nTime = atof(sTimeBuffer) - nLastBlockTime;
                nLastBlockTime = atof(sTimeBuffer);

                nAverageBlockTime = (nAverageBlockTime * (nBlocks - 1) + nTime)/nBlocks;
            }
        }
    }
    fclose(fBlockCSVFile);

    // open inv file
    boost::filesystem::path sInvCSVPath = GetDataDir() / "inv_v1.csv";
    FILE *fInvCSVFile = fopen(sInvCSVPath.string().c_str(), "r");
    if (fInvCSVFile == NULL)
    {
        throw JSONRPCError(RPC_UNREACHABLE_FILE, "Cannot open file {datadir}/inv_v1.csv");
    }

    char sInvCSVBuffer[128];

    // hash -> time
    // time for a block to reach all peers
    map<string, double> mapBlockTimeFirst;
    map<string, double> mapBlockTimeLast;

    while (fgets(sInvCSVBuffer, sizeof(sInvCSVBuffer), fInvCSVFile) != NULL)
    {
        // skip header line
        // always starts with 'Hash'
        if (sInvCSVBuffer[0] == 'H')
        {
            continue;
        }
        int c_i = 0;

        // read hash
        int s_i = 0;
        char sHashBuffer[100];
        while (sInvCSVBuffer[c_i] != ',')
        {
            sHashBuffer[s_i++] = sInvCSVBuffer[c_i++];
        }
        sHashBuffer[s_i] = '\0';
        c_i++;
        string sHash = string(sHashBuffer);

        // read IP
        // NOTE ignore IP
        while (sInvCSVBuffer[c_i] != ',')
        {
            c_i ++;
        }
        c_i ++;

        // read time
        int t_i = 0;
        char sTimeBuffer[15];
        while (sInvCSVBuffer[c_i] != '\n')
        {
            sTimeBuffer[t_i++] = sInvCSVBuffer[c_i++];
        }
        sTimeBuffer[t_i] = '\0';
        double nTime = atof(sTimeBuffer);

        // first timestamp
        if (mapBlockTimeFirst.count(sHash) == 0 &&
                mapBlockTimeLast.count(sHash) == 0)
        {
            mapBlockTimeFirst[sHash] = nTime;
            mapBlockTimeLast[sHash] = nTime;
            continue;
        }

        // edit maps
        if (mapBlockTimeFirst[sHash] > nTime)
        {
            mapBlockTimeFirst[sHash] = nTime;
            continue;
        }
        if (mapBlockTimeLast[sHash] < nTime)
        {
            mapBlockTimeLast[sHash] = nTime;
            continue;
        }
    }

    double nGlobalLatency = 0.0;            // average time for a block to reach all peers
    int l_i = 0;
    for (map<string,double>::iterator it = mapBlockTimeFirst.begin(); it != mapBlockTimeFirst.end(); it++)
    {
        string sHash = it->first;
        double nFirstTime = it->second;
        double nLastTime = mapBlockTimeLast[sHash];

        double nLatancey = nLastTime - nFirstTime;

        l_i ++;
        nGlobalLatency = (nGlobalLatency * (l_i - 1) + nLatancey)/l_i;
    }
    //printf("GLOBAL LACENCEY: %.3f\n", nGlobalLatency);

    // find forks
    UniValue oResult(UniValue::VARR);
    for (map<string,vector<string>>::iterator it = mapBlocksCSV.begin(); it != mapBlocksCSV.end(); it++)
    {
        string sHeight = it->first;
        vector<string> vHashes = it->second;

        int nHashes = vHashes.size();
        if (nHashes > 1)
        {
            // block that resolved the fork
            string sResolverHash = mapBlocksCSV[to_string(stoi(sHeight) + 1)][0];
            uint256 nResolverHash(uint256S(sResolverHash));

            CBlockIndex* pResolverBlockIndex = mapBlockIndex[nResolverHash];
            
            double nResolvedTime = mapBlockTimeCSV[sResolverHash] - mapBlockTimeCSV[pResolverBlockIndex->pprev->phashBlock->GetHex()];

            if (nResolvedTime < (70 / 4))
            {
                UniValue oFork(UniValue::VOBJ);
                oFork.push_back(Pair("height", sHeight));

                UniValue oBlocks(UniValue::VARR);
                for (int i = 0; i < nHashes; i ++)
                {
                    uint256 hash(uint256S(vHashes[i]));

                    UniValue oBlock(UniValue::VOBJ);
                    oBlock.push_back(Pair("hash", hash.GetHex()));
                    oBlock.push_back(Pair("networktime", mapBlockTimeCSV[vHashes[i]]));

                    oBlocks.push_back(oBlock);
                }

                oFork.push_back(Pair("blocks", oBlocks));

                UniValue oResolver(UniValue::VOBJ);
                oResolver.push_back(Pair("hash", sResolverHash));
                oResolver.push_back(Pair("prevhash", pResolverBlockIndex->pprev->phashBlock->GetHex()));
                oResolver.push_back(Pair("networktime", mapBlockTimeCSV[sResolverHash]));
                oResolver.push_back(Pair("resolvedtime", nResolvedTime));

                oFork.push_back(Pair("resolver", oResolver));

                oResult.push_back(oFork);
            }
        }
    }

    return oResult;

}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         okSafeMode
  //  --------------------- ------------------------  -----------------------  ----------
    { "fork",               "listforks",              &listforks,              true         },
    { "fork",               "detectdoublespends",     &detectdoublespends,     true         },
    { "fork",               "detectselfishmining",    &detectselfishmining,    true         },
};

void RegisterForkRPCCommands(CRPCTable &tableRPC)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        tableRPC.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
