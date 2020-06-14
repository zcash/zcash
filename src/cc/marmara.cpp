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

#include "CCMarmara.h"

/*
 Marmara CC is for the MARMARA project
 
 'R': two forms for initial issuance and for accepting existing
 vins normal
 vout0 approval to senderpk (issuer or owner of baton)
 
 'I'
 vin0 approval from 'R'
 vins1+ normal
 vout0 baton to 1st receiverpk
 vout1 marker to Marmara so all issuances can be tracked (spent when loop is closed)
 
 'T'
 vin0 approval from 'R'
 vin1 baton from 'I'/'T'
 vins2+ normal
 vout0 baton to next receiverpk (following the unspent baton back to original is the credit loop)
 
 'S'
 vin0 'I' marker
 vin1 baton
 vins CC utxos from credit loop
 
 'D' default/partial payment
 
 'L' lockfunds
 
*/

// start of consensus code

int64_t IsMarmaravout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

int32_t MarmaraRandomize(uint32_t ind)
{
    uint64_t val64; uint32_t val,range = (MARMARA_MAXLOCK - MARMARA_MINLOCK);
    val64 = komodo_block_prg(ind);
    val = (uint32_t)(val64 >> 32);
    val ^= (uint32_t)val64;
    return((val % range) + MARMARA_MINLOCK);
}

int32_t MarmaraUnlockht(int32_t height)
{
    uint32_t ind = height / MARMARA_GROUPSIZE;
    height = (height / MARMARA_GROUPSIZE) * MARMARA_GROUPSIZE;
    return(height + MarmaraRandomize(ind));
}

uint8_t DecodeMaramaraCoinbaseOpRet(const CScript scriptPubKey,CPubKey &pk,int32_t &height,int32_t &unlockht)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey,vopret);
    script = (uint8_t *)vopret.data();
    if ( 0 )
    {
        int32_t i;
        for (i=0; i<vopret.size(); i++)
            fprintf(stderr,"%02x",script[i]);
        fprintf(stderr," <- opret\n");
    }
    if ( vopret.size() > 2 && script[0] == EVAL_MARMARA )
    {
        if ( script[1] == 'C' || script[1] == 'P' || script[1] == 'L' )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> pk; ss >> height; ss >> unlockht) != 0 )
            {
                return(script[1]);
            } else fprintf(stderr,"DecodeMaramaraCoinbaseOpRet unmarshal error for %c\n",script[1]);
        } //else fprintf(stderr,"script[1] is %d != 'C' %d or 'P' %d or 'L' %d\n",script[1],'C','P','L');
    } else fprintf(stderr,"vopret.size() is %d\n",(int32_t)vopret.size());
    return(0);
}

CScript EncodeMarmaraCoinbaseOpRet(uint8_t funcid,CPubKey pk,int32_t ht)
{
    CScript opret; int32_t unlockht; uint8_t evalcode = EVAL_MARMARA;
    unlockht = MarmaraUnlockht(ht);
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << pk << ht << unlockht);
    if ( 0 )
    {
        std::vector<uint8_t> vopret; uint8_t *script,i;
        GetOpReturnData(opret,vopret);
        script = (uint8_t *)vopret.data();
        {
            for (i=0; i<vopret.size(); i++)
                fprintf(stderr,"%02x",script[i]);
            fprintf(stderr," <- gen opret.%c\n",funcid);
        }
    }
    return(opret);
}

CScript MarmaraLoopOpret(uint8_t funcid,uint256 createtxid,CPubKey senderpk,int64_t amount,int32_t matures,std::string currency)
{
    CScript opret; uint8_t evalcode = EVAL_MARMARA;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << createtxid << senderpk << amount << matures << currency);
    return(opret);
}

uint8_t MarmaraDecodeLoopOpret(const CScript scriptPubKey,uint256 &createtxid,CPubKey &senderpk,int64_t &amount,int32_t &matures,std::string &currency)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> createtxid; ss >> senderpk; ss >> amount; ss >> matures; ss >> currency) != 0 )
    {
        return(f);
    }
    return(0);
}

int32_t MarmaraGetcreatetxid(uint256 &createtxid,uint256 txid)
{
    CTransaction tx; uint256 hashBlock; uint8_t funcid; int32_t numvouts,matures; std::string currency; CPubKey senderpk; int64_t amount;
    if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
    {
        if ( (funcid= MarmaraDecodeLoopOpret(tx.vout[numvouts-1].scriptPubKey,createtxid,senderpk,amount,matures,currency)) == 'I' || funcid == 'T' )
            return(0);
        else if ( funcid == 'R' )
        {
            if ( createtxid == zeroid )
                createtxid = txid;
            return(0);
        }
    }
    return(-1);
}

int32_t MarmaraGetbatontxid(std::vector<uint256> &creditloop,uint256 &batontxid,uint256 txid)
{
    uint256 createtxid,spenttxid; int64_t value; int32_t vini,height,n=0,vout = 0;
    memset(&batontxid,0,sizeof(batontxid));
    if ( MarmaraGetcreatetxid(createtxid,txid) == 0 )
    {
        txid = createtxid;
        //fprintf(stderr,"txid.%s -> createtxid %s\n",txid.GetHex().c_str(),createtxid.GetHex().c_str());
        while ( CCgetspenttxid(spenttxid,vini,height,txid,vout) == 0 )
        {
            creditloop.push_back(txid);
            //fprintf(stderr,"%d: %s\n",n,txid.GetHex().c_str());
            n++;
            if ( (value= CCgettxout(spenttxid,vout,1,1)) == 10000 )
            {
                batontxid = spenttxid;
                //fprintf(stderr,"got baton %s %.8f\n",batontxid.GetHex().c_str(),(double)value/COIN);
                return(n);
            }
            else if ( value > 0 )
            {
                batontxid = spenttxid;
                fprintf(stderr,"n.%d got false baton %s/v%d %.8f\n",n,batontxid.GetHex().c_str(),vout,(double)value/COIN);
                return(n);
            }
            // get funcid
            txid = spenttxid;
        }
    }
    return(-1);
}

