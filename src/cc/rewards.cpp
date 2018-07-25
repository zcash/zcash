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
 The rewards CC contract is initially for OOT, which needs this functionality. However, many of the attributes can be parameterized to allow different rewards programs to run. Multiple rewards plans could even run on the same blockchain, though the user would need to choose which one to lock funds into.
 
 At the high level, the user would lock funds for some amount of time and at the end of it, would get all the locked funds back with an additional reward. So there needs to be a lock funds and unlock funds ability. Additionally, the rewards need to come from somewhere, so similar to the faucet, there would be a way to fund the reward.
 
 Additional requirements are for the user to be able to lock funds via SPV. This requirement in turns forces the creation of a way for anybody to be able to unlock the funds as that operation requires a native daemon running and cant be done over SPV. The idea is to allow anybody to run a script that would unlock all funds that are matured. As far as the user is concerned, he locks his funds via SPV and after some time it comes back with an extra reward.
 
 In reality, the funds are locked into a CC address that is unspendable, except for some special conditions and it needs to come back to the address that funded the lock. In order to implement this, several things are clear.
 
 1) each locked CC utxo needs to be linked to a specific rewards plan
 2) each locked CC utxo needs to know the only address that it can be unlocked into
 3) SPV requirement means the lock transaction needs to be able to be created without any CC signing
 
 The opreturn will be used to store the name of the rewards plan and all funding and locked funds with the same plan will use the same pool of rewards. plan names will be limited to 8 chars and encoded into a uint64_t.
 
 The initial funding transaction will have all the parameters for the rewards plan encoded in the vouts. Additional fundings will just increase the available CC utxo for the rewards.
 
 Locks wont have any CC vins, but will send to the RewardsCCaddress, with the plan stringbits in the opreturn. vout1 will have the unlock address and no other destination is valid.
 
 Unlock does a CC spend to the vout1 address
 */

uint64_t RewardsCalc(uint64_t claim,uint256 txid) // min/max time, mindeposit and rate
{
    uint64_t reward = 0;
    // get start time, get current time
    // if elapsed < mintime -> return 0
    // if elapsed > maxtime, elapsed = maxtime
    // calc reward
    return(reward);
}

CScript EncodeRewardsFundingOpRet(uint8_t funcid,uint64_t sbits,uint64_t APR,uint64_t minseconds,uint64_t maxseconds,uint64_t mindeposit)
{
    CScript opret; uint8_t evalcode = EVAL_REWARDS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << 'F' << sbits << APR << minseconds << maxseconds << mindeposit);
    return(opret);
}

uint8_t DecodeRewardsFundingOpRet(const CScript &scriptPubKey,uint64_t &sbits,uint64_t &APR,uint64_t &minseconds,uint64_t &maxseconds,uint64_t &mindeposit)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> APR; ss >> minseconds; ss >> maxseconds; ss >> mindeposit) != 0 )
    {
        if ( e == EVAL_REWARDS && f == 'F' )
            return(f);
    }
    return(0);
}

CScript EncodeRewardsOpRet(uint8_t funcid,uint64_t sbits,uint256 fundingtxid)
{
    CScript opret; uint8_t evalcode = EVAL_REWARDS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << sbits << fundingtxid);
    return(opret);
}

uint8_t DecodeRewardsOpRet(const CScript &scriptPubKey,uint64_t &sbits,uint256 &fundingtxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> fundingtxid) != 0 )
    {
        if ( e == EVAL_REWARDS && (f == 'L' || f == 'U') )
            return(f);
    }
    return(0);
}

uint64_t IsRewardsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool RewardsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock,activehash; int32_t i,numvins,numvouts; uint64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    for (i=0; i<numvins; i++)
    {
        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) != 0 )
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("always should find vin, but didnt");
            else
            {
                if ( hashBlock == zerohash )
                    return eval->Invalid("cant rewards from mempool");
                if ( (assetoshis= IsRewardsvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsRewardsvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+COIN+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + COIN + txfee");
    }
    else return(true);
}

bool RewardsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        // follow rules
        for (i=0; i<numvins; i++)
        {
            if ( IsCCInput(tx.vin[0].scriptSig) == 0 )
                return eval->Invalid("illegal normal vini");
        }
        if ( RewardsExactAmounts(cp,eval,tx,1,10000) == false )
            return false;
        else
        {
            preventCCvouts = 1;
            if ( IsRewardsvout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            if ( tx.vout[i].nValue != COIN )
                return eval->Invalid("invalid rewards output");
            return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
        }
    }
    return(true);
}

