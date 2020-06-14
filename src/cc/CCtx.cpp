/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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
#include "key_io.h"

std::vector<CPubKey> NULL_pubkeys;
struct NSPV_CCmtxinfo NSPV_U;

/* see description to function definition in CCinclude.h */
bool SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey)
{
#ifdef ENABLE_WALLET
    CTransaction txNewConst(mtx); SignatureData sigdata; const CKeyStore& keystore = *pwalletMain;
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    if ( ProduceSignature(TransactionSignatureCreator(&keystore,&txNewConst,vini,utxovalue,SIGHASH_ALL),scriptPubKey,sigdata,consensusBranchId) != 0 )
    {
        UpdateTransaction(mtx,vini,sigdata);
        return(true);
    } else fprintf(stderr,"signing error for SignTx vini.%d %.8f\n",vini,(double)utxovalue/COIN);
#endif
    return(false);
}

/*
FinalizeCCTx is a very useful function that will properly sign both CC and normal inputs, adds normal change and the opreturn.

This allows the contract transaction functions to create the appropriate vins and vouts and have FinalizeCCTx create a properly signed transaction.

By using -addressindex=1, it allows tracking of all the CC addresses
*/
std::string FinalizeCCTx(uint64_t CCmask, struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey mypk, uint64_t txfee, CScript opret, std::vector<CPubKey> pubkeys)
{
    UniValue sigData = FinalizeCCTxExt(false, CCmask, cp, mtx, mypk, txfee, opret, pubkeys);
    return sigData[JSON_HEXTX].getValStr();
}