CScript Marmara_scriptPubKey(int32_t height,CPubKey pk)
{
    CTxOut ccvout; struct CCcontract_info *cp,C; CPubKey Marmarapk;
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    if ( height > 0 && (height & 1) == 0 && pk.size() == 33 )
    {
        ccvout = MakeCC1of2vout(EVAL_MARMARA,0,Marmarapk,pk);
        //char coinaddr[64];
        //Getscriptaddress(coinaddr,ccvout.scriptPubKey);
        //fprintf(stderr,"Marmara_scriptPubKey %s ht.%d -> %s\n",HexStr(pk).c_str(),height,coinaddr);
    }
    return(ccvout.scriptPubKey);
}

CScript MarmaraCoinbaseOpret(uint8_t funcid,int32_t height,CPubKey pk)
{
    uint8_t *ptr;
    //fprintf(stderr,"height.%d pksize.%d\n",height,(int32_t)pk.size());
    if ( height > 0 && (height & 1) == 0 && pk.size() == 33 )
        return(EncodeMarmaraCoinbaseOpRet(funcid,pk,height));
    return(CScript());
}

int32_t MarmaraValidateCoinbase(int32_t height,CTransaction tx)
{
    struct CCcontract_info *cp,C; CPubKey Marmarapk,pk; int32_t ht,unlockht; CTxOut ccvout;
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    if ( 0 )
    {
        int32_t d,histo[365*2+30];
        memset(histo,0,sizeof(histo));
        for (ht=2; ht<100; ht++)
            fprintf(stderr,"%d ",MarmaraUnlockht(ht));
        fprintf(stderr," <- first 100 unlock heights\n");
        for (ht=2; ht<1000000; ht+=MARMARA_GROUPSIZE)
        {
            d = (MarmaraUnlockht(ht) - ht) / 1440;
            if ( d < 0 || d > sizeof(histo)/sizeof(*histo) )
                fprintf(stderr,"d error.%d at ht.%d\n",d,ht);
            else histo[d]++;
        }
        for (ht=0; ht<sizeof(histo)/sizeof(*histo); ht++)
            fprintf(stderr,"%d ",histo[ht]);
        fprintf(stderr,"<- unlock histogram[%d] by days locked\n",(int32_t)(sizeof(histo)/sizeof(*histo)));
    }
    if ( (height & 1) != 0 )
        return(0);
    if ( tx.vout.size() == 2 && tx.vout[1].nValue == 0 )
    {
        if ( DecodeMaramaraCoinbaseOpRet(tx.vout[1].scriptPubKey,pk,ht,unlockht) == 'C' )
        {
            if ( ht == height && MarmaraUnlockht(height) == unlockht )
            {
                //fprintf(stderr,"ht.%d -> unlock.%d\n",ht,unlockht);
                ccvout = MakeCC1of2vout(EVAL_MARMARA,0,Marmarapk,pk);
                if ( ccvout.scriptPubKey == tx.vout[0].scriptPubKey )
                    return(0);
                char addr0[64],addr1[64];
                Getscriptaddress(addr0,ccvout.scriptPubKey);
                Getscriptaddress(addr1,tx.vout[0].scriptPubKey);
                fprintf(stderr,"ht.%d mismatched CCvout scriptPubKey %s vs %s pk.%d %s\n",height,addr0,addr1,(int32_t)pk.size(),HexStr(pk).c_str());
            } else fprintf(stderr,"ht.%d %d vs %d unlock.%d\n",height,MarmaraUnlockht(height),ht,unlockht);
        } else fprintf(stderr,"ht.%d error decoding coinbase opret\n",height);
    }
    return(-1);
}

bool MarmaraPoScheck(char *destaddr,CScript opret,CTransaction staketx)
{
    CPubKey Marmarapk,pk; int32_t height,unlockht; uint8_t funcid; char coinaddr[64]; struct CCcontract_info *cp,C;
    //fprintf(stderr,"%s numvins.%d numvouts.%d %.8f opret[%d]\n",staketx.GetHash().ToString().c_str(),(int32_t)staketx.vin.size(),(int32_t)staketx.vout.size(),(double)staketx.vout[0].nValue/COIN,(int32_t)opret.size());
    if ( staketx.vout.size() == 2 && opret == staketx.vout[1].scriptPubKey )
    {
        cp = CCinit(&C,EVAL_MARMARA);
        funcid = DecodeMaramaraCoinbaseOpRet(opret,pk,height,unlockht);
        Marmarapk = GetUnspendable(cp,0);
        GetCCaddress1of2(cp,coinaddr,Marmarapk,pk);
        //fprintf(stderr,"matched opret! funcid.%c ht.%d unlock.%d %s\n",funcid,height,unlockht,coinaddr);
        return(strcmp(destaddr,coinaddr) == 0);
    }
    return(0);
}

