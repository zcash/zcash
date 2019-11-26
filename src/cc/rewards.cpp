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

#include "CCrewards.h"

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
 
/// the following are compatible with windows
/// mpz_set_lli sets a long long singed int to a big num mpz_t for very large integer math
extern void mpz_set_lli( mpz_t rop, long long op );
// mpz_get_si2 gets a mpz_t and returns a signed long long int
extern int64_t mpz_get_si2( mpz_t op );
// mpz_get_ui2 gets a mpz_t and returns a unsigned long long int
extern uint64_t mpz_get_ui2( mpz_t op );
 
uint64_t RewardsCalc(int64_t amount, uint256 txid, int64_t APR, int64_t minseconds, int64_t maxseconds, uint32_t timestamp)
{
    int32_t numblocks; int64_t duration; uint64_t reward = 0;
    //fprintf(stderr,"minseconds %llu maxseconds %llu\n",(long long)minseconds,(long long)maxseconds);
    if ( (duration= CCduration(numblocks,txid)) < minseconds )
    {
        fprintf(stderr,"duration %lli < minseconds %lli\n",(long long)duration,(long long)minseconds);
        return(0);
        //duration = (uint32_t)time(NULL) - (1532713903 - 3600 * 24);
    } else if ( duration > maxseconds )
        duration = maxseconds;
    /* if ( 0 ) // amount * APR * duration / COIN * 100 * 365*24*3600
        reward = (((amount * APR) / COIN) * duration) / (365*24*3600LL * 100);
    else reward = (((amount * duration) / (365 * 24 * 3600LL)) * (APR / 1000000)) / 10000;
    */
    if ( !komodo_hardfork_active(timestamp) )
        reward = (((amount * duration) / (365 * 24 * 3600LL)) * (APR / 1000000)) / 10000;
    else 
    {
        // declare and init the mpz_t big num variables 
        mpz_t mpzAmount, mpzDuration, mpzReward, mpzAPR, mpzModifier;
        mpz_init(mpzAmount);
        mpz_init(mpzDuration);
        mpz_init(mpzAPR);
        mpz_init(mpzReward);
        mpz_init(mpzModifier);

        // set the inputs to big num variables
        mpz_set_lli(mpzAmount, amount);
        mpz_set_lli(mpzDuration, duration);
        mpz_set_lli(mpzAPR, APR);
        mpz_set_lli(mpzModifier, COIN*100*365*24*3600LL);

        // (amount * APR * duration)
        mpz_mul(mpzReward, mpzAmount, mpzDuration);
        mpz_mul(mpzReward, mpzReward, mpzAPR);

        // total_of_above / (COIN * 100 * 365*24*3600LL)
        mpz_tdiv_q(mpzReward, mpzReward, mpzModifier);

        // set result to variable we can use and return it.
        reward = mpz_get_ui2(mpzReward);
    }
    if ( reward > amount )
        reward = amount;
    fprintf(stderr, "amount.%lli duration.%lli APR.%lli reward.%llu\n", (long long)amount, (long long)duration, (long long)APR, (long long)reward);
    //fprintf(stderr,"amount %.8f %.8f %llu -> duration.%llu reward %.8f vals %.8f %.8f\n",(double)amount/COIN,((double)amount * APR)/COIN,(long long)((amount * APR) / (COIN * 365*24*3600)),(long long)duration,(double)reward/COIN,(double)((amount * duration) / (365 * 24 * 3600LL))/COIN,(double)(((amount * duration) / (365 * 24 * 3600LL)) * (APR / 1000000))/COIN);
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

int64_t IsRewardsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v,uint64_t refsbits,uint256 reffundingtxid)
{
    char destaddr[64]; uint64_t sbits; uint256 fundingtxid,txid; uint8_t funcid; int32_t numvouts;
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 && (numvouts= (int32_t)tx.vout.size()) > 0 )
    {
        txid = tx.GetHash();
        if ( (funcid=  DecodeRewardsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid)) != 0 && sbits == refsbits && (fundingtxid == reffundingtxid || txid == reffundingtxid) )
        {
            
            if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
                return(tx.vout[v].nValue);
        }
    }
    return(0);
}

