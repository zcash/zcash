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

#include "CCfsm.h"
#include "../txmempool.h"

/*
 FSM CC is a highlevel CC contract that mostly uses other CC contracts. A finite state machine is defined, which combines triggers, payments and whatever other events/actions into a state machine
 
*/

// start of consensus code

int64_t IsFSMvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool FSMExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant FSM from mempool");
                if ( (assetoshis= IsFSMvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsFSMvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+COIN+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + COIN + txfee");
    }
    else return(true);
}

bool FSMValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i; bool retval;
    return eval->Invalid("no validation yet");
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        //fprintf(stderr,"check vins\n");
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
            {
                fprintf(stderr,"fsmget invalid vini\n");
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( FSMExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"fsmget invalid amount\n");
            return false;
        }
        else
        {
            preventCCvouts = 1;
            if ( IsFSMvout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            if ( tx.vout[i].nValue != COIN )
                return eval->Invalid("invalid fsm output");
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"fsmget validated\n");
            else fprintf(stderr,"fsmget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddFSMInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
{
    // add threshold check
    char coinaddr[64]; int64_t nValue,price,totalinputs = 0; uint256 txid,hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        // prevent dup
        if ( it->second.satoshis < 1000000 )
            continue;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( (nValue= IsFSMvout(cp,vintx,(int32_t)it->first.index)) > 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,(int32_t)it->first.index,CScript()));
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

std::string FSMList()
{
    return("");
}

std::string FSMCreate(uint64_t txfee,std::string name,std::string states)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,fsmpk; CScript opret; int64_t inputs,CCchange=0,nValue=COIN; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_FSM);
    if ( txfee == 0 )
        txfee = 10000;
    fsmpk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddFSMInputs(cp,mtx,fsmpk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_FSM,CCchange,fsmpk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,opret));
    } else fprintf(stderr,"cant find fsm inputs\n");
    return("");
}

std::string FSMInfo(uint256 fsmtxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,fsmpk; int64_t funds = 0; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_FSM);
    mypk = pubkey2pk(Mypubkey());
    fsmpk = GetUnspendable(cp,0);
    return("");
}


