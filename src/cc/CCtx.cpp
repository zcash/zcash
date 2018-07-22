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
 FinalizeCCTx is a very useful function that will properly sign both CC and normal inputs, adds normal change and the opreturn.
 
 This allows the contract transaction functions to create the appropriate vins and vouts and have FinalizeCCTx create a properly signed transaction.
 
 By using -addressindex=1, it allows tracking of all the CC addresses
 */
 
bool SignTx(CMutableTransaction &mtx,int32_t vini,uint64_t utxovalue,const CScript scriptPubKey)
{
#ifdef ENABLE_WALLET
    CTransaction txNewConst(mtx); SignatureData sigdata; const CKeyStore& keystore = *pwalletMain;
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,vini,utxovalue,SIGHASH_ALL),scriptPubKey,sigdata,consensusBranchId) != 0 )
    {
        UpdateTransaction(mtx,vini,sigdata);
        return(true);
    } else fprintf(stderr,"signing error for CreateAsset vini.%d %.8f\n",vini,(double)utxovalue/COIN);
#else
    return(false);
#endif
}

std::string FinalizeCCTx(uint8_t evalcode,CMutableTransaction &mtx,CPubKey mypk,uint64_t txfee,CScript opret)
{
    CTransaction vintx; std::string hex; uint256 hashBlock; uint64_t vinimask=0,utxovalues[64],change,totaloutputs=0,totalinputs=0; int32_t i,utxovout,n,err = 0; char myaddr[64],destaddr[64],unspendable[64]; uint8_t *privkey,myprivkey[32],unspendablepriv[32],*msg32 = 0; CC *mycond=0,*othercond=0,*cond; CPubKey unspendablepk;
    n = mtx.vout.size();
    for (i=0; i<n; i++)
    {
        //if ( mtx.vout[i].scriptPubKey.IsPayToCryptoCondition() == 0 )
            totaloutputs += mtx.vout[i].nValue;
    }
    if ( (n= mtx.vin.size()) > 64 )
    {
        fprintf(stderr,"FinalizeAssetTx: %d is too many vins\n",n);
        return(0);
    }
    Myprivkey(myprivkey);
    unspendablepk = GetUnspendable(evalcode,unspendablepriv);
    GetCCaddress(evalcode,myaddr,mypk);
    mycond = MakeCC(evalcode,mypk);
    GetCCaddress(evalcode,unspendable,unspendablepk);
    othercond = MakeCC(evalcode,unspendablepk);
    //fprintf(stderr,"myCCaddr.(%s) %p vs unspendable.(%s) %p\n",myaddr,mycond,unspendable,othercond);
    memset(utxovalues,0,sizeof(utxovalues));
    for (i=0; i<n; i++)
    {
        if ( GetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock,false) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            utxovalues[i] = vintx.vout[utxovout].nValue;
            totalinputs += utxovalues[i];
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                //fprintf(stderr,"vin.%d is normal %.8f\n",i,(double)utxovalues[i]/COIN);
                vinimask |= (1LL << i);
            }
            else
            {
            }
        } else fprintf(stderr,"FinalizeCCTx couldnt find %s\n",mtx.vin[i].prevout.hash.ToString().c_str());
    }
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    }
    mtx.vout.push_back(CTxOut(0,opret));
    PrecomputedTransactionData txdata(mtx);
    n = mtx.vin.size();
    for (i=0; i<n; i++)
    {
        if ( GetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock,false) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                if ( SignTx(mtx,i,vintx.vout[utxovout].nValue,vintx.vout[utxovout].scriptPubKey) == 0 )
                    fprintf(stderr,"signing error for vini.%d of %llx\n",i,(long long)vinimask);
            }
            else
            {
                Getscriptaddress(destaddr,vintx.vout[utxovout].scriptPubKey);
                //fprintf(stderr,"vin.%d is CC %.8f -> (%s)\n",i,(double)utxovalues[i]/COIN,destaddr);
                if ( strcmp(destaddr,myaddr) == 0 )
                {
                    privkey = myprivkey;
                    cond = mycond;
                    //fprintf(stderr,"my CC addr.(%s)\n",myaddr);
                }
                else if ( strcmp(destaddr,unspendable) == 0 )
                {
                    privkey = unspendablepriv;
                    cond = othercond;
                    //fprintf(stderr,"unspendable CC addr.(%s)\n",unspendable);
                }
                else
                {
                    fprintf(stderr,"vini.%d has unknown CC address.(%s)\n",i,destaddr);
                    continue;
                }
                uint256 sighash = SignatureHash(CCPubKey(cond), mtx, i, SIGHASH_ALL, 0, 0, &txdata);
                if ( cc_signTreeSecp256k1Msg32(cond,privkey,sighash.begin()) != 0 )
                    mtx.vin[i].scriptSig = CCSig(cond);
                else fprintf(stderr,"vini.%d has CC signing error address.(%s)\n",i,destaddr);
            }
        } else fprintf(stderr,"FinalizeAssetTx couldnt find %s\n",mtx.vin[i].prevout.hash.ToString().c_str());
    }
    if ( mycond != 0 )
        cc_free(mycond);
    if ( othercond != 0 )
        cc_free(othercond);
    std::string strHex = EncodeHexTx(mtx);
    if ( strHex.size() > 0 )
        return(strHex);
    else return(0);
}

void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr)
{
    int32_t type=0,i,n; char *ptr; std::string addrstr; uint160 hashBytes; std::vector<std::pair<uint160, int> > addresses;
    n = (int32_t)strlen(coinaddr);
    addrstr.resize(n+1);
    ptr = (char *)addrstr.data();
    for (i=0; i<=n; i++)
        ptr[i] = coinaddr[i];
    CBitcoinAddress address(addrstr);
    if ( address.GetIndexKey(hashBytes, type) == 0 )
        return;
    addresses.push_back(std::make_pair(hashBytes,type));
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++)
    {
        if ( GetAddressUnspent((*it).first, (*it).second, unspentOutputs) == 0 )
            return;
    }
}

uint64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,uint64_t total,int32_t maxinputs)
{
    int32_t n = 0; uint64_t nValue,totalinputs = 0; std::vector<COutput> vecOutputs;
#ifdef ENABLE_WALLET
    const CKeyStore& keystore = *pwalletMain;
    assert(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if ( out.fSpendable != 0 )
        {
            mtx.vin.push_back(CTxIn(out.tx->GetHash(),out.i,CScript()));
            nValue = out.tx->vout[out.i].nValue;
            totalinputs += nValue;
            n++;
            if ( totalinputs >= total || n >= maxinputs )
                break;
        }
    }
    if ( totalinputs >= total )
        return(totalinputs);
#endif
    return(0);
}