// extended version that supports signInfo object with conds to vins map for remote cc calls
UniValue FinalizeCCTxExt(bool remote, uint64_t CCmask, struct CCcontract_info *cp, CMutableTransaction &mtx, CPubKey mypk, uint64_t txfee, CScript opret, std::vector<CPubKey> pubkeys)
{
    auto consensusBranchId = CurrentEpochBranchId(chainActive.Height() + 1, Params().GetConsensus());
    CTransaction vintx; std::string hex; CPubKey globalpk; uint256 hashBlock; uint64_t mask=0,nmask=0,vinimask=0;
    int64_t utxovalues[CC_MAXVINS],change,normalinputs=0,totaloutputs=0,normaloutputs=0,totalinputs=0,normalvins=0,ccvins=0; 
    int32_t i,flag,mgret,utxovout,n,err = 0;
	char myaddr[64], destaddr[64], unspendable[64], mytokensaddr[64], mysingletokensaddr[64], unspendabletokensaddr[64],CC1of2CCaddr[64];
    uint8_t *privkey = NULL, myprivkey[32] = { '\0' }, unspendablepriv[32] = { '\0' }, /*tokensunspendablepriv[32],*/ *msg32 = 0;
	CC *mycond=0, *othercond=0, *othercond2=0,*othercond4=0, *othercond3=0, *othercond1of2=NULL, *othercond1of2tokens = NULL, *cond=0,  *condCC2=0,*mytokenscond = NULL, *mysingletokenscond = NULL, *othertokenscond = NULL;
	CPubKey unspendablepk /*, tokensunspendablepk*/;
	struct CCcontract_info *cpTokens, tokensC;
    UniValue sigData(UniValue::VARR),result(UniValue::VOBJ);
    const UniValue sigDataNull = NullUniValue;

    globalpk = GetUnspendable(cp,0);
    n = mtx.vout.size();
    for (i=0; i<n; i++)
    {
        if ( mtx.vout[i].scriptPubKey.IsPayToCryptoCondition() == 0 )
            normaloutputs += mtx.vout[i].nValue;
        totaloutputs += mtx.vout[i].nValue;
    }
    if ( (n= mtx.vin.size()) > CC_MAXVINS )
    {
        fprintf(stderr,"FinalizeCCTx: %d is too many vins\n",n);
        result.push_back(Pair(JSON_HEXTX, "0"));
        return result;
    }

    //Myprivkey(myprivkey);  // for NSPV mode we need to add myprivkey for the explicitly defined mypk param
#ifdef ENABLE_WALLET
    // get privkey for mypk
    CKeyID keyID = mypk.GetID();
    CKey vchSecret;
    if (pwalletMain->GetKey(keyID, vchSecret))
        memcpy(myprivkey, vchSecret.begin(), sizeof(myprivkey));
#endif

    GetCCaddress(cp,myaddr,mypk);
    mycond = MakeCCcond1(cp->evalcode,mypk);

	// to spend from single-eval evalcode 'unspendable' cc addr
	unspendablepk = GetUnspendable(cp, unspendablepriv);
	GetCCaddress(cp, unspendable, unspendablepk);
	othercond = MakeCCcond1(cp->evalcode, unspendablepk);
    GetCCaddress1of2(cp,CC1of2CCaddr,unspendablepk,unspendablepk);

    //fprintf(stderr,"evalcode.%d (%s)\n",cp->evalcode,unspendable);

	// tokens support:
	// to spend from dual/three-eval mypk vout
	GetTokensCCaddress(cp, mytokensaddr, mypk);
    // NOTE: if additionalEvalcode2 is not set it is a dual-eval (not three-eval) cc cond:
	mytokenscond = MakeTokensCCcond1(cp->evalcode, cp->additionalTokensEvalcode2, mypk);  

	// to spend from single-eval EVAL_TOKENS mypk 
	cpTokens = CCinit(&tokensC, EVAL_TOKENS);
	GetCCaddress(cpTokens, mysingletokensaddr, mypk);
	mysingletokenscond = MakeCCcond1(EVAL_TOKENS, mypk);

	// to spend from dual/three-eval EVAL_TOKEN+evalcode 'unspendable' pk:
	GetTokensCCaddress(cp, unspendabletokensaddr, unspendablepk);  // it may be a three-eval cc, if cp->additionalEvalcode2 is set
	othertokenscond = MakeTokensCCcond1(cp->evalcode, cp->additionalTokensEvalcode2, unspendablepk);

    //Reorder vins so that for multiple normal vins all other except vin0 goes to the end
    //This is a must to avoid hardfork change of validation in every CC, because there could be maximum one normal vin at the begining with current validation.
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8)
            continue;
        if ( myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock) != 0 && mtx.vin[i].prevout.n < vintx.vout.size() )
        {
            if ( vintx.vout[mtx.vin[i].prevout.n].scriptPubKey.IsPayToCryptoCondition() == 0 && ccvins==0)
                normalvins++;            
            else ccvins++;
        }
        else
        {
            fprintf(stderr,"vin.%d vout.%d is bigger than vintx.%d\n",i,mtx.vin[i].prevout.n,(int32_t)vintx.vout.size());
            memset(myprivkey,0,32);
            return UniValue(UniValue::VOBJ);
        }
    }
    if (normalvins>1 && ccvins)
    {        
        for(i=1;i<normalvins;i++)
        {   
            mtx.vin.push_back(mtx.vin[1]);
            mtx.vin.erase(mtx.vin.begin() + 1);            
        }
    }
    memset(utxovalues,0,sizeof(utxovalues));
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8) continue;
        if ( (mgret= myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock)) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            utxovalues[i] = vintx.vout[utxovout].nValue;
            totalinputs += utxovalues[i];
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                //fprintf(stderr,"vin.%d is normal %.8f\n",i,(double)utxovalues[i]/COIN);               
                normalinputs += utxovalues[i];
                vinimask |= (1LL << i);
            }
            else
            {                
                mask |= (1LL << i);
            }
        } else fprintf(stderr,"FinalizeCCTx couldnt find %s mgret.%d\n",mtx.vin[i].prevout.hash.ToString().c_str(),mgret);
    }
    nmask = (1LL << n) - 1;
    if ( 0 && (mask & nmask) != (CCmask & nmask) )
        fprintf(stderr,"mask.%llx vs CCmask.%llx %llx %llx %llx\n",(long long)(mask & nmask),(long long)(CCmask & nmask),(long long)mask,(long long)CCmask,(long long)nmask);
    if ( totalinputs >= totaloutputs+2*txfee )
    {
        change = totalinputs - (totaloutputs+txfee);
        mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
    }
    if ( opret.size() > 0 )
        mtx.vout.push_back(CTxOut(0,opret));
    PrecomputedTransactionData txdata(mtx);
    n = mtx.vin.size(); 
    for (i=0; i<n; i++)
    {
        if (i==0 && mtx.vin[i].prevout.n==10e8)
            continue;
        if ( (mgret= myGetTransaction(mtx.vin[i].prevout.hash,vintx,hashBlock)) != 0 )
        {
            utxovout = mtx.vin[i].prevout.n;
            if ( vintx.vout[utxovout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                if ( KOMODO_NSPV_FULLNODE )
                {
                    if (!remote)
                    {
                        if (SignTx(mtx, i, vintx.vout[utxovout].nValue, vintx.vout[utxovout].scriptPubKey) == 0)
                            fprintf(stderr, "signing error for vini.%d of %llx\n", i, (long long)vinimask);
                    }
                    else
                    {
                        // if no myprivkey for mypk it means remote call from nspv superlite client
                        // add sigData for superlite client
                        UniValue cc(UniValue::VNULL);
                        AddSigData2UniValue(sigData, i, cc, HexStr(vintx.vout[utxovout].scriptPubKey), vintx.vout[utxovout].nValue );  // store vin i with scriptPubKey
                    }
                }
                else
                {
                    {
                        char addr[64];
                        Getscriptaddress(addr,vintx.vout[utxovout].scriptPubKey);
                        fprintf(stderr,"vout[%d] %.8f -> %s\n",utxovout,dstr(vintx.vout[utxovout].nValue),addr);
                    }
                    if ( NSPV_SignTx(mtx,i,vintx.vout[utxovout].nValue,vintx.vout[utxovout].scriptPubKey,0) == 0 )
                        fprintf(stderr,"NSPV signing error for vini.%d of %llx\n",i,(long long)vinimask);
                }
            }
            else
            {
                Getscriptaddress(destaddr,vintx.vout[utxovout].scriptPubKey);
                //fprintf(stderr,"FinalizeCCTx() vin.%d is CC %.8f -> (%s) vs %s\n",i,(double)utxovalues[i]/COIN,destaddr,mysingletokensaddr);
				//std::cerr << "FinalizeCCtx() searching destaddr=" << destaddr << " for vin[" << i << "] satoshis=" << utxovalues[i] << std::endl;
                if( strcmp(destaddr, myaddr) == 0 )
                {
//fprintf(stderr, "FinalizeCCTx() matched cc myaddr (%s)\n", myaddr);
                    privkey = myprivkey;
                    cond = mycond;
                }
				else if (strcmp(destaddr, mytokensaddr) == 0)  // if this is TokensCC1vout
				{
					privkey = myprivkey;
					cond = mytokenscond;
//fprintf(stderr,"FinalizeCCTx() matched dual-eval TokensCC1vout my token addr.(%s)\n",mytokensaddr);
				}
				else if (strcmp(destaddr, mysingletokensaddr) == 0)  // if this is TokensCC1vout
				{
					privkey = myprivkey;
					cond = mysingletokenscond;
//fprintf(stderr, "FinalizeCCTx() matched single-eval token CC1vout my token addr.(%s)\n", mytokensaddr);
				}
                else if ( strcmp(destaddr,unspendable) == 0 )
                {
                    privkey = unspendablepriv;
                    cond = othercond;
//fprintf(stderr,"FinalizeCCTx evalcode(%d) matched unspendable CC addr.(%s)\n",cp->evalcode,unspendable);
                }
				else if (strcmp(destaddr, unspendabletokensaddr) == 0)
				{
					privkey = unspendablepriv;
					cond = othertokenscond;
//fprintf(stderr,"FinalizeCCTx() matched unspendabletokensaddr dual/three-eval CC addr.(%s)\n",unspendabletokensaddr);
				}
				// check if this is the 2nd additional evalcode + 'unspendable' cc addr:
                else if ( strcmp(destaddr, cp->unspendableaddr2) == 0)
                {
//fprintf(stderr,"FinalizeCCTx() matched %s unspendable2!\n",cp->unspendableaddr2);
                    privkey = cp->unspendablepriv2;
                    if( othercond2 == 0 ) 
                        othercond2 = MakeCCcond1(cp->unspendableEvalcode2, cp->unspendablepk2);
                    cond = othercond2;
                }
				// check if this is 3rd additional evalcode + 'unspendable' cc addr:
                else if ( strcmp(destaddr,cp->unspendableaddr3) == 0 )
                {
//fprintf(stderr,"FinalizeCCTx() matched %s unspendable3!\n",cp->unspendableaddr3);
                    privkey = cp->unspendablepriv3;
                    if( othercond3 == 0 )
                        othercond3 = MakeCCcond1(cp->unspendableEvalcode3, cp->unspendablepk3);
                    cond = othercond3;
                }
				// check if this is spending from 1of2 cc coins addr:
				else if (strcmp(cp->coins1of2addr, destaddr) == 0)
				{
//fprintf(stderr,"FinalizeCCTx() matched %s unspendable1of2!\n",cp->coins1of2addr);
                    privkey = cp->coins1of2priv;//myprivkey;
					if (othercond1of2 == 0)
						othercond1of2 = MakeCCcond1of2(cp->evalcode, cp->coins1of2pk[0], cp->coins1of2pk[1]);
					cond = othercond1of2;
				}
                else if ( strcmp(CC1of2CCaddr,destaddr) == 0 )
                {
//fprintf(stderr,"FinalizeCCTx() matched %s CC1of2CCaddr!\n",CC1of2CCaddr);
                    privkey = unspendablepriv;
                    if (condCC2 == 0)
                        condCC2 = MakeCCcond1of2(cp->evalcode,unspendablepk,unspendablepk);
                    cond = condCC2;
                }
				// check if this is spending from 1of2 cc tokens addr:
				else if (strcmp(cp->tokens1of2addr, destaddr) == 0)
				{
//fprintf(stderr,"FinalizeCCTx() matched %s cp->tokens1of2addr!\n", cp->tokens1of2addr);
					privkey = cp->tokens1of2priv;//myprivkey;
					if (othercond1of2tokens == 0)
                        // NOTE: if additionalEvalcode2 is not set then it is dual-eval cc else three-eval cc
                        // TODO: verify evalcodes order if additionalEvalcode2 is not 0
						othercond1of2tokens = MakeTokensCCcond1of2(cp->evalcode, cp->additionalTokensEvalcode2, cp->tokens1of2pk[0], cp->tokens1of2pk[1]);
					cond = othercond1of2tokens;
				}
                else
                {
                    flag = 0;
                    if ( pubkeys != NULL_pubkeys )
                    {
                        char coinaddr[64];
                        GetCCaddress1of2(cp,coinaddr,globalpk,pubkeys[i]);
                        //fprintf(stderr,"%s + %s -> %s vs %s\n",HexStr(globalpk).c_str(),HexStr(pubkeys[i]).c_str(),coinaddr,destaddr);
                        if ( strcmp(destaddr,coinaddr) == 0 )
                        {
                            privkey = cp->CCpriv;
                            if ( othercond4 != 0 )
                                cc_free(othercond4);
                            othercond4 = MakeCCcond1of2(cp->evalcode,globalpk,pubkeys[i]);
                            cond = othercond4;
                            flag = 1;
                        }
                    } //else  privkey = myprivkey;

                    if ( flag == 0 )
                    {
                        fprintf(stderr,"CC signing error: vini.%d has unknown CC address.(%s)\n",i,destaddr);
                        memset(myprivkey,0,32);
                        return sigDataNull;
                    }
                }
                uint256 sighash = SignatureHash(CCPubKey(cond), mtx, i, SIGHASH_ALL,utxovalues[i],consensusBranchId, &txdata);
                if ( 0 )
                {
                    int32_t z;
                    for (z=0; z<32; z++)
                        fprintf(stderr,"%02x",privkey[z]);
                    fprintf(stderr," privkey, ");
                    for (z=0; z<32; z++)
                        fprintf(stderr,"%02x",((uint8_t *)sighash.begin())[z]);
                    fprintf(stderr," sighash [%d] %.8f %x\n",i,(double)utxovalues[i]/COIN,consensusBranchId);
                }

                if (!remote)  // we have privkey in the wallet
                {
                    if (cc_signTreeSecp256k1Msg32(cond, privkey, sighash.begin()) != 0)
                    {
                        mtx.vin[i].scriptSig = CCSig(cond);
                    }
                    else
                    {
                        fprintf(stderr, "vini.%d has CC signing error address.(%s) %s\n", i, destaddr, EncodeHexTx(mtx).c_str());
                        memset(myprivkey, 0, sizeof(myprivkey));
                        return sigDataNull;
                    }
                }
                else   // no privkey locally - remote call
                {
                    // serialize cc:
                    UniValue ccjson;
                    ccjson.read(cc_conditionToJSONString(cond));
                    if (ccjson.empty())
                    {
                        fprintf(stderr, "vini.%d can't serialize CC.(%s) %s\n", i, destaddr, EncodeHexTx(mtx).c_str());
                        memset(myprivkey, 0, sizeof(myprivkey));
                        return sigDataNull;
                    }

                    AddSigData2UniValue(sigData, i, ccjson, std::string(), vintx.vout[utxovout].nValue);  // store vin i with scriptPubKey
                }
            }
        } else fprintf(stderr,"FinalizeCCTx2 couldnt find %s mgret.%d\n",mtx.vin[i].prevout.hash.ToString().c_str(),mgret);
    }
    if ( mycond != 0 )
        cc_free(mycond);
    if ( condCC2 != 0 )
        cc_free(condCC2);
    if ( othercond != 0 )
        cc_free(othercond);
    if ( othercond2 != 0 )
        cc_free(othercond2);
    if ( othercond3 != 0 )
        cc_free(othercond3);
    if ( othercond4 != 0 )
        cc_free(othercond4);
    if ( othercond1of2 != 0 )
        cc_free(othercond1of2);
    if ( othercond1of2tokens != 0 )
        cc_free(othercond1of2tokens);
    if ( mytokenscond != 0 )
        cc_free(mytokenscond);   
    if ( mysingletokenscond != 0 )
        cc_free(mysingletokenscond);   
    if ( othertokenscond != 0 )
        cc_free(othertokenscond);   
    memset(myprivkey,0,sizeof(myprivkey));
    std::string strHex = EncodeHexTx(mtx);
    if ( strHex.size() > 0 )
        result.push_back(Pair(JSON_HEXTX, strHex));
    else {
        result.push_back(Pair(JSON_HEXTX, "0"));
    }
    if (sigData.size() > 0) result.push_back(Pair(JSON_SIGDATA,sigData));
    return result;
}