bool MarmaraValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    std::vector<uint8_t> vopret; CTransaction vinTx; uint256 hashBlock;  int32_t numvins,numvouts,i,ht,unlockht,vht,vunlockht; uint8_t funcid,vfuncid,*script; CPubKey pk,vpk;
    if ( ASSETCHAINS_MARMARA == 0 )
        return eval->Invalid("-ac_marmara must be set for marmara CC");
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else if ( tx.vout.size() >= 2 )
    {
        GetOpReturnData(tx.vout[tx.vout.size()-1].scriptPubKey,vopret);
        script = (uint8_t *)vopret.data();
        if ( vopret.size() < 2 || script[0] != EVAL_MARMARA )
            return eval->Invalid("no opreturn");
        funcid = script[1];
        if ( funcid == 'P' )
        {
            funcid = DecodeMaramaraCoinbaseOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,pk,ht,unlockht);
            for (i=0; i<numvins; i++)
            {
                if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
                {
                    if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("cant find vinTx");
                    else
                    {
                        if ( vinTx.IsCoinBase() == 0 )
                            return eval->Invalid("noncoinbase input");
                        else if ( vinTx.vout.size() != 2 )
                            return eval->Invalid("coinbase doesnt have 2 vouts");
                        vfuncid = DecodeMaramaraCoinbaseOpRet(vinTx.vout[1].scriptPubKey,vpk,vht,vunlockht);
                        if ( vfuncid != 'C' || vpk != pk || vunlockht != unlockht )
                            return eval->Invalid("mismatched opreturn");
                    }
                }
            }
            return(true);
        }
        else if ( funcid == 'L' ) // lock -> lock funds with a unlockht
        {
            return(true);
        }
        else if ( funcid == 'R' ) // receive -> agree to receive 'I' from pk, amount, currency, dueht
        {
            return(true);
        }
        else if ( funcid == 'I' ) // issue -> issue currency to pk with due date height
        {
            return(true);
        }
        else if ( funcid == 'T' ) // transfer -> given 'R' transfer 'I' or 'T' to the pk of 'R'
        {
            return(true);
        }
        else if ( funcid == 'S' ) // settlement -> automatically spend issuers locked funds, given 'I'
        {
            return(true);
        }
        else if ( funcid == 'D' ) // insufficient settlement
        {
            return(true);
        }
        else if ( funcid == 'C' ) // coinbase
        {
            return(true);
        }
        // staking only for locked utxo
    }
    return eval->Invalid("fall through error");
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddMarmaraCoinbases(struct CCcontract_info *cp,CMutableTransaction &mtx,int32_t firstheight,CPubKey poolpk,int32_t maxinputs)
{
    char coinaddr[64]; CPubKey Marmarapk,pk; int64_t nValue,totalinputs = 0; uint256 txid,hashBlock; CTransaction vintx; int32_t unlockht,ht,vout,unlocks,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    Marmarapk = GetUnspendable(cp,0);
    GetCCaddress1of2(cp,coinaddr,Marmarapk,poolpk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    unlocks = MarmaraUnlockht(firstheight);
    //fprintf(stderr,"check coinaddr.(%s)\n",coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //fprintf(stderr,"txid.%s/v%d\n",txid.GetHex().c_str(),vout);
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( vintx.IsCoinBase() != 0 && vintx.vout.size() == 2 && vintx.vout[1].nValue == 0 )
            {
                if ( DecodeMaramaraCoinbaseOpRet(vintx.vout[1].scriptPubKey,pk,ht,unlockht) == 'C' && unlockht == unlocks && pk == poolpk && ht >= firstheight )
                {
                    if ( (nValue= vintx.vout[vout].nValue) > 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
                    {
                        if ( maxinputs != 0 )
                            mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                        nValue = it->second.satoshis;
                        totalinputs += nValue;
                        n++;
                        if ( maxinputs > 0 && n >= maxinputs )
                            break;
                    } //else fprintf(stderr,"nValue.%8f\n",(double)nValue/COIN);
                } //else fprintf(stderr,"decode error unlockht.%d vs %d pk.%d\n",unlockht,unlocks,pk == poolpk);
            } else fprintf(stderr,"not coinbase\n");
        } else fprintf(stderr,"error getting tx\n");
    }
    return(totalinputs);
}

int64_t AddMarmarainputs(CMutableTransaction &mtx,std::vector<CPubKey> &pubkeys,char *coinaddr,int64_t total,int32_t maxinputs)
{
    uint64_t threshold,nValue,totalinputs = 0; uint256 txid,hashBlock; CTransaction tx; int32_t numvouts,ht,unlockht,vout,i,n = 0; uint8_t funcid; CPubKey pk; std::vector<int64_t> vals;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,true);
    if ( maxinputs > CC_MAXVINS )
        maxinputs = CC_MAXVINS;
    if ( maxinputs > 0 )
        threshold = total/maxinputs;
    else threshold = total;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < threshold )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 && vout < numvouts && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
        {
            if ( (funcid= DecodeMaramaraCoinbaseOpRet(tx.vout[numvouts-1].scriptPubKey,pk,ht,unlockht)) == 'C' || funcid == 'P' || funcid == 'L' )
            {
                //char str[64]; fprintf(stderr,"(%s) %s/v%d %.8f ht.%d unlockht.%d\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN,ht,unlockht);
                if ( total != 0 && maxinputs != 0 )
                {
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                    pubkeys.push_back(pk);
                }
                totalinputs += it->second.satoshis;
                vals.push_back(it->second.satoshis);
                n++;
                if ( maxinputs != 0 && total == 0 )
                    continue;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            } else fprintf(stderr,"null funcid\n");
        }
    }
    if ( maxinputs != 0 && total == 0 )
    {
        std::sort(vals.begin(),vals.end());
        totalinputs = 0;
        for (i=0; i<maxinputs && i<vals.size(); i++)
            totalinputs += vals[i];
    }
    return(totalinputs);
}

