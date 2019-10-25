#include "dbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"
#include "cc/eval.h"
#include "crosschain.h"
#include "main.h"
#include "notaries_staked.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CDBWrapper(GetDataDir() / "notarisations", nCacheSize, fMemory, fWipe, false, 64) { }


NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight)
{
    EvalRef eval;
    NotarisationsInBlock vNotarisations;
    CrosschainAuthority auth_STAKED;
    int timestamp = block.nTime;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        CTransaction tx = block.vtx[i];

        NotarisationData data;
        bool parsed = ParseNotarisationOpReturn(tx, data);
        if (!parsed) data = NotarisationData();
        if (strlen(data.symbol) == 0)
          continue;

        //printf("Checked notarisation data for %s \n",data.symbol);
        int authority = GetSymbolAuthority(data.symbol);

        if (authority == CROSSCHAIN_KOMODO) {
            if (!eval->CheckNotaryInputs(tx, nHeight, block.nTime))
                continue;
            //printf("Authorised notarisation data for %s \n",data.symbol);
        } else if (authority == CROSSCHAIN_STAKED) {
            // We need to create auth_STAKED dynamically here based on timestamp
            int32_t staked_era = STAKED_era(timestamp);
            if (staked_era == 0) {
              // this is an ERA GAP, so we will ignore this notarization
              continue;
             if ( is_STAKED(data.symbol) == 255 )
              // this chain is banned... we will discard its notarisation. 
              continue;
            } else {
              // pass era slection off to notaries_staked.cpp file
              auth_STAKED = Choose_auth_STAKED(staked_era);
            }
            if (!CheckTxAuthority(tx, auth_STAKED))
                continue;
        }

        if (parsed) {
            vNotarisations.push_back(std::make_pair(tx.GetHash(), data));
            //printf("Parsed a notarisation for: %s, txid:%s, ccid:%i, momdepth:%i\n",
            //      data.symbol, tx.GetHash().GetHex().data(), data.ccId, data.MoMDepth);
            //if (!data.MoMoM.IsNull()) printf("MoMoM:%s\n", data.MoMoM.GetHex().data());
        } else
            LogPrintf("WARNING: Couldn't parse notarisation for tx: %s at height %i\n",
                    tx.GetHash().GetHex().data(), nHeight);
    }
    return vNotarisations;
}

bool IsTXSCL(const char* symbol)
{
    return strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0;
}


bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs)
{
    return pnotarisations->Read(blockHash, nibs);
}


bool GetBackNotarisation(uint256 notarisationHash, Notarisation &n)
{
    return pnotarisations->Read(notarisationHash, n);
}


/*
 * Write an index of KMD notarisation id -> backnotarisation
 */
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch)
{
    int wrote = 0;
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull()) {
            batch.Write(n.second.txHash, n);
            wrote++;
        }
    }
}


void EraseBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch)
{
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull())
            batch.Erase(n.second.txHash);
    }
}

/*
 * Scan notarisationsdb backwards for blocks containing a notarisation
 * for given symbol. Return height of matched notarisation or 0.
 */
int ScanNotarisationsDB(int height, std::string symbol, int scanLimitBlocks, Notarisation& out)
{
    if (height < 0 || height > chainActive.Height())
        return false;

    for (int i=0; i<scanLimitBlocks; i++) {
        if (i > height) break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *chainActive[height-i]->phashBlock;
        if (!GetBlockNotarisations(blockHash, notarisations))
            continue;

        BOOST_FOREACH(Notarisation& nota, notarisations) {
            if (strcmp(nota.second.symbol, symbol.data()) == 0) {
                out = nota;
                return height-i;
            }
        }
    }
    return 0;
}

int ScanNotarisationsDB2(int height, std::string symbol, int scanLimitBlocks, Notarisation& out)
{
    int32_t i,maxheight,ht;
    maxheight = chainActive.Height();
    if ( height < 0 || height > maxheight )
        return false;
    for (i=0; i<scanLimitBlocks; i++)
    {
        ht = height+i;
        if ( ht > maxheight )
            break;
        NotarisationsInBlock notarisations;
        uint256 blockHash = *chainActive[ht]->phashBlock;
        if ( !GetBlockNotarisations(blockHash,notarisations) )
            continue;
        BOOST_FOREACH(Notarisation& nota,notarisations)
        {
            if ( strcmp(nota.second.symbol,symbol.data()) == 0 )
            {
                out = nota;
                return(ht);
            }
        }
    }
    return 0;
}