void NSPV_CCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr,bool ccflag);
void NSPV_CCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &txids,char *coinaddr,bool ccflag);
void NSPV_CCtxids(std::vector<uint256> &txids,char *coinaddr,bool ccflag, uint8_t evalcode,uint256 filtertxid, uint8_t func);

void SetCCunspents(std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs,char *coinaddr,bool ccflag)
{
    int32_t type=0,i,n; char *ptr; std::string addrstr; uint160 hashBytes; std::vector<std::pair<uint160, int> > addresses;
    if ( KOMODO_NSPV_SUPERLITE )
    {
        NSPV_CCunspents(unspentOutputs,coinaddr,ccflag);
        return;
    }
    n = (int32_t)strlen(coinaddr);
    addrstr.resize(n+1);
    ptr = (char *)addrstr.data();
    for (i=0; i<=n; i++)
        ptr[i] = coinaddr[i];
    CBitcoinAddress address(addrstr);
    if ( address.GetIndexKey(hashBytes, type, ccflag) == 0 )
        return;
    addresses.push_back(std::make_pair(hashBytes,type));
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++)
    {
        if ( GetAddressUnspent((*it).first, (*it).second, unspentOutputs) == 0 )
            return;
    }
}

void SetCCtxids(std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,char *coinaddr,bool ccflag)
{
    int32_t type=0,i,n; char *ptr; std::string addrstr; uint160 hashBytes; std::vector<std::pair<uint160, int> > addresses;
    if ( KOMODO_NSPV_SUPERLITE )
    {
        NSPV_CCtxids(addressIndex,coinaddr,ccflag);
        return;
    }
    n = (int32_t)strlen(coinaddr);
    addrstr.resize(n+1);
    ptr = (char *)addrstr.data();
    for (i=0; i<=n; i++)
        ptr[i] = coinaddr[i];
    CBitcoinAddress address(addrstr);
    if ( address.GetIndexKey(hashBytes, type, ccflag) == 0 )
        return;
    addresses.push_back(std::make_pair(hashBytes,type));
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++)
    {
        if ( GetAddressIndex((*it).first, (*it).second, addressIndex) == 0 )
            return;
    }
}