bool RewardsExactAmounts(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx,uint64_t txfee,uint64_t refsbits,uint256 reffundingtxid)
{
    static uint256 zerohash;
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
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
                if ( (assetoshis= IsRewardsvout(cp,vinTx,tx.vin[i].prevout.n,refsbits,reffundingtxid)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsRewardsvout(cp,tx,i,refsbits,reffundingtxid)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu txfee %llu\n",(long long)inputs,(long long)outputs,(long long)txfee);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool RewardsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    uint256 txid,fundingtxid,hashBlock,vinfundingtxid; uint64_t vinsbits,sbits,APR,minseconds,maxseconds,mindeposit,amount,reward,txfee=10000; int32_t numvins,numvouts,preventCCvins,preventCCvouts,i; uint8_t funcid; CScript scriptPubKey; CTransaction fundingTx,vinTx;
    int64_t dummy;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        txid = tx.GetHash();
        if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,sbits,fundingtxid)) != 0 )
        {
            if ( eval->GetTxUnconfirmed(fundingtxid,fundingTx,hashBlock) == 0 )
                return eval->Invalid("cant find fundingtxid");
            else if ( fundingTx.vout.size() > 0 && DecodeRewardsFundingOpRet(fundingTx.vout[fundingTx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) != 'F' )
                return eval->Invalid("fundingTx not valid");
            if ( APR > REWARDSCC_MAXAPR )
                return eval->Invalid("excessive APR");
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
                    //vout.0: funding CC change or recover normal payout
                    //vout.1: normal output to unlock address
                    //vout.n-1: opreturn 'U' sbits fundingtxid
                    if ( fundingtxid == tx.vin[0].prevout.hash )
                        return eval->Invalid("cant unlock fundingtxid");
                    else if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                        return eval->Invalid("always should find vin.0, but didnt");
                    else if ( DecodeRewardsOpRet(tx.vin[0].prevout.hash,vinTx.vout[vinTx.vout.size()-1].scriptPubKey,vinsbits,vinfundingtxid) != 'L' )
                        return eval->Invalid("can only unlock locktxid");
                    else if ( fundingtxid != vinfundingtxid )
                        return eval->Invalid("mismatched vinfundingtxid");
                    for (i=0; i<numvins; i++)
                    {
                        if ( (*cp->ismyvin)(tx.vin[i].scriptSig) == 0 )
                            return eval->Invalid("unexpected normal vin for unlock");
                    }
                    if ( !CheckTxFee(tx, txfee, chainActive.LastTip()->GetHeight(), chainActive.LastTip()->nTime, dummy) )
                        return eval->Invalid("txfee is too high");
                    amount = vinTx.vout[0].nValue;
                    reward = RewardsCalc((int64_t)amount,tx.vin[0].prevout.hash,(int64_t)APR,(int64_t)minseconds,(int64_t)maxseconds,GetLatestTimestamp(eval->GetCurrentHeight()));
                    if ( reward == 0 )
                        return eval->Invalid("no eligible rewards");
                    if ( numvins == 1 && tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                    {
                        if ( tx.vout[1].nValue != 10000 )
                            return eval->Invalid("wrong marker vout value");
                        else if ( tx.vout[1].scriptPubKey != tx.vout[0].scriptPubKey )
                            return eval->Invalid("unlock recover tx vout.1 mismatched scriptPubKey");
                        else if ( tx.vout[0].scriptPubKey != vinTx.vout[1].scriptPubKey )
                            return eval->Invalid("unlock recover tx vout.0 mismatched scriptPubKey");
                        else if ( tx.vout[0].nValue > vinTx.vout[0].nValue )
                            return eval->Invalid("unlock recover tx vout.0 mismatched amounts");
                        else if ( tx.vout[2].nValue > 0 )
                            return eval->Invalid("unlock recover tx vout.1 nonz amount");
                        else return(true);
                    }
                    if ( vinTx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("unlock tx vout.0 is normal output");
                    else if ( numvouts != 3 )
                        return eval->Invalid("unlock tx wrong number of vouts");
                    else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                        return eval->Invalid("unlock tx vout.0 is normal output");
                    else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() != 0 )
                        return eval->Invalid("unlock tx vout.1 is CC output");
                    else if ( tx.vout[1].scriptPubKey != vinTx.vout[1].scriptPubKey )
                        return eval->Invalid("unlock tx vout.1 mismatched scriptPubKey");
                    if ( RewardsExactAmounts(cp,eval,tx,txfee+tx.vout[1].nValue,sbits,fundingtxid) == 0 )
                        return false;
                    else if ( tx.vout[1].nValue > amount+reward )
                        return eval->Invalid("unlock tx vout.1 isnt amount+reward");
                    else if ( tx.vout[2].nValue > 0 )
                        return eval->Invalid("unlock tx vout.2 isnt 0");
                    preventCCvouts = 1;
                    break;
                default:
                    fprintf(stderr,"illegal rewards funcid.(%c)\n",funcid);
                    return eval->Invalid("unexpected rewards funcid");
                    break;
            }
        } else return eval->Invalid("unexpected rewards missing funcid");
        return(PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts));
    }
    return(true);
}

