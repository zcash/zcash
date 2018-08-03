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
 
 
 createfunding
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: normal marker vout for easy searching
 vout.2: normal change
 vout.n-1: opreturn 'F' sbits APR minseconds maxseconds mindeposit
 
 addfunding
 vins.*: normal inputs
 vout.0: CC vout for funding
 vout.1: normal change
 vout.n-1: opreturn 'A' sbits fundingtxid
 
 lock
 vins.*: normal inputs
 vout.0: CC vout for locked funds
 vout.1: normal output to unlock address
 vout.2: change
 vout.n-1: opreturn 'L' sbits fundingtxid
 
 unlock
 vin.0: locked funds CC vout.0 from lock
 vin.1+: funding CC vout.0 from 'F' and 'A' and 'U'
 vout.0: funding CC change
 vout.1: normal output to unlock address
 vout.n-1: opreturn 'U' sbits fundingtxid
 
 */

uint64_t RewardsCalc(uint64_t amount,uint256 txid,uint64_t APR,uint64_t minseconds,uint64_t maxseconds,uint64_t mindeposit)
{
    int32_t numblocks; uint64_t duration,reward = 0;
    fprintf(stderr,"minseconds %llu maxseconds %llu\n",(long long)minseconds,(long long)maxseconds);
    if ( (duration= CCduration(numblocks,txid)) < minseconds )
    {
        fprintf(stderr,"duration %llu < minseconds %llu\n",(long long)duration,(long long)minseconds);
        return(0);
        //duration = (uint32_t)time(NULL) - (1532713903 - 3600 * 24);
    } else if ( duration > maxseconds )
        duration = maxseconds;
    reward = (((amount * APR) / COIN) * duration) / (365*24*3600LL * 100);
    fprintf(stderr,"amount %.8f %.8f %llu -> duration.%llu reward %.8f\n",(double)amount/COIN,((double)amount * APR)/COIN,(long long)((amount * APR) / (COIN * 365*24*3600)),(long long)duration,(double)reward/COIN);
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
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> APR; ss >> minseconds; ss >> maxseconds; ss >> mindeposit) != 0 )
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

uint8_t DecodeRewardsOpRet(uint256 txid,const CScript &scriptPubKey,uint64_t &sbits,uint256 &fundingtxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid; uint64_t APR,minseconds,maxseconds,mindeposit;
    GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_REWARDS )
        {
            if ( script[1] == 'F' )
            {
                if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> APR; ss >> minseconds; ss >> maxseconds; ss >> mindeposit) != 0 )
                {
                    fundingtxid = txid;
                    return('F');
                } else fprintf(stderr,"unmarshal error for F\n");
            }
            else if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> fundingtxid) != 0 )
            {
                if ( e == EVAL_REWARDS && (f == 'L' || f == 'U' || f == 'A') )
                    return(f);
                else fprintf(stderr,"mismatched e.%02x f.(%c)\n",e,f);
            }
        } else fprintf(stderr,"script[0] %02x != EVAL_REWARDS\n",script[0]);
    } else fprintf(stderr,"not enough opret.[%d]\n",(int32_t)vopret.size());
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