UniValue MarmaraLock(uint64_t txfee,int64_t amount,int32_t height)
{
    CMutableTransaction tmpmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; CPubKey Marmarapk,mypk,pk; int32_t unlockht,refunlockht,vout,ht,numvouts; int64_t nValue,val,inputsum=0,threshold,remains,change = 0; std::string rawtx,errorstr; char coinaddr[64]; uint256 txid,hashBlock; CTransaction tx; uint8_t funcid;
    if ( txfee == 0 )
        txfee = 10000;
    if ( (height & 1) != 0 )
        height++;
    cp = CCinit(&C,EVAL_MARMARA);
    mypk = pubkey2pk(Mypubkey());
    Marmarapk = GetUnspendable(cp,0);
    Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG);
    if ( (val= CCaddress_balance(coinaddr,0)) < amount )
        val -= txfee;
    else val = amount;
    if ( val > txfee )
        inputsum = AddNormalinputs2(mtx,val,CC_MAXVINS/2);
    //fprintf(stderr,"normal inputs %.8f val %.8f\n",(double)inputsum/COIN,(double)val/COIN);
    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,amount,Marmarapk,mypk));
    if ( inputsum < amount+txfee )
    {
        refunlockht = MarmaraUnlockht(height);
        result.push_back(Pair("normalfunds",ValueFromAmount(inputsum)));
        result.push_back(Pair("height",height));
        result.push_back(Pair("unlockht",refunlockht));
        remains = (amount + txfee) - inputsum;
        std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
        GetCCaddress1of2(cp,coinaddr,Marmarapk,mypk);
        SetCCunspents(unspentOutputs,coinaddr,true);
        threshold = remains / (MARMARA_VINS+1);
        uint8_t mypriv[32];
        Myprivkey(mypriv);
        CCaddr1of2set(cp,Marmarapk,mypk,mypriv,coinaddr);
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            txid = it->first.txhash;
            vout = (int32_t)it->first.index;
            if ( (nValue= it->second.satoshis) < threshold )
                continue;
            if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 && vout < numvouts && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
            {
                if ( (funcid= DecodeMaramaraCoinbaseOpRet(tx.vout[numvouts-1].scriptPubKey,pk,ht,unlockht)) == 'C' || funcid == 'P' || funcid == 'L' )
                {
                    if ( unlockht < refunlockht )
                    {
                        mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                        //fprintf(stderr,"merge CC vout %s/v%d %.8f unlockht.%d < ref.%d\n",txid.GetHex().c_str(),vout,(double)nValue/COIN,unlockht,refunlockht);
                        inputsum += nValue;
                        remains -= nValue;
                        if ( inputsum >= amount + txfee )
                        {
                            //fprintf(stderr,"inputsum %.8f >= amount %.8f, update amount\n",(double)inputsum/COIN,(double)amount/COIN);
                            amount = inputsum - txfee;
                            break;
                        }
                    }
                }
            }
        }
        memset(mypriv,0,sizeof(mypriv));
    }
    if ( inputsum >= amount+txfee )
    {
        if ( inputsum > amount+txfee )
        {
            change = (inputsum - amount);
            mtx.vout.push_back(CTxOut(change,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        }
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,MarmaraCoinbaseOpret('L',height,mypk));
        if ( rawtx.size() == 0 )
            errorstr = (char *)"couldnt finalize CCtx";
        else
        {
            result.push_back(Pair("result",(char *)"success"));
            result.push_back(Pair("hex",rawtx));
            return(result);
        }
    } else errorstr = (char *)"insufficient funds";
    result.push_back(Pair("result",(char *)"error"));
    result.push_back(Pair("error",errorstr));
    return(result);
}

int32_t MarmaraSignature(uint8_t *utxosig,CMutableTransaction &mtx)
{
    uint256 txid,hashBlock; uint8_t *ptr; int32_t i,siglen,vout,numvouts; CTransaction tx; std::string rawtx; CPubKey mypk; std::vector<CPubKey> pubkeys; struct CCcontract_info *cp,C; uint64_t txfee;
    txfee = 10000;
    vout = mtx.vin[0].prevout.n;
    if ( myGetTransaction(mtx.vin[0].prevout.hash,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 && vout < numvouts )
    {
        cp = CCinit(&C,EVAL_MARMARA);
        mypk = pubkey2pk(Mypubkey());
        pubkeys.push_back(mypk);
        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,tx.vout[numvouts - 1].scriptPubKey,pubkeys);
        if ( rawtx.size() > 0 )
        {
            siglen = mtx.vin[0].scriptSig.size();
            ptr = &mtx.vin[0].scriptSig[0];
            for (i=0; i<siglen; i++)
            {
                utxosig[i] = ptr[i];
                //fprintf(stderr,"%02x",ptr[i]);
            }
            //fprintf(stderr," got signed rawtx.%s siglen.%d\n",rawtx.c_str(),siglen);
            return(siglen);
        }
    }
    return(0);
}

// jl777: decide on what unlockht settlement change should have -> from utxo making change

