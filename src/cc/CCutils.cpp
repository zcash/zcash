/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "CCinclude.h"

/*
 CCutils has low level functions that are universally useful for all contracts.
 */

CTxOut MakeCC1vout(uint8_t evalcode,CAmount nValue,CPubKey pk)
{
    CTxOut vout;
    CC *payoutCond = MakeCCcond1(evalcode,pk);
    vout = CTxOut(nValue,CCPubKey(payoutCond));
    cc_free(payoutCond);
    return(vout);
}

CC *MakeCCcond1(uint8_t evalcode,CPubKey pk)
{
    std::vector<CC*> pks;
    pks.push_back(CCNewSecp256k1(pk));
    CC *condCC = CCNewEval(E_MARSHAL(ss << evalcode));
    CC *Sig = CCNewThreshold(1, pks);
    return CCNewThreshold(2, {condCC, Sig});
}

CC* GetCryptoCondition(CScript const& scriptSig)
{
    auto pc = scriptSig.begin();
    opcodetype opcode;
    std::vector<unsigned char> ffbin;
    if (scriptSig.GetOp(pc, opcode, ffbin))
        return cc_readFulfillmentBinary((uint8_t*)ffbin.data(), ffbin.size()-1);
}

bool IsCCInput(CScript const& scriptSig)
{
    CC *cond;
    if ( (cond= GetCryptoCondition(scriptSig)) == 0 )
        return false;
    cc_free(cond);
    return true;
}

int32_t unstringbits(char *buf,uint64_t bits)
{
    int32_t i;
    for (i=0; i<8; i++,bits>>=8)
        if ( (buf[i]= (char)(bits & 0xff)) == 0 )
            break;
    buf[i] = 0;
    return(i);
}

uint64_t stringbits(char *str)
{
    uint64_t bits = 0;
    if ( str == 0 )
        return(0);
    int32_t i,n = (int32_t)strlen(str);
    if ( n > 8 )
        n = 8;
    for (i=n-1; i>=0; i--)
        bits = (bits << 8) | (str[i] & 0xff);
    //printf("(%s) -> %llx %llu\n",str,(long long)bits,(long long)bits);
    return(bits);
}

uint256 revuint256(uint256 txid)
{
    uint256 revtxid; int32_t i;
    for (i=31; i>=0; i--)
        ((uint8_t *)&revtxid)[31-i] = ((uint8_t *)&txid)[i];
    return(revtxid);
}

char *uint256_str(char *dest,uint256 txid)
{
    int32_t i,j=0;
    for (i=31; i>=0; i--)
        sprintf(&dest[j++ * 2],"%02x",((uint8_t *)&txid)[i]);
    dest[64] = 0;
    return(dest);
}

char *pubkey33_str(char *dest,uint8_t *pubkey33)
{
    int32_t i;
    if ( pubkey33 != 0 )
    {
        for (i=0; i<33; i++)
            sprintf(&dest[i * 2],"%02x",pubkey33[i]);
    } else dest[0] = 0;
    return(dest);
}

uint256 Parseuint256(char *hexstr)
{
    uint256 txid; int32_t i; std::vector<unsigned char> txidbytes(ParseHex(hexstr));
    memset(&txid,0,sizeof(txid));
    if ( strlen(hexstr) == 64 )
    {
        for (i=31; i>=0; i--)
            ((uint8_t *)&txid)[31-i] = ((uint8_t *)txidbytes.data())[i];
    }
    return(txid);
}

CPubKey pubkey2pk(std::vector<uint8_t> pubkey)
{
    CPubKey pk; int32_t i,n; uint8_t *dest,*pubkey33;
    n = pubkey.size();
    dest = (uint8_t *)pk.begin();
    pubkey33 = (uint8_t *)pubkey.data();
    for (i=0; i<n; i++)
        dest[i] = pubkey33[i];
    return(pk);
}

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

bool GetCCaddress(struct CCcontract_info *cp,char *destaddr,CPubKey pk)
{
    CC *payoutCond;
    destaddr[0] = 0;
    if ( pk.size() == 0 )
        pk = GetUnspendable(cp,0);
    if ( (payoutCond= MakeCCcond1(cp->evalcode,pk)) != 0 )
    {
        Getscriptaddress(destaddr,CCPubKey(payoutCond));
        cc_free(payoutCond);
    }
    return(destaddr[0] != 0);
}

bool ConstrainVout(CTxOut vout,int32_t CCflag,char *cmpaddr,uint64_t nValue)
{
    char destaddr[64];
    if ( vout.scriptPubKey.IsPayToCryptoCondition() != CCflag )
    {
        fprintf(stderr,"constrain vout error isCC %d vs %d CCflag\n",vout.scriptPubKey.IsPayToCryptoCondition(),CCflag);
        return(false);
    }
    else if ( cmpaddr != 0 && (Getscriptaddress(destaddr,vout.scriptPubKey) == 0 || strcmp(destaddr,cmpaddr) != 0) )
    {
        fprintf(stderr,"constrain vout error addr %s vs %s\n",cmpaddr!=0?cmpaddr:"",destaddr!=0?destaddr:"");
        return(false);
    }
    else if ( nValue != 0 && nValue != vout.nValue ) //(nValue == 0 && vout.nValue < 10000) || (
    {
        fprintf(stderr,"constrain vout error nValue %.8f vs %.8f\n",(double)nValue/COIN,(double)vout.nValue/COIN);
        return(false);
    }
    else return(true);
}