void SetCCtxids(std::vector<uint256> &txids,char *coinaddr,bool ccflag, uint8_t evalcode, uint256 filtertxid, uint8_t func)
{
    int32_t type=0,i,n; char *ptr; std::string addrstr; uint160 hashBytes; std::vector<std::pair<uint160, int> > addresses;
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    if ( KOMODO_NSPV_SUPERLITE )
    {
        NSPV_CCtxids(txids,coinaddr,ccflag,evalcode,filtertxid,func);
        return;
    }
    n = (int32_t)strlen(coinaddr);
    addrstr.resize(n+1);
    ptr = (char *)addrstr.data();
    for (i=0; i<=n; i++)
        ptr[i] = coinaddr[i];
    CBitcoinAddress address(addrstr);
    if ( address.GetIndexKey(hashBytes, type, ccflag) == 0 )
        return;
    addresses.push_back(std::make_pair(hashBytes,type));
    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++)
    {
        if ( GetAddressIndex((*it).first, (*it).second, addressIndex) == 0 )
            return;
        for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it1=addressIndex.begin(); it1!=addressIndex.end(); it1++)
        {
            if (it1->second>=0) txids.push_back(it1->first.txhash);
        }
    } 
}

int64_t CCutxovalue(char *coinaddr,uint256 utxotxid,int32_t utxovout,int32_t CCflag)
{
    uint256 txid; std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,CCflag!=0?true:false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( txid == utxotxid && utxovout == it->first.index )
            return(it->second.satoshis);
    }
    return(0);
}