UniValue MarmaraSettlement(uint64_t txfee,uint256 refbatontxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); std::vector<uint256> creditloop; uint256 batontxid,createtxid,refcreatetxid,hashBlock; uint8_t funcid; int32_t numerrs=0,i,n,numvouts,matures,refmatures,height; int64_t amount,refamount,remaining,inputsum,change; CPubKey Marmarapk,mypk,pk; std::string currency,refcurrency,rawtx; CTransaction tx,batontx; char coinaddr[64],myCCaddr[64],destaddr[64],batonCCaddr[64],str[2],txidaddr[64]; std::vector<CPubKey> pubkeys; struct CCcontract_info *cp,C;
    if ( txfee == 0 )
        txfee = 10000;
    cp = CCinit(&C,EVAL_MARMARA);
    mypk = pubkey2pk(Mypubkey());
    Marmarapk = GetUnspendable(cp,0);
    remaining = change = 0;
    height = chainActive.LastTip()->GetHeight();
    if ( (n= MarmaraGetbatontxid(creditloop,batontxid,refbatontxid)) > 0 )
    {
        if ( myGetTransaction(batontxid,batontx,hashBlock) != 0 && (numvouts= batontx.vout.size()) > 1 )
        {
            if ( (funcid= MarmaraDecodeLoopOpret(batontx.vout[numvouts-1].scriptPubKey,refcreatetxid,pk,refamount,refmatures,refcurrency)) != 0 )
            {
                if ( refcreatetxid != creditloop[0] )
                {
                    result.push_back(Pair("result",(char *)"error"));
                    result.push_back(Pair("error",(char *)"invalid refcreatetxid, setting to creditloop[0]"));
                    return(result);
                }
                else if ( chainActive.LastTip()->GetHeight() < refmatures )
                {
                    fprintf(stderr,"doesnt mature for another %d blocks\n",refmatures - chainActive.LastTip()->GetHeight());
                    result.push_back(Pair("result",(char *)"error"));
                    result.push_back(Pair("error",(char *)"cant settle immature creditloop"));
                    return(result);
                }
                else if ( (refmatures & 1) == 0 )
                {
                    result.push_back(Pair("result",(char *)"error"));
                    result.push_back(Pair("error",(char *)"cant automatic settle even maturity heights"));
                    return(result);
                }
                else if ( n < 1 )
                {
                    result.push_back(Pair("result",(char *)"error"));
                    result.push_back(Pair("error",(char *)"creditloop too short"));
                    return(result);
                }
                remaining = refamount;
                GetCCaddress(cp,myCCaddr,Mypubkey());
                Getscriptaddress(batonCCaddr,batontx.vout[0].scriptPubKey);
                if ( strcmp(myCCaddr,batonCCaddr) == 0 )
                {
                    mtx.vin.push_back(CTxIn(n == 1 ? batontxid : creditloop[1],1,CScript())); // issuance marker
                    pubkeys.push_back(Marmarapk);
                    mtx.vin.push_back(CTxIn(batontxid,0,CScript()));
                    pubkeys.push_back(mypk);
                    for (i=1; i<n; i++)
                    {
                        if ( myGetTransaction(creditloop[i],tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
                        {
                            if ( (funcid= MarmaraDecodeLoopOpret(tx.vout[numvouts-1].scriptPubKey,createtxid,pk,amount,matures,currency)) != 0 )
                            {
                                GetCCaddress1of2(cp,coinaddr,Marmarapk,pk);
                                if ( (inputsum= AddMarmarainputs(mtx,pubkeys,coinaddr,remaining,MARMARA_VINS)) >= remaining )
                                {
                                    change = (inputsum - remaining);
                                    mtx.vout.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                                    if ( change > txfee )
                                        mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,change,Marmarapk,pk));
                                    rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,MarmaraLoopOpret('S',createtxid,mypk,0,refmatures,currency),pubkeys);
                                    result.push_back(Pair("result",(char *)"success"));
                                    result.push_back(Pair("hex",rawtx));
                                    return(result);
                                } else remaining -= inputsum;
                                if ( mtx.vin.size() >= CC_MAXVINS - MARMARA_VINS )
                                    break;
                            } else fprintf(stderr,"null funcid for creditloop[%d]\n",i);
                        } else fprintf(stderr,"couldnt get creditloop[%d]\n",i);
                    }
                    if ( refamount - remaining > 2*txfee )
                    {
                        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(CCtxidaddr(txidaddr,createtxid))) << OP_CHECKSIG)); // failure marker
                        if ( refamount-remaining > 3*txfee )
                            mtx.vout.push_back(CTxOut(refamount-remaining-2*txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                        rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,MarmaraLoopOpret('D',createtxid,mypk,-remaining,refmatures,currency),pubkeys);
                        result.push_back(Pair("result",(char *)"error"));
                        result.push_back(Pair("error",(char *)"insufficient funds"));
                        result.push_back(Pair("hex",rawtx));
                        result.push_back(Pair("remaining",ValueFromAmount(remaining)));
                    }
                    else
                    {
                        // jl777: maybe fund a txfee to report no funds avail
                        result.push_back(Pair("result",(char *)"error"));
                        result.push_back(Pair("error",(char *)"no funds available at all"));
                    }
                }
                else
                {
                    result.push_back(Pair("result",(char *)"error"));
                    result.push_back(Pair("error",(char *)"this node does not have the baton"));
                    result.push_back(Pair("myCCaddr",myCCaddr));
                    result.push_back(Pair("batonCCaddr",batonCCaddr));
                }
            }
            else
            {
                result.push_back(Pair("result",(char *)"error"));
                result.push_back(Pair("error",(char *)"couldnt get batontxid opret"));
            }
        }
        else
        {
            result.push_back(Pair("result",(char *)"error"));
            result.push_back(Pair("error",(char *)"couldnt find batontxid"));
        }
    }
    else
    {
        result.push_back(Pair("result",(char *)"error"));
        result.push_back(Pair("error",(char *)"couldnt get creditloop"));
    }
    return(result);
}

int32_t MarmaraGetCreditloops(int64_t &totalamount,std::vector<uint256> &issuances,int64_t &totalclosed,std::vector<uint256> &closed,struct CCcontract_info *cp,int32_t firstheight,int32_t lastheight,int64_t minamount,int64_t maxamount,CPubKey refpk,std::string refcurrency)
{
    char coinaddr[64]; CPubKey Marmarapk,senderpk; int64_t amount; uint256 createtxid,txid,hashBlock; CTransaction tx; int32_t numvouts,vout,matures,n=0; std::string currency;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    Marmarapk = GetUnspendable(cp,0);
    GetCCaddress(cp,coinaddr,Marmarapk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    // do all txid, conditional on spent/unspent
    //fprintf(stderr,"check coinaddr.(%s)\n",coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        //fprintf(stderr,"txid.%s/v%d\n",txid.GetHex().c_str(),vout);
        if ( vout == 1 && myGetTransaction(txid,tx,hashBlock) != 0 )
        {
            if ( tx.IsCoinBase() == 0 && (numvouts= tx.vout.size()) > 2 && tx.vout[numvouts - 1].nValue == 0 )
            {
                if ( MarmaraDecodeLoopOpret(tx.vout[numvouts-1].scriptPubKey,createtxid,senderpk,amount,matures,currency) == 'I' )
                {
                    n++;
                    if ( currency == refcurrency && matures >= firstheight && matures <= lastheight && amount >= minamount && amount <= maxamount && (refpk.size() == 0 || senderpk == refpk) )
                    {
                        issuances.push_back(txid);
                        totalamount += amount;
                    }
                }
            }
        } else fprintf(stderr,"error getting tx\n");
    }
    return(n);
}

