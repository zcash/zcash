#include "cc/eval.h"
#include "crosschain.h"
#include "notarisationdb.h"
//#include "notaries_STAKED.h"

const char *notaries_STAKEDcc[][2] =
{
    {"blackjok3r", "021914947402d936a89fbdd1b12be49eb894a1568e5e17bb18c8a6cffbd3dc106e" },
    {"alright", "0285657c689b903218c97f5f10fe1d10ace2ed6595112d9017f54fb42ea1c1dda8" },
    {"webworker01", "031d1fb39ae4dca28965c3abdbd21faa0f685f6d7b87a60561afa7c448343fef6d" },
    {"CrisF", "03f87f1bccb744d90fdbf7fad1515a98e9fc7feb1800e460d2e7565b88c3971bf3" },
    {"daemonfox", "0383484bdc745b2b953c85b5a0c496a1f27bc42ae971f15779ed1532421b3dd943" },
    {"smk762", "020a403b030211c87c77fc3d397db91313d96b3b43b86ca15cb08481508a7f754f" },
    {"jorian", "02150c410a606b898bcab4f083e48e0f98a510e0d48d4db367d37f318d26ae72e3" },
    {"TonyL", "021a559101e355c907d9c553671044d619769a6e71d624f68bfec7d0afa6bd6a96" },
    {"Emman", "038f642dcdacbdf510b7869d74544dbc6792548d9d1f8d73a999dd9f45f513c935" },
    {"titomane", "0222d144bbf15280c574a0ccdc4f390f87779504692fef6e567543c03aa057dfcf" },
    {"CHMEX", "03ed125d1beb118d12ff0a052bdb0cee32591386d718309b2924f2c36b4e7388e6" },
    {"metaphilibert", "0344182c376f054e3755d712361672138660bda8005abb64067eb5aa98bdb40d10" },
    {"jusoaresf", "02dfb7ed72a23f6d07f0ea2f28192ee174733cc8412ec0f97b073007b78fab6346" },
    {"mylo", "03f6b7fcaf0b8b8ec432d0de839a76598b78418dadd50c8e5594c0e557d914ec09" },
    {"ca333", "02f7597468703c1c5c8465dd6d43acaae697df9df30bed21494d193412a1ea193e" },
};


int GetSymbolAuthority(const char* symbol)
{
    if (strlen(symbol) >= 5 && strncmp(symbol, "TXSCL", 5) == 0)
        return CROSSCHAIN_TXSCL;
    if (strlen(symbol) >= 6 && strncmp(symbol, "STKD", 4) == 0)
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
