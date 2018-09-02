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

#include "CCTriggers.h"

/*
 Triggers CC is a building block CC that allows creation of event -> action processing, where events are defined during trigger creation and actions to be mostly done via payments, but by making payments to other CC contracts, it can be used to invoke other CC contracts
 
*/

// start of consensus code

int64_t IsTriggersvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool TriggersExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        //fprintf(stderr,"vini.%d\n",i);
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            //fprintf(stderr,"vini.%d check mempool\n",i);
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                //fprintf(stderr,"vini.%d check hash and vout\n",i);
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant Triggers from mempool");
                if ( (assetoshis= IsTriggersvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsTriggersvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool TriggersValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid; uint8_t hash[32]; char str[65],destaddr[64];
    return(false);
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
            {
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( TriggersExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Triggersget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Triggersget validated\n");
            else fprintf(stderr,"Triggersget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddTriggersInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
{
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout,n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        // no need to prevent dup
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsTriggersvout(cp,vintx,vout)) > 1000000 && myIsutxo_spentinmempool(txid,vout) == 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    return(totalinputs);
}

std::string TriggersGet(uint64_t txfee,int64_t nValue)
{
    CMutableTransaction mtx,tmpmtx; CPubKey mypk,Triggerspk; int64_t inputs,CCchange=0; struct CCcontract_info *cp,C; std::string rawhex; uint32_t j; int32_t i,len; uint8_t buf[32768]; bits256 hash;
    cp = CCinit(&C,EVAL_TRIGGERS);
    if ( txfee == 0 )
        txfee = 10000;
    Triggerspk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddTriggersInputs(cp,mtx,Triggerspk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_TRIGGERS,CCchange,Triggerspk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        fprintf(stderr,"start at %u\n",(uint32_t)time(NULL));
        j = rand() & 0xfffffff;
        for (i=0; i<1000000; i++,j++)
        {
            tmpmtx = mtx;
            rawhex = FinalizeCCTx(-1LL,cp,tmpmtx,mypk,txfee,CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_TRIGGERS << (uint8_t)'G' << j));
            if ( (len= (int32_t)rawhex.size()) > 0 && len < 65536 )
            {
                len >>= 1;
                decode_hex(buf,len,(char *)rawhex.c_str());
                hash = bits256_doublesha256(0,buf,len);
                if ( (hash.bytes[0] & 0xff) == 0 && (hash.bytes[31] & 0xff) == 0 )
                {
                    fprintf(stderr,"found valid txid after %d iterations %u\n",i,(uint32_t)time(NULL));
                    return(rawhex);
                }
                //fprintf(stderr,"%02x%02x ",hash.bytes[0],hash.bytes[31]);
            }
        }
        fprintf(stderr,"couldnt generate valid txid %u\n",(uint32_t)time(NULL));
        return("");
    } else fprintf(stderr,"cant find Triggers inputs\n");
    return("");
}

std::string TriggersFund(uint64_t txfee,int64_t funds)
{
    CMutableTransaction mtx; CPubKey mypk,Triggerspk; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_TRIGGERS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    Triggerspk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_TRIGGERS,funds,Triggerspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}

UniValue TriggersInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx; CPubKey Triggerspk; struct CCcontract_info *cp,C; int64_t funding;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Triggers"));
    cp = CCinit(&C,EVAL_TRIGGERS);
    Triggerspk = GetUnspendable(cp,0);
    funding = AddTriggersInputs(cp,mtx,Triggerspk,0,0);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

