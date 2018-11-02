#include "../main.h"
bool Getscriptaddress(char *destaddr,const CScript &scriptPubKey)
{
    CTxDestination address; txnouttype whichType;
    if ( ExtractDestination(scriptPubKey,address) != 0 )
    {
        strcpy(destaddr,(char *)CBitcoinAddress(address).ToString().c_str());
        return(true);
    }
    fprintf(stderr,"ExtractDestination failed\n");
    return(false);
}

bool pubkey2addr(char *destaddr,uint8_t *pubkey33)
{
    std::vector<uint8_t>pk; int32_t i;
    for (i=0; i<33; i++)
        pk.push_back(pubkey33[i]);
    return(Getscriptaddress(destaddr,CScript() << pk << OP_CHECKSIG));
}
