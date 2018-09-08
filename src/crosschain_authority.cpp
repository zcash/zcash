#include "cc/eval.h"
#include "crosschain.h"
#include "notarisationdb.h"
//#include "notaries_STAKED.h"

const char *notaries_STAKEDcc[][2] =
{
    {"alright", "03b4f49a1c22087e0a9dfaa87aef98ef496c544f9f86038f6c9fea4550543a7679"},
    {"test1", "0323b1271dceb046a91e79bf80fc5874fb51b9a5ad572c50ca5f58ee9444b32965"},
    {"test2", "027b45bc21781c287b06b4af48081b49c9ff42cf9e925a8b32dc28a9e85edd2ccd"},
    {"test3", "023142dd900025a812c985e0c8d8730cbe7791126b8ceac71a506eeee1cb4d2633"},
    {"test4", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" },
    {"titomane_EU", "0360b4805d885ff596f94312eed3e4e17cb56aa8077c6dd78d905f8de89da9499f" }, // 60
    {"titomane_SH", "03573713c5b20c1e682a2e8c0f8437625b3530f278e705af9b6614de29277a435b" },
    {"webworker01_NA", "03bb7d005e052779b1586f071834c5facbb83470094cff5112f0072b64989f97d7" },
    {"xrobesx_NA", "03f0cc6d142d14a40937f12dbd99dbd9021328f45759e26f1877f2a838876709e1" },
};


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


const CrosschainAuthority auth_STAKED = [&](){
    CrosschainAuthority auth;
    auth.size = (int32_t)(sizeof(notaries_STAKEDcc)/sizeof(*notaries_STAKEDcc));
    auth.requiredSigs = (auth.size/5);
    for (int n=0; n<auth.size; n++)
        for (size_t i=0; i<33; i++)
            sscanf(notaries_STAKEDcc[n][1]+(i*2), "%2hhx", auth.notaries[n]+i);
    return auth;
}();