static uint64_t myIs_unlockedtx_inmempool(uint256 &txid,int32_t &vout,uint64_t refsbits,uint256 reffundingtxid,uint64_t needed)
{
    uint8_t funcid; uint64_t sbits,nValue; uint256 fundingtxid; char str[65];
    memset(&txid,0,sizeof(txid));
    vout = -1;
    nValue = 0;
    BOOST_FOREACH(const CTxMemPoolEntry &e,mempool.mapTx)
    {
        const CTransaction &tx = e.GetTx();
        if ( tx.vout.size() > 0 && tx.vout[0].nValue >= needed )
        {
            const uint256 &hash = tx.GetHash();
            if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,hash,0) == 0 )
            {
                if ( (funcid= DecodeRewardsOpRet(hash,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) == 'U' && sbits == refsbits && fundingtxid == reffundingtxid )
                {
                    txid = hash;
                    vout = 0;
                    nValue = tx.vout[0].nValue;
                    fprintf(stderr,"found 'U' %s %.8f in unspent in mempool\n",uint256_str(str,txid),(double)nValue/COIN);
                    return(nValue);
                }
            }
        }
    }
    return(nValue);
}

// 'L' vs 'F' and 'A'
int64_t AddRewardsInputs(CScript &scriptPubKey,uint64_t maxseconds,struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs,uint64_t refsbits,uint256 reffundingtxid)
{
    char coinaddr[64],str[65]; uint64_t threshold,sbits,nValue,totalinputs = 0; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t numblocks,j,vout,n = 0; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    GetCCaddress(cp,coinaddr,pk);
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
        //fprintf(stderr,"(%s) %s/v%d %.8f\n",coinaddr,uint256_str(str,txid),vout,(double)it->second.satoshis/COIN);
        for (j=0; j<mtx.vin.size(); j++)
            if ( txid == mtx.vin[j].prevout.hash && vout == mtx.vin[j].prevout.n )
                break;
        if ( j != mtx.vin.size() )
            continue;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
        {
            if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) != 0 )
            {
                if ( sbits != refsbits || fundingtxid != reffundingtxid )
                    continue;
                if ( maxseconds == 0 && funcid != 'F' && funcid != 'A' && funcid != 'U' )
                    continue;
                else if ( maxseconds != 0 && funcid != 'L' )
                {
                    if ( CCduration(numblocks,txid) < maxseconds )
                        continue;
                }
                fprintf(stderr,"maxseconds.%d (%c) %.8f %.8f\n",(int32_t)maxseconds,funcid,(double)tx.vout[vout].nValue/COIN,(double)it->second.satoshis/COIN);
                if ( total != 0 && maxinputs != 0 )
                {
                    if ( maxseconds != 0 )
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
    if ( maxseconds == 0 && totalinputs < total && (maxinputs == 0 || n < maxinputs-1) )
    {
        fprintf(stderr,"search mempool for unlocked and unspent CC rewards output for %.8f\n",(double)(total-totalinputs)/COIN);
        if ( (nValue= myIs_unlockedtx_inmempool(txid,vout,refsbits,reffundingtxid,total-totalinputs)) > 0 )
        {
            mtx.vin.push_back(CTxIn(txid,vout,CScript()));
            fprintf(stderr,"added mempool vout for %.8f\n",(double)nValue/COIN);
            totalinputs += nValue;
            n++;
        }
    }
    return(totalinputs);
}

int64_t RewardsPlanFunds(uint64_t &lockedfunds,uint64_t refsbits,struct CCcontract_info *cp,CPubKey pk,uint256 reffundingtxid)
{
    char coinaddr[64]; uint64_t sbits; int64_t nValue,totalinputs = 0; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t vout; uint8_t funcid;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    lockedfunds = 0;
    GetCCaddress(cp,coinaddr,pk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout[vout].scriptPubKey.IsPayToCryptoCondition() != 0 )
        {
            if ( (funcid= DecodeRewardsOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) == 'F' || funcid == 'A' || funcid == 'U' || funcid == 'L' )
            {
                if ( refsbits == sbits && (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    if ( (nValue= IsRewardsvout(cp,tx,vout,sbits,fundingtxid)) > 0 )
                    {
                        if ( funcid == 'L' )
                            lockedfunds += nValue;
                        else totalinputs += nValue;
                    }
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
    std::vector<uint256> txids;
    GetCCaddress(cp,CCaddr,rewardspk);
    SetCCtxids(txids,CCaddr,true,cp->evalcode,zeroid,'F');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        //int height = it->first.blockHeight;
        txid = *it;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && tx.vout.size() > 0 && ConstrainVout(tx.vout[0],1,CCaddr,0) != 0 )
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
    UniValue result(UniValue::VOBJ); uint256 hashBlock; CTransaction vintx; uint64_t lockedfunds,APR,minseconds,maxseconds,mindeposit,sbits,funding; CPubKey rewardspk; struct CCcontract_info *cp,C; char str[67],numstr[65];
    if ( myGetTransaction(rewardsid,vintx,hashBlock) == 0 )
    {
        fprintf(stderr,"cant find fundingtxid\n");
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","cant find fundingtxid"));
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodeRewardsFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        fprintf(stderr,"fundingtxid isnt rewards creation txid\n");
        result.push_back(Pair("result","error"));
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
    funding = RewardsPlanFunds(lockedfunds,sbits,cp,rewardspk,rewardsid);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    sprintf(numstr,"%.8f",(double)lockedfunds/COIN);
    result.push_back(Pair("locked",numstr));
    return(result);
}

UniValue RewardsList()
{
    UniValue result(UniValue::VARR); std::vector<uint256> txids; struct CCcontract_info *cp,C; uint256 txid,hashBlock; CTransaction vintx; uint64_t sbits,APR,minseconds,maxseconds,mindeposit; char str[65];
    cp = CCinit(&C,EVAL_REWARDS);
    SetCCtxids(txids,cp->normaladdr,false,cp->evalcode,zeroid,'F');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,a,b,c,d; struct CCcontract_info *cp,C;
    if ( funds < COIN || mindeposit < 0 || minseconds < 0 || maxseconds < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return("");
    }
    if ( APR > REWARDSCC_MAXAPR )
    {
        fprintf(stderr,"25%% APR is maximum\n");
        return("");
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
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,funds+2*txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,funds,rewardspk));
        mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(rewardspk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsFundingOpRet('F',sbits,APR,minseconds,maxseconds,mindeposit)));
    }
    fprintf(stderr,"cant find enough inputs\n");
    return("");
}

std::string RewardsAddfunding(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,rewardspk; CScript opret; uint64_t sbits,a,b,c,d; struct CCcontract_info *cp,C;
    if ( amount < 0 )
    {
        fprintf(stderr,"negative parameter error\n");
        return("");
    }
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,a,b,c,d) == 0 )
    {
        CCerror = strprintf("Rewards plan %s doesnt exist\n",planstr);
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    sbits = stringbits(planstr);
    if ( AddNormalinputs(mtx,mypk,amount+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,amount,rewardspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsOpRet('A',sbits,fundingtxid)));
    } else {
        CCerror = "cant find enough inputs";
        fprintf(stderr,"%s\n", CCerror.c_str());
    }
    CCerror = "cant find fundingtxid";
    fprintf(stderr,"%s\n", CCerror.c_str());
    return("");
}