UniValue MarmaraReceive(uint64_t txfee,CPubKey senderpk,int64_t amount,std::string currency,int32_t matures,uint256 batontxid,bool automaticflag)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CPubKey mypk; struct CCcontract_info *cp,C; std::string rawtx; char *errorstr=0; uint256 createtxid; int64_t batonamount; int32_t needbaton = 0;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( txfee == 0 )
        txfee = 10000;
    if ( automaticflag != 0 && (matures & 1) == 0 )
        matures++;
    else if ( automaticflag == 0 && (matures & 1) != 0 )
        matures++;
    mypk = pubkey2pk(Mypubkey());
    memset(&createtxid,0,sizeof(createtxid));
    if ( batontxid != zeroid && MarmaraGetcreatetxid(createtxid,batontxid) < 0 )
        errorstr = (char *)"cant get createtxid from batontxid";
    else if ( currency != "MARMARA" )
        errorstr = (char *)"for now, only MARMARA loops are supported";
    else if ( amount <= txfee )
        errorstr = (char *)"amount must be for more than txfee";
    else if ( matures <= chainActive.LastTip()->GetHeight() )
        errorstr = (char *)"it must mature in the future";
    if ( errorstr == 0 )
    {
        if ( batontxid != zeroid )
            batonamount = txfee;
        else batonamount = 2*txfee;
        if ( AddNormalinputs(mtx,mypk,batonamount + txfee,1) > 0 )
        {
            errorstr = (char *)"couldnt finalize CCtx";
            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA,batonamount,senderpk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,MarmaraLoopOpret('R',createtxid,senderpk,amount,matures,currency));
            if ( rawtx.size() > 0 )
                errorstr = 0;
        } else errorstr = (char *)"dont have enough normal inputs for 2*txfee";
    }
    if ( rawtx.size() == 0 || errorstr != 0 )
    {
        result.push_back(Pair("result","error"));
        if ( errorstr != 0 )
            result.push_back(Pair("error",errorstr));
    }
    else
    {
        result.push_back(Pair("result",(char *)"success"));
        result.push_back(Pair("hex",rawtx));
        result.push_back(Pair("funcid","R"));
        result.push_back(Pair("createtxid",createtxid.GetHex()));
        if ( batontxid != zeroid )
            result.push_back(Pair("batontxid",batontxid.GetHex()));
        result.push_back(Pair("senderpk",HexStr(senderpk)));
        result.push_back(Pair("amount",ValueFromAmount(amount)));
        result.push_back(Pair("matures",matures));
        result.push_back(Pair("currency",currency));
    }
    return(result);
}

UniValue MarmaraIssue(uint64_t txfee,uint8_t funcid,CPubKey receiverpk,int64_t amount,std::string currency,int32_t matures,uint256 approvaltxid,uint256 batontxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ); CPubKey mypk,Marmarapk; struct CCcontract_info *cp,C; std::string rawtx; uint256 createtxid; char *errorstr=0;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( txfee == 0 )
        txfee = 10000;
    // make sure less than maxlength
    Marmarapk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( MarmaraGetcreatetxid(createtxid,approvaltxid) < 0 )
        errorstr = (char *)"cant get createtxid from approvaltxid";
    else if ( currency != "MARMARA" )
        errorstr = (char *)"for now, only MARMARA loops are supported";
    else if ( amount <= txfee )
        errorstr = (char *)"amount must be for more than txfee";
    else if ( matures <= chainActive.LastTip()->GetHeight() )
        errorstr = (char *)"it must mature in the future";
    if ( errorstr == 0 )
    {
        mtx.vin.push_back(CTxIn(approvaltxid,0,CScript()));
        if ( funcid == 'T' )
            mtx.vin.push_back(CTxIn(batontxid,0,CScript()));
        if ( funcid == 'I' || AddNormalinputs(mtx,mypk,txfee,1) > 0 )
        {
            errorstr = (char *)"couldnt finalize CCtx";
            mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA,txfee,receiverpk));
            if ( funcid == 'I' )
                mtx.vout.push_back(MakeCC1vout(EVAL_MARMARA,txfee,Marmarapk));
            rawtx = FinalizeCCTx(0,cp,mtx,mypk,txfee,MarmaraLoopOpret(funcid,createtxid,receiverpk,amount,matures,currency));
            if ( rawtx.size() > 0 )
                errorstr = 0;
        } else errorstr = (char *)"dont have enough normal inputs for 2*txfee";
    }
    if ( rawtx.size() == 0 || errorstr != 0 )
    {
        result.push_back(Pair("result","error"));
        if ( errorstr != 0 )
            result.push_back(Pair("error",errorstr));
    }
    else
    {
        result.push_back(Pair("result",(char *)"success"));
        result.push_back(Pair("hex",rawtx));
        char str[2]; str[0] = funcid, str[1] = 0;
        result.push_back(Pair("funcid",str));
        result.push_back(Pair("createtxid",createtxid.GetHex()));
        result.push_back(Pair("approvaltxid",approvaltxid.GetHex()));
        if ( funcid == 'T' )
            result.push_back(Pair("batontxid",batontxid.GetHex()));
        result.push_back(Pair("receiverpk",HexStr(receiverpk)));
        result.push_back(Pair("amount",ValueFromAmount(amount)));
        result.push_back(Pair("matures",matures));
        result.push_back(Pair("currency",currency));
    }
    return(result);
}