int64_t CCgettxout(uint256 txid,int32_t vout,int32_t mempoolflag,int32_t lockflag)
{
    CCoins coins;
    //fprintf(stderr,"CCgettxoud %s/v%d\n",txid.GetHex().c_str(),vout);
    if ( mempoolflag != 0 )
    {
        if ( lockflag != 0 )
        {
            LOCK(mempool.cs);
            CCoinsViewMemPool view(pcoinsTip, mempool);
            if (!view.GetCoins(txid, coins))
                return(-1);
            else if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) != 0 )
                return(-1);
        }
        else
        {
            CCoinsViewMemPool view(pcoinsTip, mempool);
            if (!view.GetCoins(txid, coins))
                return(-1);
            else if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) != 0 )
                return(-1);
        }
    }
    else
    {
        if (!pcoinsTip->GetCoins(txid, coins))
            return(-1);
    }
    if ( vout < coins.vout.size() )
        return(coins.vout[vout].nValue);
    else return(-1);
}

int32_t CCgetspenttxid(uint256 &spenttxid,int32_t &vini,int32_t &height,uint256 txid,int32_t vout)
{
    CSpentIndexKey key(txid, vout);
    CSpentIndexValue value;
    if ( !GetSpentIndex(key, value) )
        return(-1);
    spenttxid = value.txid;
    vini = (int32_t)value.inputIndex;
    height = value.blockHeight;
    return(0);
}