bool PreventCC(Eval* eval,const CTransaction &tx,int32_t preventCCvins,int32_t numvins,int32_t preventCCvouts,int32_t numvouts)
{
    int32_t i;
    if ( preventCCvins >= 0 )
    {
        for (i=preventCCvins; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[i].scriptSig) != 0 )
                return eval->Invalid("invalid CC vin");
        }
    }
    if ( preventCCvouts >= 0 )
    {
        for (i=preventCCvouts; i<numvouts; i++)
        {
            if ( tx.vout[i].scriptPubKey.IsPayToCryptoCondition() != 0 )
            {
                fprintf(stderr,"vout.%d is CC\n",i);
                return eval->Invalid("invalid CC vout");
            }
        }
    }
    return(true);
}

std::vector<uint8_t> Mypubkey()
{
    extern uint8_t NOTARY_PUBKEY33[33];
    std::vector<uint8_t> pubkey; int32_t i; uint8_t *dest,*pubkey33;
    pubkey33 = NOTARY_PUBKEY33;
    pubkey.resize(33);
    dest = pubkey.data();
    for (i=0; i<33; i++)
        dest[i] = pubkey33[i];
    return(pubkey);
}

bool Myprivkey(uint8_t myprivkey[])
{
    char coinaddr[64]; std::string strAddress; char *dest; int32_t i,n; CBitcoinAddress address; CKeyID keyID; CKey vchSecret;
    if ( Getscriptaddress(coinaddr,CScript() << Mypubkey() << OP_CHECKSIG) != 0 )
    {
        n = (int32_t)strlen(coinaddr);
        strAddress.resize(n+1);
        dest = (char *)strAddress.data();
        for (i=0; i<n; i++)
            dest[i] = coinaddr[i];
        dest[i] = 0;
        if ( address.SetString(strAddress) != 0 && address.GetKeyID(keyID) != 0 )
        {
#ifdef ENABLE_WALLET
            if ( pwalletMain->GetKey(keyID,vchSecret) != 0 )
            {
                memcpy(myprivkey,vchSecret.begin(),32);
                if ( 0 )
                {
                    for (i=0; i<32; i++)
                        fprintf(stderr,"0x%02x, ",myprivkey[i]);
                    fprintf(stderr," found privkey!\n");
                }
                return(true);
            }
#endif
        }
    }
    fprintf(stderr,"privkey for the -pubkey= address is not in the wallet, importprivkey!\n");
    return(false);
}

CPubKey GetUnspendable(struct CCcontract_info *cp,uint8_t *unspendablepriv)
{
    if ( unspendablepriv != 0 )
        memcpy(unspendablepriv,cp->CCpriv,32);
    return(pubkey2pk(ParseHex(cp->CChexstr)));
}

bool ProcessCC(struct CCcontract_info *cp,Eval* eval, std::vector<uint8_t> paramsNull,const CTransaction &ctx, unsigned int nIn)
{
    CTransaction createTx; uint256 assetid,assetid2,hashBlock; uint8_t funcid; int32_t i,n; uint64_t amount; std::vector<uint8_t> origpubkey;
    // there is a chance CC tx is valid in mempool, but invalid when in block, so we cant filter duplicate requests. if any of the vins are spent, for example
    //txid = ctx.GetHash();
    //if ( txid == cp->prevtxid )
    //    return(true);
    //fprintf(stderr,"process CC %02x\n",cp->evalcode);
    if ( paramsNull.size() != 0 ) // Don't expect params
        return eval->Invalid("Cannot have params");
    else if ( ctx.vout.size() == 0 )
        return eval->Invalid("no-vouts");
    else if ( (*cp->validate)(cp,eval,ctx) != 0 )
    {
        //fprintf(stderr,"done CC %02x\n",cp->evalcode);
        //cp->prevtxid = txid;
        return(true);
    }
    //fprintf(stderr,"invalid CC %02x\n",cp->evalcode);
    return(false);
}

int64_t CCduration(int32_t &numblocks,uint256 txid)
{
    CTransaction tx; uint256 hashBlock; uint32_t txheight,txtime=0; char str[65]; CBlockIndex *pindex; int64_t duration = 0;
    numblocks = 0;
    if ( myGetTransaction(txid,tx,hashBlock) == 0 )
    {
        fprintf(stderr,"CCduration cant find duration txid %s\n",uint256_str(str,txid));
        return(0);
    }
    else if ( hashBlock == zeroid )
    {
        fprintf(stderr,"CCduration no hashBlock for txid %s\n",uint256_str(str,txid));
        return(0);
    }
    else if ( (pindex= mapBlockIndex[hashBlock]) == 0 || (txtime= pindex->nTime) == 0 || (txheight= pindex->nHeight) <= 0 )
    {
        fprintf(stderr,"CCduration no txtime %u or txheight.%d %p for txid %s\n",txtime,txheight,pindex,uint256_str(str,txid));
        return(0);
    }
    else if ( (pindex= chainActive.LastTip()) == 0 || pindex->nTime < txtime || pindex->nHeight <= txheight )
    {
        fprintf(stderr,"CCduration backwards timestamps %u %u for txid %s hts.(%d %d)\n",(uint32_t)pindex->nTime,txtime,uint256_str(str,txid),txheight,(int32_t)pindex->nHeight);
        return(0);
    }
    numblocks = (pindex->nHeight - txheight);
    duration = (pindex->nTime - txtime);
    fprintf(stderr,"duration %d (%u - %u) numblocks %d (%d - %d)\n",(int32_t)duration,(uint32_t)pindex->nTime,txtime,numblocks,pindex->nHeight,txheight);
    return(duration);
}

