#include "leveldbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"
#include "cc/eval.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "notarisations", nCacheSize, fMemory, fWipe, false, 64) { }


NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight)
{
    EvalRef eval;
    NotarisationsInBlock vNotarisations;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        CTransaction tx = block.vtx[i];

        // Special case for TXSCL. Should prob be removed at some point.
        bool isTxscl = 0;
        {
            NotarisationData data;
            if (ParseNotarisationOpReturn(tx, data))
                if (strlen(data.symbol) >= 5 && strncmp(data.symbol, "TXSCL", 5) == 0)
                    isTxscl = 1;
        }

        if (isTxscl || eval->CheckNotaryInputs(tx, nHeight, block.nTime)) {
            NotarisationData data;
            if (ParseNotarisationOpReturn(tx, data)) {
                vNotarisations.push_back(std::make_pair(tx.GetHash(), data));
                //printf("Parsed a notarisation for: %s, txid:%s, ccid:%i, momdepth:%i\n",
                //      data.symbol, tx.GetHash().GetHex().data(), data.ccId, data.MoMDepth);
                //if (!data.MoMoM.IsNull()) printf("MoMoM:%s\n", data.MoMoM.GetHex().data());
            }
            else
                LogPrintf("WARNING: Couldn't parse notarisation for tx: %s at height %i\n",
                        tx.GetHash().GetHex().data(), nHeight);
        }
    }
    return vNotarisations;
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
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CLevelDBBatch &batch)
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


void EraseBackNotarisations(const NotarisationsInBlock notarisations, CLevelDBBatch &batch)
{
    BOOST_FOREACH(const Notarisation &n, notarisations)
    {
        if (!n.second.txHash.IsNull())
            batch.Erase(n.second.txHash);
    }
}