bool RewardsExactAmounts(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx,uint64_t txfee)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; uint64_t inputs=0,outputs=0,assetoshis;
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
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool RewardsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
{
    uint256 txid,fundingtxid,hashBlock; uint64_t sbits,APR,minseconds,maxseconds,mindeposit,amount,reward,txfee=10000; int32_t numvins,numvouts,preventCCvins,preventCCvouts,i; uint8_t funcid; CScript scriptPubKey; CTransaction fundingTx,vinTx;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        txid = tx.GetHash();
        if ( (funcid=  DecodeRewardsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid)) != 0 )
        {
            if ( eval->GetTxUnconfirmed(fundingtxid,fundingTx,hashBlock) == 0 )
                return eval->Invalid("cant find fundingtxid");
            else if ( fundingTx.vout.size() > 0 && DecodeRewardsFundingOpRet(fundingTx.vout[fundingTx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) != 'F' )
                return eval->Invalid("fundingTx not valid");
            switch ( funcid )
            {
                case 'F':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal marker vout for easy searching
                    //vout.2: normal change
                    //vout.n-1: opreturn 'F' sbits APR minseconds maxseconds mindeposit
                    return eval->Invalid("unexpected RewardsValidate for createfunding");
                    break;
                case 'A':
                    //vins.*: normal inputs
                    //vout.0: CC vout for funding
                    //vout.1: normal change
                    //vout.n-1: opreturn 'A' sbits fundingtxid
                    return eval->Invalid("unexpected RewardsValidate for addfunding");
                    break;
                case 'L':
                    //vins.*: normal inputs
                    //vout.0: CC vout for locked funds
                    //vout.1: normal output to unlock address
                    //vout.2: change
                    //vout.n-1: opreturn 'L' sbits fundingtxid
                    return eval->Invalid("unexpected RewardsValidate for lock");
                    break;
                case 'U':
                    //vin.0: locked funds CC vout.0 from lock
                    //vin.1+: funding CC vout.0 from 'F' and 'A' and 'U'
                    //vout.0: funding CC change
                    //vout.1: normal output to unlock address
                    //vout.n-1: opreturn 'U' sbits fundingtxid
                    for (i=0; i<numvins; i++)
                    {
                        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) == 0 )
                            return eval->Invalid("unexpected normal vin for unlock");
                    }
                    if ( RewardsExactAmounts(cp,eval,tx,txfee+tx.vout[1].nValue) == 0 )
                        return false;
                    else if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt");
                    else if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("lock tx vout.0 is normal output");
                    else if ( tx.vout.size() < 3 )
                        return eval->Invalid("unlock tx not enough vouts");
                    else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("unlock tx vout.0 is normal output");
                    else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() != 0 )
                        return eval->Invalid("unlock tx vout.1 is CC output");
                    else if ( tx.vout[1].scriptPubKey != vinTx.vout[1].scriptPubKey )
                        return eval->Invalid("unlock tx vout.1 mismatched scriptPubKey");
                    amount = vinTx.vout[0].nValue;
                    reward = RewardsCalc(amount,tx.vin[0].prevout.hash,APR,minseconds,maxseconds,mindeposit);
                    if ( tx.vout[1].nValue > amount+reward )
                        return eval->Invalid("unlock tx vout.1 isnt amount+reward");
                    preventCCvouts = 1;
                    break;
            }
        }
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}

// 'L' vs 'F' and 'A'
uint64_t AddRewardsInputs(CScript &scriptPubKey,int32_t fundsflag,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,uint64_t total,int32_t maxinputs)
{
    char coinaddr[64],str[65]; uint64_t sbits,nValue,totalinputs = 0; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t j,vout,n = 0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( it->second.satoshis < 1000000 )
            continue;
        fprintf(stderr,"(%s) %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout.size() > 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) != 0 )
            {
                fprintf(stderr,"fundsflag.%d (%c) %.8f %.8f\n",fundsflag,funcid,(double)tx.vout[vout].nValue/COIN,(double)it->second.satoshis/COIN);
                if ( fundsflag != 0 && funcid != 'F' && funcid != 'A' && funcid != 'U' )
                    continue;
                else if ( fundsflag == 0 && (funcid != 'L' || tx.vout.size() < 4) )
                    continue;
                if ( total != 0 && maxinputs != 0 )
                {
                    if ( fundsflag == 0 )
                        scriptPubKey = tx.vout[1].scriptPubKey;
                    mtx.vin.push_back(CTxIn(txid,vout,CScript()));
                }
                totalinputs += it->second.satoshis;
                n++;
                if ( (total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs) )
                    break;
            } else fprintf(stderr,"null funcid\n");
        }
    }
    return(totalinputs);
}

