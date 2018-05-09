#ifndef NOTARISATIONDB_H
#define NOTARISATIONDB_H

#include "uint256.h"
#include "leveldbwrapper.h"
#include "cc/eval.h"


class NotarisationDB : public CLevelDBWrapper
{
public:
    NotarisationDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
};


extern NotarisationDB *pnotarisations;


typedef std::pair<uint256,NotarisationData> Notarisation;
typedef std::vector<Notarisation> NotarisationsInBlock;

NotarisationsInBlock GetNotarisationsInBlock(const CBlock &block, int nHeight);

void WriteBackNotarisations(NotarisationsInBlock notarisations);

#endif  /* NOTARISATIONDB_H */
