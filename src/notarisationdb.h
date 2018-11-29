#ifndef NOTARISATIONDB_H
#define NOTARISATIONDB_H

#include "uint256.h"
#include "dbwrapper.h"
#include "cc/eval.h"


class NotarisationDB : public CDBWrapper
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
void WriteBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch);
void EraseBackNotarisations(const NotarisationsInBlock notarisations, CDBBatch &batch);
int ScanNotarisationsDB(int height, std::string symbol, int scanLimitBlocks, Notarisation& out);
bool IsTXSCL(const char* symbol);

#endif  /* NOTARISATIONDB_H */