UniValue MarmaraCreditloop(uint256 txid)
{
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); std::vector<uint256> creditloop; uint256 batontxid,createtxid,refcreatetxid,hashBlock; uint8_t funcid; int32_t numerrs=0,i,n,numvouts,matures,refmatures; int64_t amount,refamount; CPubKey pk; std::string currency,refcurrency; CTransaction tx; char coinaddr[64],myCCaddr[64],destaddr[64],batonCCaddr[64],str[2]; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_MARMARA);
    if ( (n= MarmaraGetbatontxid(creditloop,batontxid,txid)) > 0 )
    {
        if ( myGetTransaction(batontxid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
        {
            result.push_back(Pair("result",(char *)"success"));
            Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG);
            result.push_back(Pair("myaddress",coinaddr));
            GetCCaddress(cp,myCCaddr,Mypubkey());
            result.push_back(Pair("myCCaddress",myCCaddr));
            if ( (funcid= MarmaraDecodeLoopOpret(tx.vout[numvouts-1].scriptPubKey,refcreatetxid,pk,refamount,refmatures,refcurrency)) != 0 )
            {
                str[0] = funcid, str[1] = 0;
                result.push_back(Pair("funcid",str));
                result.push_back(Pair("currency",refcurrency));
                if ( funcid == 'S' )
                {
                    refcreatetxid = creditloop[0];
                    result.push_back(Pair("settlement",batontxid.GetHex()));
                    result.push_back(Pair("createtxid",refcreatetxid.GetHex()));
                    result.push_back(Pair("remainder",ValueFromAmount(refamount)));
                    result.push_back(Pair("settled",refmatures));
                    result.push_back(Pair("pubkey",HexStr(pk)));
                    Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                    result.push_back(Pair("coinaddr",coinaddr));
                    result.push_back(Pair("collected",ValueFromAmount(tx.vout[0].nValue)));
                    Getscriptaddress(destaddr,tx.vout[0].scriptPubKey);
                    if ( strcmp(coinaddr,destaddr) != 0 )
                    {
                        result.push_back(Pair("destaddr",destaddr));
                        numerrs++;
                    }
                    refamount = -1;
                }
                else if ( funcid == 'D' )
                {
                    refcreatetxid = creditloop[0];
                    result.push_back(Pair("settlement",batontxid.GetHex()));
                    result.push_back(Pair("createtxid",refcreatetxid.GetHex()));
                    result.push_back(Pair("remainder",ValueFromAmount(refamount)));
                    result.push_back(Pair("settled",refmatures));
                    Getscriptaddress(destaddr,tx.vout[0].scriptPubKey);
                    result.push_back(Pair("txidaddr",destaddr));
                    if ( tx.vout.size() > 1 )
                        result.push_back(Pair("collected",ValueFromAmount(tx.vout[1].nValue)));
                }
                else
                {
                    result.push_back(Pair("batontxid",batontxid.GetHex()));
                    result.push_back(Pair("createtxid",refcreatetxid.GetHex()));
                    result.push_back(Pair("amount",ValueFromAmount(refamount)));
                    result.push_back(Pair("matures",refmatures));
                    if ( refcreatetxid != creditloop[0] )
                    {
                        fprintf(stderr,"invalid refcreatetxid, setting to creditloop[0]\n");
                        refcreatetxid = creditloop[0];
                        numerrs++;
                    }
                    result.push_back(Pair("batonpk",HexStr(pk)));
                    Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                    result.push_back(Pair("batonaddr",coinaddr));
                    GetCCaddress(cp,batonCCaddr,pk);
                    result.push_back(Pair("batonCCaddr",batonCCaddr));
                    Getscriptaddress(coinaddr,tx.vout[0].scriptPubKey);
                    if ( strcmp(coinaddr,batonCCaddr) != 0 )
                    {
                        result.push_back(Pair("vout0address",coinaddr));
                        numerrs++;
                    }
                    if ( strcmp(myCCaddr,coinaddr) == 0 )
                        result.push_back(Pair("ismine",1));
                    else result.push_back(Pair("ismine",0));
                }
                for (i=0; i<n; i++)
                {
                    if ( myGetTransaction(creditloop[i],tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 1 )
                    {
                        if ( (funcid= MarmaraDecodeLoopOpret(tx.vout[numvouts-1].scriptPubKey,createtxid,pk,amount,matures,currency)) != 0 )
                        {
                            UniValue obj(UniValue::VOBJ);
                            obj.push_back(Pair("txid",creditloop[i].GetHex()));
                            str[0] = funcid, str[1] = 0;
                            obj.push_back(Pair("funcid",str));
                            if ( funcid == 'R' && createtxid == zeroid )
                            {
                                createtxid = creditloop[i];
                                obj.push_back(Pair("issuerpk",HexStr(pk)));
                                Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                                obj.push_back(Pair("issueraddr",coinaddr));
                                GetCCaddress(cp,coinaddr,pk);
                                obj.push_back(Pair("issuerCCaddr",coinaddr));
                            }
                            else
                            {
                                obj.push_back(Pair("receiverpk",HexStr(pk)));
                                Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG);
                                obj.push_back(Pair("receiveraddr",coinaddr));
                                GetCCaddress(cp,coinaddr,pk);
                                obj.push_back(Pair("receiverCCaddr",coinaddr));
                            }
                            Getscriptaddress(destaddr,tx.vout[0].scriptPubKey);
                            if ( strcmp(destaddr,coinaddr) != 0 )
                            {
                                obj.push_back(Pair("vout0address",destaddr));
                                numerrs++;
                            }
                            if ( i == 0 && refamount < 0 )
                            {
                                refamount = amount;
                                refmatures = matures;
                                result.push_back(Pair("amount",ValueFromAmount(refamount)));
                                result.push_back(Pair("matures",refmatures));
                            }
                            if ( createtxid != refcreatetxid || amount != refamount || matures != refmatures || currency != refcurrency )
                            {
                                numerrs++;
                                obj.push_back(Pair("objerror",(char *)"mismatched createtxid or amount or matures or currency"));
                                obj.push_back(Pair("createtxid",createtxid.GetHex()));
                                obj.push_back(Pair("amount",ValueFromAmount(amount)));
                                obj.push_back(Pair("matures",matures));
                                obj.push_back(Pair("currency",currency));
                            }
                            a.push_back(obj);
                        }
                    }
                }
                result.push_back(Pair("n",n));
                result.push_back(Pair("numerrors",numerrs));
                result.push_back(Pair("creditloop",a));
            }
            else
            {
                result.push_back(Pair("result",(char *)"error"));
                result.push_back(Pair("error",(char *)"couldnt get batontxid opret"));
            }
        }
        else
        {
            result.push_back(Pair("result",(char *)"error"));
            result.push_back(Pair("error",(char *)"couldnt find batontxid"));
        }
    }
    else
    {
        result.push_back(Pair("result",(char *)"error"));
        result.push_back(Pair("error",(char *)"couldnt get creditloop"));
    }
    return(result);
}