uint64_t RewardsPlanFunds(uint64_t refsbits,struct CCcontract_info *cp,CPubKey pk,uint256 reffundingtxid)
{
    char coinaddr[64]; uint64_t sbits,nValue,totalinputs = 0; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t vout; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) != 0 )
            {
                if ( (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    if ( refsbits == sbits && (nValue= IsRewardsvout(cp,tx,vout)) > 0 )
                        totalinputs += nValue;
                    else fprintf(stderr,"refsbits.%llx sbits.%llx nValue %.8f\n",(long long)refsbits,(long long)sbits,(double)nValue/COIN);
                } //else fprintf(stderr,"else case\n");
            } else fprintf(stderr,"funcid.%d %c skipped %.8f\n",funcid,funcid,(double)tx.vout[vout].nValue/COIN);
        }
    }
    return(totalinputs);
}

bool RewardsPlanExists(struct CCcontract_info *cp,uint64_t refsbits,CPubKey rewardspk,uint64_t &APR,uint64_t &minseconds,uint64_t &maxseconds,uint64_t &mindeposit)
{
    char CCaddr[64]; uint64_t sbits; uint256 txid,hashBlock; CTransaction tx;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    GetCCaddress(cp,CCaddr,rewardspk);
    SetCCtxids(txids,CCaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        //int height = it->first.blockHeight;
        txid = it->first.txhash;
        if ( GetTransaction(txid,tx,hashBlock,false) != 0 && tx.vout.size() > 0 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
        {
            //char str[65]; fprintf(stderr,"rewards plan %s\n",uint256_str(str,txid));
            if ( DecodeRewardsFundingOpRet(tx.vout[tx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) == 'F' )
            {
                if ( sbits == refsbits )
                    return(true);
            }
        }
    }
    return(false);
}

UniValue RewardsInfo(uint256 rewardsid)
{
    UniValue result(UniValue::VOBJ); uint256 hashBlock; CTransaction vintx; uint64_t APR,minseconds,maxseconds,mindeposit,sbits,funding; CPubKey rewardspk; struct CCcontract_info *cp,C; char str[67],numstr[65];
    if ( GetTransaction(rewardsid,vintx,hashBlock,false) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        result.push_back(Pair("error","cant find fundingtxid"));
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodeRewardsFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        fprintf(stderr,"fundingtxid isnt rewards creation txid\n");
        result.push_back(Pair("error","fundingtxid isnt rewards creation txid"));
        return(result);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("fundingtxid",uint256_str(str,rewardsid)));
    unstringbits(str,sbits);
    result.push_back(Pair("name",str));
    result.push_back(Pair("sbits",sbits));
    sprintf(numstr,"%.8f",(double)APR/COIN);
    result.push_back(Pair("APR",numstr));
    result.push_back(Pair("minseconds",minseconds));
    result.push_back(Pair("maxseconds",maxseconds));
    sprintf(numstr,"%.8f",(double)mindeposit/COIN);
    result.push_back(Pair("mindeposit",numstr));
    cp = CCinit(&C,EVAL_REWARDS);
    rewardspk = GetUnspendable(cp,0);
    funding = RewardsPlanFunds(sbits,cp,rewardspk,rewardsid);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

UniValue RewardsList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction vintx; uint64_t sbits,APR,minseconds,maxseconds,mindeposit; char str[65];
    cp = CCinit(&C,EVAL_REWARDS);
    SetCCtxids(addressIndex,cp->normaladdr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=addressIndex.begin(); it!=addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if ( GetTransaction(txid,vintx,hashBlock,false) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeRewardsFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string RewardsCreateFunding(uint64_t txfee,char *planstr,int64_t funds,int64_t APR,int64_t minseconds,int64_t maxseconds,int64_t mindeposit)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,a,b,c,d; struct CCcontract_info *cp,C;
    if ( funds < 0 || mindeposit < 0 || minseconds < 0 || maxseconds < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,a,b,c,d) != 0 )
    {
        fprintf(stderr,"Rewards plan (%s) already exists\n",planstr);
        return(0);
    }
    if ( AddNormalinputs(mtx,mypk,funds+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,rewardspk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(rewardspk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsFundingOpRet('F',sbits,APR,minseconds,maxseconds,mindeposit)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return(0);
}

std::string RewardsAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,a,b,c,d; struct CCcontract_info *cp,C;
    if ( amount < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,a,b,c,d) == 0 )
    {
        fprintf(stderr,"Rewards plan %s doesnt exist\n",planstr);
        return(0);
    }
    sbits = stringbits(planstr);
    if ( AddNormalinputs(mtx,mypk,amount+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,rewardspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsOpRet('A',sbits,fundingtxid)));
    } else fprintf(stderr,"cant find enough inputs\n");
    fprintf(stderr,"cant find fundingtxid\n");
    return(0);
}

std::string RewardsLock(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t deposit)
{
    CMutableTransaction mtx; CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,funding,APR,minseconds,maxseconds,mindeposit; struct CCcontract_info *cp,C;
    if ( deposit < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return(0);
    }
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        fprintf(stderr,"Rewards plan %s doesnt exist\n",planstr);
        return(0);
    }
    if ( deposit < mindeposit )
    {
        fprintf(stderr,"Rewards plan %s deposit %.8f < mindeposit %.8f\n",planstr,(double)deposit/COIN,(double)mindeposit/COIN);
        return(0);
    }
    if ( (funding= RewardsPlanFunds(sbits,cp,rewardspk,fundingtxid)) >= deposit ) // arbitrary cmpval
    {
        if ( AddNormalinputs(mtx,mypk,deposit+2*txfee,64) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,deposit,rewardspk));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsOpRet('L',sbits,fundingtxid)));
        } else fprintf(stderr,"cant find enough inputs %.8f note enough for %.8f\n",(double)funding/COIN,(double)deposit/COIN);
    }
    fprintf(stderr,"cant find rewards inputs\n");
    return(0);
}

