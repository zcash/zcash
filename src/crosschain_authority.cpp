#include "cc/eval.h"
#include "crosschain.h"
#include "notarisationdb.h"
//#include "notaries_STAKED.h"

const char *notaries_STAKEDcc[][2] =
{
      {"alright", "037fa9d151c8bafd67c1829707b9d72e193e8c27c7b284fef1ffe2070601e508be"},
      {"webworker01_NA", "03bb7d005e052779b1586f071834c5facbb83470094cff5112f0072b64989f97d7" },
      {"CrisF", "03921b5e03b3b4d31f94f6ad40b21170a524ab9f2f344bb575811aeca059ca174b"},
      {"Emman", "038F642DCDACBDF510B7869D74544DBC6792548D9D1F8D73A999DD9F45F513C935"},
      {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943"},
      {"xrobesx", "020a403b030211c87c77fc3d397db91313d96b3b43b86ca15cb08481508a7f754f"},
      {"jorian","02150c410a606b898bcab4f083e48e0f98a510e0d48d4db367d37f318d26ae72e3"},
      {"gcharang","024ce12f3423345350d8535e402803da30abee3c2941840b5002bf05e88b7f6e2d"},
      {"nabob","03ee91c20b6d26e3604022f42df6bb8de6f669da4591f93b568778cba13d9e9ddf"},
      {"TonyL","02f5988bda204b42fd451163257ff409d11dcbd818eb2d96ab6a72382eff2b2622"},
      {"blackjok3r","021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e"}
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