UniValue MarmaraPoolPayout(uint64_t txfee,int32_t firstheight,double perc,char *jsonstr) // [[pk0, shares0], [pk1, shares1], ...]
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); cJSON *item,*array; std::string rawtx; int32_t i,n; uint8_t buf[33]; CPubKey Marmarapk,pk,poolpk; int64_t payout,poolfee=0,total,totalpayout=0; double poolshares,share,shares = 0.; char *pkstr,*errorstr=0; struct CCcontract_info *cp,C;
    poolpk = pubkey2pk(Mypubkey());
    if ( txfee == 0 )
        txfee = 10000;
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    if ( (array= cJSON_Parse(jsonstr)) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
    {
        for (i=0; i<n; i++)
        {
            item = jitem(array,i);
            if ( (pkstr= jstr(jitem(item,0),0)) != 0 && strlen(pkstr) == 66 )
                shares += jdouble(jitem(item,1),0);
            else
            {
                errorstr = (char *)"all items must be of the form [<pubke>, <shares>]";
                break;
            }
        }
        if ( errorstr == 0 && shares > SMALLVAL )
        {
            shares += shares * perc;
            if ( (total= AddMarmaraCoinbases(cp,mtx,firstheight,poolpk,60)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    item = jitem(array,i);
                    if ( (share= jdouble(jitem(item,1),0)) > SMALLVAL )
                    {
                        payout = (share * (total - txfee)) / shares;
                        if ( payout > 0 )
                        {
                            if ( (pkstr= jstr(jitem(item,0),0)) != 0 && strlen(pkstr) == 66 )
                            {
                                UniValue x(UniValue::VOBJ);
                                totalpayout += payout;
                                decode_hex(buf,33,pkstr);
                                mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,payout,Marmarapk,buf2pk(buf)));
                                x.push_back(Pair(pkstr, (double)payout/COIN));
                                a.push_back(x);
                            }
                        }
                    }
                }
                if ( totalpayout > 0 && total > totalpayout-txfee )
                {
                    poolfee = (total - totalpayout - txfee);
                    mtx.vout.push_back(MakeCC1of2vout(EVAL_MARMARA,poolfee,Marmarapk,poolpk));
                }
                rawtx = FinalizeCCTx(0,cp,mtx,poolpk,txfee,MarmaraCoinbaseOpret('P',firstheight,poolpk));
                if ( rawtx.size() == 0 )
                    errorstr = (char *)"couldnt finalize CCtx";
            } else errorstr = (char *)"couldnt find any coinbases to payout";
        }
        else if ( errorstr == 0 )
            errorstr = (char *)"no valid shares submitted";
        free(array);
    } else errorstr = (char *)"couldnt parse poolshares jsonstr";
    if ( rawtx.size() == 0 || errorstr != 0 )
    {
        result.push_back(Pair("result","error"));
        if ( errorstr != 0 )
            result.push_back(Pair("error",errorstr));
    }
    else
    {
        result.push_back(Pair("result",(char *)"success"));
        result.push_back(Pair("hex",rawtx));
        if ( totalpayout > 0 && total > totalpayout-txfee )
        {
            result.push_back(Pair("firstheight",firstheight));
            result.push_back(Pair("lastheight",((firstheight / MARMARA_GROUPSIZE)+1) * MARMARA_GROUPSIZE  - 1));
            result.push_back(Pair("total",ValueFromAmount(total)));
            result.push_back(Pair("totalpayout",ValueFromAmount(totalpayout)));
            result.push_back(Pair("totalshares",shares));
            result.push_back(Pair("poolfee",ValueFromAmount(poolfee)));
            result.push_back(Pair("perc",ValueFromAmount((int64_t)(100. * (double)poolfee/totalpayout * COIN))));
            result.push_back(Pair("payouts",a));
        }
    }
    return(result);
}

// get all tx, constrain by vout, issuances[] and closed[]

UniValue MarmaraInfo(CPubKey refpk,int32_t firstheight,int32_t lastheight,int64_t minamount,int64_t maxamount,std::string currency)
{
    CMutableTransaction mtx; std::vector<CPubKey> pubkeys;
    UniValue result(UniValue::VOBJ),a(UniValue::VARR),b(UniValue::VARR); int32_t i,n,matches; int64_t totalclosed=0,totalamount=0; std::vector<uint256> issuances,closed; char coinaddr[64];
    CPubKey Marmarapk; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_MARMARA);
    Marmarapk = GetUnspendable(cp,0);
    result.push_back(Pair("result","success"));
    Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(Mypubkey())) << OP_CHECKSIG);
    result.push_back(Pair("myaddress",coinaddr));
    result.push_back(Pair("normal",ValueFromAmount(CCaddress_balance(coinaddr,0))));
    
    GetCCaddress1of2(cp,coinaddr,Marmarapk,Mypubkey());
    result.push_back(Pair("myCCactivated",coinaddr));
    result.push_back(Pair("activated",ValueFromAmount(CCaddress_balance(coinaddr,1))));
    result.push_back(Pair("activated16",ValueFromAmount(AddMarmarainputs(mtx,pubkeys,coinaddr,0,MARMARA_VINS))));
    
    GetCCaddress(cp,coinaddr,Mypubkey());
    result.push_back(Pair("myCCaddress",coinaddr));
    result.push_back(Pair("CCutxos",ValueFromAmount(CCaddress_balance(coinaddr,1))));

    if ( refpk.size() == 33 )
        result.push_back(Pair("issuer",HexStr(refpk)));
    if ( currency.size() == 0 )
        currency = (char *)"MARMARA";
    if ( firstheight <= lastheight )
        firstheight = 0, lastheight = (1 << 30);
    if ( minamount <= maxamount )
        minamount = 0, maxamount = (1LL << 60);
    result.push_back(Pair("firstheight",firstheight));
    result.push_back(Pair("lastheight",lastheight));
    result.push_back(Pair("minamount",ValueFromAmount(minamount)));
    result.push_back(Pair("maxamount",ValueFromAmount(maxamount)));
    result.push_back(Pair("currency",currency));
    if ( (n= MarmaraGetCreditloops(totalamount,issuances,totalclosed,closed,cp,firstheight,lastheight,minamount,maxamount,refpk,currency)) > 0 )
    {
        result.push_back(Pair("n",n));
        matches = (int32_t)issuances.size();
        result.push_back(Pair("pending",matches));
        for (i=0; i<matches; i++)
            a.push_back(issuances[i].GetHex());
        result.push_back(Pair("issuances",a));
        result.push_back(Pair("totalamount",ValueFromAmount(totalamount)));
        matches = (int32_t)closed.size();
        result.push_back(Pair("numclosed",matches));
        for (i=0; i<matches; i++)
            b.push_back(closed[i].GetHex());
        result.push_back(Pair("closed",b));
        result.push_back(Pair("totalclosed",ValueFromAmount(totalclosed)));
    }
    return(result);
}
