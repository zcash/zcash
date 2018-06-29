#ifndef TESTUTILS_H
#define TESTUTILS_H

#include "main.h"


#define VCH(a,b) std::vector<unsigned char>(a, a + b)

static char ccjsonerr[1000] = "\0";
#define CCFromJson(o,s) \
    o = cc_conditionFromJSONString(s, ccjsonerr); \
    if (!o) FAIL() << "bad json: " << ccjsonerr;


extern std::string notaryPubkey;
extern std::string notarySecret;
extern CKey notaryKey;


void setupChain();
void generateBlock(CBlock *block=NULL);
bool acceptTx(const CTransaction tx, CValidationState &state);
void acceptTxFail(const CTransaction tx);
void getInputTx(CScript scriptPubKey, CTransaction &txIn);
CMutableTransaction spendTx(const CTransaction &txIn, int nOut=0);
std::vector<uint8_t> getSig(const CMutableTransaction mtx, CScript inputPubKey, int nIn=0);


#endif /* TESTUTILS_H */
