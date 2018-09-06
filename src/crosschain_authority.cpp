#include "cc/eval.h"
#include "crosschain.h"
#include "notarisationdb.h"


int GetSymbolAuthority(const char* symbol)
{
    if (strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0)
        return CROSSCHAIN_TXSCL;
    if (strlen(symbol) >= 6 && strncmp(symbol, "STAKED", 6) == 0)
        return CROSSCHAIN_STAKED;
    return CROSSCHAIN_KOMODO;
}


bool CheckTxAuthority(const CTransaction &tx, CrosschainAuthority auth)
{
    EvalRef eval;

    if (tx.vin.size() < auth.requiredSigs) return false;

    uint8_t seen[64] = {0};

    BOOST_FOREACH(const CTxIn &txIn, tx.vin)
    {
        // Get notary pubkey
        CTransaction tx;
        uint256 hashBlock;
        if (!eval->GetTxUnconfirmed(txIn.prevout.hash, tx, hashBlock)) return false;
        if (tx.vout.size() < txIn.prevout.n) return false;
        CScript spk = tx.vout[txIn.prevout.n].scriptPubKey;
        if (spk.size() != 35) return false;
        const unsigned char *pk = spk.data();
        if (pk++[0] != 33) return false;
        if (pk[33] != OP_CHECKSIG) return false;

        // Check it's a notary
        for (int i=0; i<auth.size; i++) {
            if (!seen[i]) {
                if (memcmp(pk, auth.notaries[i], 33) == 0) {
                    seen[i] = 1;
                    goto found;
                }
            }
        }
        return false;
        found:;
    }

    return true;
}


const char *notaries_STAKED[4][2] =
{
    {"alright", "03b4f49a1c22087e0a9dfaa87aef98ef496c544f9f86038f6c9fea4550543a7679"},
    {"alright", "03b4f49a1c22087e0a9dfaa87aef98ef496c544f9f86038f6c9fea4550543a7679"},
    {"alright", "03b4f49a1c22087e0a9dfaa87aef98ef496c544f9f86038f6c9fea4550543a7679"},
    {"alright", "03b4f49a1c22087e0a9dfaa87aef98ef496c544f9f86038f6c9fea4550543a7679"}
};

/*
 * TO EDIT:
 * 1) Edit sigs above
 * 2) Update size and requiredSigs below
 */

const CrosschainAuthority auth_STAKED = [&](){
    CrosschainAuthority auth;
    auth.size = 4;
    auth.requiredSigs = 2;
    for (int n=0; n<auth.size; n++)
        for (size_t i=0; i<33; i++)
            sscanf(notaries_STAKED[n][1]+(i*2), "%2hhx", auth.notaries[n]+i);
    return auth;
}();

