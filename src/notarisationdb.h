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

NotarisationsInBlock ScanBlockNotarisations(const CBlock &block, int nHeight);
bool GetBlockNotarisations(uint256 blockHash, NotarisationsInBlock &nibs);
bool GetBackNotarisation(uint256 notarisationHash, Notarisation &n);
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CLevelDBBatch &batch);
void EraseBackNotarisations(const NotarisationsInBlock notarisations, CLevelDBBatch &batch);

#endif  /* NOTARISATIONDB_H */