std::string RewardsLock(uint64_t txfee,char *planstr,uint256 fundingtxid,int64_t deposit)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,rewardspk; CScript opret; uint64_t lockedfunds,sbits,funding,APR,minseconds,maxseconds,mindeposit; struct CCcontract_info *cp,C;
    if ( deposit < txfee )
    {
        CCerror = "deposit amount less than txfee";
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    rewardspk = GetUnspendable(cp,0);
    sbits = stringbits(planstr);
    if ( RewardsPlanExists(cp,sbits,rewardspk,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        CCerror = strprintf("Rewards plan %s doesnt exist\n",planstr);
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    if ( deposit < mindeposit )
    {
        CCerror = strprintf("Rewards plan %s deposit %.8f < mindeposit %.8f\n",planstr,(double)deposit/COIN,(double)mindeposit/COIN);
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    if ( (funding= RewardsPlanFunds(lockedfunds,sbits,cp,rewardspk,fundingtxid)) >= deposit ) // arbitrary cmpval
    {
        if ( AddNormalinputs(mtx,mypk,deposit+2*txfee,64) > 0 )
        {
            mtx.vout.push_back(MakeCC1vout(cp->evalcode,deposit,rewardspk));
            mtx.vout.push_back(CTxOut(txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeRewardsOpRet('L',sbits,fundingtxid)));
        } else {
            CCerror = strprintf("cant find enough inputs %.8f not enough for %.8f, make sure you imported privkey for the -pubkey address\n",(double)funding/COIN,(double)deposit/COIN);
            fprintf(stderr,"%s\n",CCerror.c_str());
        }
    }
    fprintf(stderr,"cant find rewards inputs funding %.8f locked %.8f vs deposit %.8f\n",(double)funding/COIN,(double)lockedfunds/COIN,(double)deposit/COIN);
    return("");
}

std::string RewardsUnlock(uint64_t txfee,char *planstr,uint256 fundingtxid,uint256 locktxid)
{
    CMutableTransaction firstmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction tx; char coinaddr[64]; CPubKey mypk,rewardspk; CScript scriptPubKey,ignore; uint256 hashBlock; uint64_t sbits,APR,minseconds,maxseconds,mindeposit; int64_t funding,reward=0,amount=0,inputs,CCchange=0; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_REWARDS);
    if ( txfee == 0 )
        txfee = 10000;
    rewardspk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    sbits = stringbits(planstr);
    if ( locktxid == fundingtxid )
    {
        fprintf(stderr,"Rewards plan cant unlock fundingtxid\n");
        CCerror = "Rewards plan cant unlock fundingtxid";
        return("");
    }
    if ( RewardsPlanExists(cp,sbits,rewardspk,APR,minseconds,maxseconds,mindeposit) == 0 )
    {
        fprintf(stderr,"Rewards plan %s doesnt exist\n",planstr);
        CCerror = "Rewards plan does not exist";
        return("");
    }
    fprintf(stderr,"APR %.8f minseconds.%llu maxseconds.%llu mindeposit %.8f\n",(double)APR/COIN,(long long)minseconds,(long long)maxseconds,(double)mindeposit/COIN);
    if ( locktxid == zeroid )
        amount = AddRewardsInputs(scriptPubKey,maxseconds,cp,mtx,rewardspk,(1LL << 30),1,sbits,fundingtxid);
    else
    {
        GetCCaddress(cp,coinaddr,rewardspk);
        if ( (amount= CCutxovalue(coinaddr,locktxid,0,1)) == 0 )
        {
            fprintf(stderr,"%s locktxid/v0 is spent\n",coinaddr);
            CCerror = "locktxid/v0 is spent";
            return("");
        }
        if ( myGetTransaction(locktxid,tx,hashBlock) != 0 && tx.vout.size() > 0 && tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
        {
            scriptPubKey = tx.vout[1].scriptPubKey;
            mtx.vin.push_back(CTxIn(locktxid,0,CScript()));
        }
        else
        {
            fprintf(stderr,"%s no normal vout.1 in locktxid\n",coinaddr);
            CCerror = "no normal vout.1 in locktxid";
            return("");
        }
    }
    if ( amount > txfee )
    {
        reward = RewardsCalc((int64_t)amount,mtx.vin[0].prevout.hash,(int64_t)APR,(int64_t)minseconds,(int64_t)maxseconds,komodo_chainactive_timestamp());
        if ( scriptPubKey.size() > 0 )
        {
            if ( reward > txfee )
            {
                firstmtx = mtx;
                if ( (inputs= AddRewardsInputs(ignore,0,cp,mtx,rewardspk,reward+txfee,30,sbits,fundingtxid)) >= reward+txfee )
                {
                    if ( inputs >= (reward + 2*txfee) )
                        CCchange = (inputs - (reward + txfee));
                    fprintf(stderr,"inputs %.8f CCchange %.8f amount %.8f reward %.8f\n",(double)inputs/COIN,(double)CCchange/COIN,(double)amount/COIN,(double)reward/COIN);
                    mtx.vout.push_back(MakeCC1vout(cp->evalcode,CCchange,rewardspk));
                    mtx.vout.push_back(CTxOut(amount+reward,scriptPubKey));
                    return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,EncodeRewardsOpRet('U',sbits,fundingtxid)));
                }
                else
                {
                    firstmtx.vout.push_back(CTxOut(amount-txfee*2,scriptPubKey));
                    fprintf(stderr,"not enough rewards funds to payout %.8f, recover mode tx\n",(double)(reward+txfee)/COIN);
                    return(FinalizeCCTx(-1LL,cp,firstmtx,mypk,txfee,EncodeRewardsOpRet('U',sbits,fundingtxid)));
                }
            }
            else
            {
                CCerror = strprintf("reward %.8f is <= the transaction fee", reward);
                fprintf(stderr,"%s\n", CCerror.c_str());
            }
        }
        else
        {
            CCerror = "invalid scriptPubKey";
            fprintf(stderr,"%s\n", CCerror.c_str());
        }
    }
    else
    {
        CCerror = "amount must be more than txfee";
        fprintf(stderr,"%s\n", CCerror.c_str());
    }
    fprintf(stderr,"amount %.8f -> reward %.8f\n",(double)amount/COIN,(double)reward/COIN);
    return("");
}