std::string RewardsUnlock(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 locktxid)
{
    CMutableTransaction mtx; CTransaction tx; char coinaddr[64]; CPubKey mypk,rewardspk; CScript opret,scriptPubKey,ignore; uint256 hashBlock; uint64_t funding,sbits,reward=0,amount=0,inputs,CCchange=0,APR,minseconds,maxseconds,mindeposit; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    rewardspk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        fprintf(stderr,"Rewards plan %s doesnt exist\n",planstr);
        return(0);
    }
    fprintf(stderr,"APR %.8f minseconds.%llu maxseconds.%llu mindeposit %.8f\n",(double)APR/COIN,(long long)minseconds,(long long)maxseconds,(double)mindeposit/COIN);
    if ( locktxid == zeroid )
        amount = AddRewardsInputs(scriptPubKey,0,cp,mtx,rewardspk,(1LL << 30),1);
    else
    {
        GetCCaddress(cp,coinaddr,rewardspk);
        if ( (amount= CCutxovalue(coinaddr,locktxid,0)) == 0 )
        {
            fprintf(stderr,"%s locktxid/v0 is spent\n",coinaddr);
            return(0);
        }
        if ( GetTransaction(locktxid,tx,hashBlock,false) != 0 && tx.vout.size() > 0 && tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
        {
            scriptPubKey = tx.vout[1].scriptPubKey;
            mtx.vin.push_back(CTxIn(locktxid,0,CScript()));
        }
        else
        {
            fprintf(stderr,"%s no normal vout.1 in locktxid\n",coinaddr);
            return(0);
        }
    }
    if ( amount > 0 && (reward= RewardsCalc(amount,mtx.vin[0].prevout.hash,APR,minseconds,maxseconds,mindeposit)) > txfee && scriptPubKey.size() > 0 )
    {
        if ( (inputs= AddRewardsInputs(ignore,1,cp,mtx,rewardspk,reward+txfee,30)) > 0 )
        {
            if ( inputs >= (reward + 2*txfee) )
                CCchange = (inputs - (reward + txfee));
            fprintf(stderr,"inputs %.8f CCchange %.8f amount %.8f reward %.8f\n",(double)inputs/COIN,(double)CCchange/COIN,(double)amount/COIN,(double)reward/COIN);
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,rewardspk));
            mtx.vout.push_back(CTxOut(amount+reward,scriptPubKey));
            return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,EncodeRewardsOpRet('U',sbits,fundingtxid)));
        }
        fprintf(stderr,"cant find enough rewards inputs\n");
    }
    fprintf(stderr,"amount %.8f -> reward %.8f\n",(double)amount/COIN,(double)reward/COIN);
    return(0);
}