int64_t CCaddress_balance(char *coinaddr,int32_t CCflag)
{
    int64_t sum = 0; std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,CCflag!=0?true:false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        sum += it->second.satoshis;
    }
    return(sum);
}

int64_t CCfullsupply(uint256 tokenid)
{
    uint256 hashBlock; int32_t numvouts; CTransaction tx; std::vector<uint8_t> origpubkey; std::string name,description;
    if ( myGetTransaction(tokenid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 )
    {
        if (DecodeTokenCreateOpRet(tx.vout[numvouts-1].scriptPubKey,origpubkey,name,description))
        {
            return(tx.vout[1].nValue);
        }
    }
    return(0);
}

int64_t CCtoken_balance(char *coinaddr,uint256 reftokenid)
{
    int64_t price,sum = 0; int32_t numvouts; CTransaction tx; uint256 tokenid,txid,hashBlock; 
	std::vector<uint8_t>  vopretExtra;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
	uint8_t evalCode;

    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts=tx.vout.size()) > 0 )
        {
            char str[65];
			std::vector<CPubKey> voutTokenPubkeys;
            std::vector<std::pair<uint8_t, vscript_t>>  oprets;
            if ( reftokenid==txid || (DecodeTokenOpRet(tx.vout[numvouts-1].scriptPubKey, evalCode, tokenid, voutTokenPubkeys, oprets) != 0 && reftokenid == tokenid))
            {
                sum += it->second.satoshis;
            }
        }
    }
    return(sum);
}

int32_t CC_vinselect(int32_t *aboveip,int64_t *abovep,int32_t *belowip,int64_t *belowp,struct CC_utxo utxos[],int32_t numunspents,int64_t value)
{
    int32_t i,abovei,belowi; int64_t above,below,gap,atx_value;
    abovei = belowi = -1;
    for (above=below=i=0; i<numunspents; i++)
    {
        // Filter to randomly pick utxo to avoid conflicts, and having multiple CC choose the same ones.
        //if ( numunspents > 200 ) {
        //    if ( (rand() % 100) < 90 )
        //        continue;
        //}
        if ( (atx_value= utxos[i].nValue) <= 0 )
            continue;
        if ( atx_value == value )
        {
            *aboveip = *belowip = i;
            *abovep = *belowp = 0;
            return(i);
        }
        else if ( atx_value > value )
        {
            gap = (atx_value - value);
            if ( above == 0 || gap < above )
            {
                above = gap;
                abovei = i;
            }
        }
        else
        {
            gap = (value - atx_value);
            if ( below == 0 || gap < below )
            {
                below = gap;
                belowi = i;
            }
        }
        //printf("value %.8f gap %.8f abovei.%d %.8f belowi.%d %.8f\n",dstr(value),dstr(gap),abovei,dstr(above),belowi,dstr(below));
    }
    *aboveip = abovei;
    *abovep = above;
    *belowip = belowi;
    *belowp = below;
    //printf("above.%d below.%d\n",abovei,belowi);
    if ( abovei >= 0 && belowi >= 0 )
    {
        if ( above < (below >> 1) )
            return(abovei);
        else return(belowi);
    }
    else if ( abovei >= 0 )
        return(abovei);
    else return(belowi);
}