uint64_t AddRewardsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64]; uint64_t nValue,totalinputs = 0; uint256 txid,hashBlock; CTransaction vintx; int32_t n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsRewardsvout(cp,vintx,(int32_t)it->first.index)) > 0 )
            {
                if ( total != 0 && maxinputs != 0 )
                    mtx.vin.push_back(CTxIn(txid,(int32_t)it->first.index,CScript()));
                totalinputs += it->second.satoshis;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            }
        }
    }
    return(totalinputs);
}

uint64_t RewardsPlanFunds(uint64_t &refsbits,struct CCcontract_info *cp,CPubKey &pk,char *planstr,uint256 fundingtxid)
{
    char coinaddr[64]; uint64_t sbits,nValue,totalinputs = 0; uint256 hashBlock; CTransaction vintx;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    if ( planstr == 0 || planstr[0] == 0 || strlen(planstr) > 8 )
        return(0);
    refsbits = stringbits(planstr);
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        if ( GetTransaction(it->first.txhash,vintx,hashBlock,false) != 0 )
        {
            if ( (nValue= IsRewardsvout(cp,vintx,(int32_t)it->first.index)) > 0 )
            {
                
                totalinputs += nValue;
            }
        }
    }
    return(totalinputs);
}

std::string RewardsUnlock(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 locktxid)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t funding,sbits,reward,amount=0,inputs,CCchange=0; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    rewardspk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    funding = RewardsPlanFunds(sbits,cp,rewardspk,planstr,fundingtxid);
    if ( locktxid == zeroid )
        amount= AddRewardsInputs(cp,mtx,mypk,(1LL << 30),1);
    //else amount = value...;
    if ( amount > 0 && (reward= RewardsCalc(amount,mtx.vin[0].prevout.hash)) > txfee )
    {
        if ( (inputs= AddRewardsInputs(cp,mtx,mypk,reward+amount+txfee,30)) > 0 )
        {
            if ( inputs >= (amount + reward + 2*txfee) )
                CCchange = (inputs - amount - reward - txfee);
            if ( CCchange != 0 )
                mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,rewardspk));
            mtx.vout.push_back(CTxOut(amount+reward,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(cp,mtx,mypk,txfee,EncodeRewardsOpRet('U',sbits,fundingtxid)));
        }
        fprintf(stderr,"cant find enough rewards inputs\n");
    }
    fprintf(stderr,"cant find rewards inputs\n");
    return(0);
}

std::string RewardsCreateFunding(uint64_t txfee,char *planstr,uint64_t funds,uint64_t APR,uint64_t minseconds,uint64_t maxseconds,uint64_t mindeposit)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,funding; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    funding = RewardsPlanFunds(sbits,cp,rewardspk,planstr,zeroid);
    if ( AddNormalinputs(mtx,mypk,funds+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,rewardspk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(rewardspk)) << OP_CHECKSIG));
        return(FinalizeCCTx(cp,mtx,mypk,txfee,EncodeRewardsFundingOpRet('F',sbits,APR,minseconds,maxseconds,mindeposit)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return(0);
}

std::string RewardsAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,uint64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,funding; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    if ( (funding= RewardsPlanFunds(sbits,cp,rewardspk,planstr,fundingtxid)) >= amount )
    {
        if ( AddNormalinputs(mtx,mypk,amount+txfee,64) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,rewardspk));
            return(FinalizeCCTx(cp,mtx,mypk,txfee,EncodeRewardsOpRet('L',sbits,fundingtxid)));
        } else fprintf(stderr,"cant find enough inputs\n");
    }
    fprintf(stderr,"cant find rewards inputs\n");
    return(0);
}

std::string RewardsLock(uint64_t txfee,char *planstr,uint256 fundingtxid,uint64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,funding; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    if ( (funding= RewardsPlanFunds(sbits,cp,rewardspk,planstr,fundingtxid)) >= amount )
    {
        if ( AddNormalinputs(mtx,mypk,amount+txfee,64) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,rewardspk));
            return(FinalizeCCTx(cp,mtx,mypk,txfee,EncodeRewardsOpRet('L',sbits,fundingtxid)));
        } else fprintf(stderr,"cant find enough inputs\n");
    }
    fprintf(stderr,"cant find rewards inputs\n");
    return(0);
}


