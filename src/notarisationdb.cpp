#include "leveldbwrapper.h"
#include "notarisationdb.h"
#include "uint256.h"
#include "cc/eval.h"

#include <boost/foreach.hpp>


NotarisationDB *pnotarisations;


NotarisationDB::NotarisationDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "notarisations", nCacheSize, fMemory, fWipe, false, 64) { }


NotarisationsInBlock GetNotarisationsInBlock(const CBlock &block, int nHeight)
{
    EvalRef eval;
    NotarisationsInBlock vNotarisations;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        CTransaction tx = block.vtx[i];
        if (eval->CheckNotaryInputs(tx, nHeight, block.nTime)) {
            NotarisationData data;
            if (ParseNotarisationOpReturn(tx, data))
                vNotarisations.push_back(std::make_pair(tx.GetHash(), data));
            else
                fprintf(stderr, "Warning: Couldn't parse notarisation for tx: %s at height %i\n",
                        tx.GetHash().GetHex().data(), nHeight);
        }
    }
    return vNotarisations;
}


/*
 * Write an index of KMD notarisation id -> backnotarisation
 */
void WriteBackNotarisations(NotarisationsInBlock notarisations)
{
    BOOST_FOREACH(Notarisation &n, notarisations)
    {
        if (n.second.IsBackNotarisation) {
            pnotarisations->Write(n.second.txHash, n);
            printf("WriteBackNotarisations {\n  m3:%s\n}\n", n.second.MoMoM.GetHex().data());
        }
    }
}