int64_t AddNormalinputsLocal(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs)
{
    int32_t abovei,belowi,ind,vout,i,n = 0; int64_t sum,threshold,above,below; int64_t remains,nValue,totalinputs = 0; uint256 txid,hashBlock; std::vector<COutput> vecOutputs; CTransaction tx; struct CC_utxo *utxos,*up;
    if ( KOMODO_NSPV_SUPERLITE )
        return(NSPV_AddNormalinputs(mtx,mypk,total,maxinputs,&NSPV_U));

    // if (mypk != pubkey2pk(Mypubkey()))  //remote superlite mypk, do not use wallet since it is not locked for non-equal pks (see rpcs with nspv support)!
    //     return(AddNormalinputs3(mtx, mypk, total, maxinputs));

#ifdef ENABLE_WALLET
    assert(pwalletMain != NULL);
    const CKeyStore& keystore = *pwalletMain;
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false, NULL, true);
    utxos = (struct CC_utxo *)calloc(CC_MAXVINS,sizeof(*utxos));
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    sum = 0;
    BOOST_FOREACH(const COutput& out, vecOutputs)
    {
        if ( out.fSpendable != 0 && out.tx->vout[out.i].nValue >= threshold )
        {
            txid = out.tx->GetHash();
            vout = out.i;
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && vout < tx.vout.size() && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() == 0 )
            {
                //fprintf(stderr,"check %.8f to vins array.%d of %d %s/v%d\n",(double)out.tx->vout[out.i].nValue/COIN,n,maxutxos,txid.GetHex().c_str(),(int32_t)vout);
                if ( mtx.vin.size() > 0 )
                {
                    for (i=0; i<mtx.vin.size(); i++)
                        if ( txid == mtx.vin[i].prevout.hash && vout == mtx.vin[i].prevout.n )
                            break;
                    if ( i != mtx.vin.size() )
                        continue;
                }
                if ( n > 0 )
                {
                    for (i=0; i<n; i++)
                        if ( txid == utxos[i].txid && vout == utxos[i].vout )
                            break;
                    if ( i != n )
                        continue;
                }
                if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
                {
                    up = &utxos[n++];
                    up->txid = txid;
                    up->nValue = out.tx->vout[out.i].nValue;
                    up->vout = vout;
                    sum += up->nValue;
                    //fprintf(stderr,"add %.8f to vins array.%d of %d\n",(double)up->nValue/COIN,n,maxutxos);
                    if ( n >= maxinputs || sum >= total )
                        break;
                }
            }
        }
    }
    remains = total;
    for (i=0; i<maxinputs && n>0; i++)
    {
        below = above = 0;
        abovei = belowi = -1;
        if ( CC_vinselect(&abovei,&above,&belowi,&below,utxos,n,remains) < 0 )
        {
            printf("error finding unspent i.%d of %d, %.8f vs %.8f\n",i,n,(double)remains/COIN,(double)total/COIN);
            free(utxos);
            return(0);
        }
        if ( belowi < 0 || abovei >= 0 )
            ind = abovei;
        else ind = belowi;
        if ( ind < 0 )
        {
            printf("error finding unspent i.%d of %d, %.8f vs %.8f, abovei.%d belowi.%d ind.%d\n",i,n,(double)remains/COIN,(double)total/COIN,abovei,belowi,ind);
            free(utxos);
            return(0);
        }
        up = &utxos[ind];
        mtx.vin.push_back(CTxIn(up->txid,up->vout,CScript()));
        totalinputs += up->nValue;
        remains -= up->nValue;
        utxos[ind] = utxos[--n];
        memset(&utxos[n],0,sizeof(utxos[n]));
        //fprintf(stderr,"totalinputs %.8f vs total %.8f i.%d vs max.%d\n",(double)totalinputs/COIN,(double)total/COIN,i,maxinputs);
        if ( totalinputs >= total || (i+1) >= maxinputs )
            break;
    }
    free(utxos);
    if ( totalinputs >= total )
    {
        //fprintf(stderr,"return totalinputs %.8f\n",(double)totalinputs/COIN);
        return(totalinputs);
    }
#endif
    return(0);
}

// always uses -pubkey param as mypk
int64_t AddNormalinputs2(CMutableTransaction &mtx, int64_t total, int32_t maxinputs)
{
    CPubKey mypk = pubkey2pk(Mypubkey());
    return AddNormalinputsRemote(mtx, mypk, total, maxinputs);
}

