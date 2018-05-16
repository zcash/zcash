#ifndef CROSSCHAIN_H
#define CROSSCHAIN_H

#include "cc/eval.h"


/* On assetchain */
std::pair<uint256,MerkleBranch> GetAssetchainProof(uint256 hash, int &npIdx);

/* On KMD */
uint256 GetProofRoot(char* symbol, uint32_t targetCCid, int kmdHeight, std::vector<uint256> &moms, int* assetChainHeight);

/* On KMD */
MerkleBranch GetCrossChainProof(uint256 txid, char* targetSymbol,
        uint32_t targetCCid, uint256 notarisationTxid, MerkleBranch assetChainProof);

/* On assetchain */
bool ValidateCrossChainProof(uint256 txid, int notarisationHeight, MerkleBranch proof);


#endif /* CROSSCHAIN_H */
