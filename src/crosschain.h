#ifndef CROSSCHAIN_H
#define CROSSCHAIN_H

#include "cc/eval.h"


/* On assetchain */
TxProof GetAssetchainProof(uint256 hash);

/* On KMD */
uint256 CalculateProofRoot(const char* symbol, uint32_t targetCCid, int kmdHeight,
        std::vector<uint256> &moms, uint256 &destNotarisationTxid);
TxProof GetCrossChainProof(const uint256 txid, const char* targetSymbol, uint32_t targetCCid,
        const TxProof assetChainProof);
void CompleteImportTransaction(CTransaction &importTx);

/* On assetchain */
bool GetNextBacknotarisation(uint256 txid, std::pair<uint256,NotarisationData> &bn);


#endif /* CROSSCHAIN_H */