// has additional mypk param for nspv calls
int64_t AddNormalinputsRemote(CMutableTransaction &mtx, CPubKey mypk, int64_t total, int32_t maxinputs)
{
    int32_t abovei,belowi,ind,vout,i,n = 0; int64_t sum,threshold,above,below; int64_t remains,nValue,totalinputs = 0; char coinaddr[64]; uint256 txid,hashBlock; CTransaction tx; struct CC_utxo *utxos,*up;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    if ( KOMODO_NSPV_SUPERLITE )
        return(NSPV_AddNormalinputs(mtx,mypk,total,maxinputs,&NSPV_U));
    utxos = (struct CC_utxo *)calloc(CC_MAXVINS,sizeof(*utxos));
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    sum = 0;
    Getscriptaddress(coinaddr,CScript() << vscript_t(mypk.begin(), mypk.end()) << OP_CHECKSIG);
    SetCCunspents(unspentOutputs,coinaddr,false);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < threshold )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && vout < tx.vout.size() && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() == 0 )
        {
            //fprintf(stderr,"check %.8f to vins array.%d of %d %s/v%d\n",(double)out.tx->vout[out.i].nValue/COIN,n,maxutxos,txid.GetHex().c_str(),(int32_t)vout);
            if ( mtx.vin.size() > 0 )
            {
                for (i=0; i<mtx.vin.size(); i++)
                    if ( txid == mtx.vin[i].prevout.hash && vout == mtx.vin[i].prevout.n )
                        break;
                if ( i != mtx.vin.size() )
                    continue;
            }
            if ( n > 0 )
            {
                for (i=0; i<n; i++)
                    if ( txid == utxos[i].txid && vout == utxos[i].vout )
                        break;
                if ( i != n )
                    continue;
            }
            if ( myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                up = &utxos[n++];
                up->txid = txid;
                up->nValue = it->second.satoshis;
                up->vout = vout;
                sum += up->nValue;
                //fprintf(stderr,"add %.8f to vins array.%d of %d\n",(double)up->nValue/COIN,n,maxutxos);
                if ( n >= maxinputs || sum >= total )
                    break;
            }
        }
    }
    remains = total;
    for (i=0; i<maxinputs && n>0; i++)
    {
        below = above = 0;
        abovei = belowi = -1;
        if ( CC_vinselect(&abovei,&above,&belowi,&below,utxos,n,remains) < 0 )
        {
            printf("error finding unspent i.%d of %d, %.8f vs %.8f\n",i,n,(double)remains/COIN,(double)total/COIN);
            free(utxos);
            return(0);
        }
        if ( belowi < 0 || abovei >= 0 )
            ind = abovei;
        else ind = belowi;
        if ( ind < 0 )
        {
            printf("error finding unspent i.%d of %d, %.8f vs %.8f, abovei.%d belowi.%d ind.%d\n",i,n,(double)remains/COIN,(double)total/COIN,abovei,belowi,ind);
            free(utxos);
            return(0);
        }
        up = &utxos[ind];
        mtx.vin.push_back(CTxIn(up->txid,up->vout,CScript()));
        totalinputs += up->nValue;
        remains -= up->nValue;
        utxos[ind] = utxos[--n];
        memset(&utxos[n],0,sizeof(utxos[n]));
        //fprintf(stderr,"totalinputs %.8f vs total %.8f i.%d vs max.%d\n",(double)totalinputs/COIN,(double)total/COIN,i,maxinputs);
        if ( totalinputs >= total || (i+1) >= maxinputs )
            break;
    }
    free(utxos);
    if ( totalinputs >= total )
    {
        //fprintf(stderr,"return totalinputs %.8f\n",(double)totalinputs/COIN);
        return(totalinputs);
    }
    return(0);
}

int64_t AddNormalinputs(CMutableTransaction &mtx,CPubKey mypk,int64_t total,int32_t maxinputs,bool remote)
{
    if (!remote)  return (AddNormalinputsLocal(mtx,mypk,total,maxinputs));
    else return (AddNormalinputsRemote(mtx,mypk,total,maxinputs));
}

void AddSigData2UniValue(UniValue &sigdata, int32_t vini, UniValue& ccjson, std::string sscriptpubkey, int64_t amount)
{
    UniValue elem(UniValue::VOBJ);
    elem.push_back(Pair("vin", vini));
    if (!ccjson.empty())
        elem.push_back(Pair("cc", ccjson));
    if (!sscriptpubkey.empty())
        elem.push_back(Pair("scriptPubKey", sscriptpubkey));
    elem.push_back(Pair("amount", amount));
    sigdata.push_back(elem);
}