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

#include "CClotto.h"
#include "../txmempool.h"

/*
 A blockchain lotto has the problem of generating the deterministic random numbers needed to get a winner in a way that doesnt allow cheating. If we save the entropy for later publishing and display the hash of the entropy, it is true that the players wont know what the entropy value is, however the creator of the lotto funds will be able to know and simply create a winning ticket when the jackpot is large enough.
 
 We also need to avoid chain reorgs from disclosing the entropy and then allowing people to submit a winning ticket calculated based on the disclosed entropy (see attack vector in dice.cpp)
 
 As usual it needs to be provably fair and random
 
 The solution is for everybody to post the hash of their entropy when purchasing tickets. Then at the time of the drawing, nodes would post their entropy over an N block period to avoid censorship attack. After the N block period, then we have valid entropy that we know was locked in prior to the start of the N blocks and that nobody would have been able to know ahead of time the final entropy value.
 
 As long as one node submits a high entropy value, then just by combining all the submissions together, we get the drawing's entropy value. Given that, the usual process can determine if there was a winner at the specified odds. In fact, all the nodes are able to determine exactly how many winners there were and whether to validate 1/w payouts to the w winners or rollover the jackpot to the next drawing.
 
 To remove the need for an autopayout, the winning node(s) would need to submit a 1/w payout tx, this would be able to be done at any time and the winner does not have to have submitted proof of hentropy. In order to prevent a player from opportunistically withholding their entropy, the lotto creator will post the original proof of hentropy after the N block player submission period. This masks to all the players the final value of entropy.
 
 Attack vector: the lotto creator can have many player tickets in reserve all with their entropy ready to submit, but based on the actual submissions, find the one which gives him the best outcome. since all the player submissions will be known via mempool, along with the original hentropy. However the lotto creator would have to mine the final block in order to know the order of the player tickets.
 
 Thinking about this evil miner attack, it seems pretty bad, so a totally new approach is needed. Preferably with a simple enough protocol. Let us remove any special knowledge by the lotto creator, so like the faucet, it seems just that there is a single lotto for a chain.
 
 >>>>>>>>>>>> second iteration
 
 What we need is something that gives each ticket an equal chance at the jackpot, without allowing miner or relayer to gain an advantage. ultimately the jackpot payout tx needs to be confirmed, so there needs to be some number of blocks to make a claim to avoid censorship attack. If onchain entropy is needed, then it should be reduced to 1 bit per block to reduce the grinding that is possible. This does mean a block miner for the last bit of entropy can double their chances at winning, but the alternative is to have an external source of entropy, which creates its own set of issues like what prevents the nodes getting the external entropy from cheating?
 
 Conveniently the lotto mechanics are similar to a PoS staking, so it can be based on everybody trying to stake a single lotto jackpot.
 
 The calculation would need to be based on the payout address and utxosize, so relayers cant intercept it to steal the jackpot.
 
 each jackpot would effectively restart the lotto
 
 the funds from new lotto tickets can be spent by the jackpot, but those tickets can still win the new jackpot
 
 each set of tickets (utxo) would become eligible to claim the jackpot after some time is elapsed so the entropy for that utxo can be obtained. [6 bits * 32 + 1 bit * 16] 48 blocks
 
It is possible to have a jackpot but miss out on it due to not claiming it. To minimize the effect from this, each ticket would have one chance to win, which can be calculated and a jackpot claim submitted just once.
 
 in order to randomize the timing of claim, a txid PoW similar to faucetget will maximize the chance of only a single jackpot txid that can propagate throughout the mempools, which will prevent the second one broadcast. Granted the mining node can override this if they also have a winning ticket, but assuming the PoS lottery makes it unlikely for two winners in a single block, this is not a big issue.
 
 In order to adapt the difficulty of winning the lotto, but not requiring recalculating all past tickets, as new lotto tickets are sold without a jackpot, it needs to become easier to win. Basically as the lotto jackpot gets bigger and bigger, it keeps getting easier to win! This convergence will avoid having unwinnable jackpots.
 
 rpc calls
 lottoinfo
 lottotickets <numtickets>
 lottostatus
 lottowinner tickethash ticketid

*/

// start of consensus code

int64_t IsLottovout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool LottoExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Lotto from mempool");
                if ( (assetoshis= IsLottovout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsLottovout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool LottoValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
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
                fprintf(stderr,"Lottoget invalid vini\n");
                return eval->Invalid("illegal normal vini");
            }
        }
        //fprintf(stderr,"check amounts\n");
        if ( LottoExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Lottoget invalid amount\n");
            return false;
        }
        else
        {
            preventCCvouts = 1;
            if ( IsLottovout(cp,tx,0) != 0 )
            {
                preventCCvouts++;
                i = 1;
            } else i = 0;
            if ( tx.vout[i].nValue != COIN )
                return eval->Invalid("invalid Lotto output");
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Lottoget validated\n");
            else fprintf(stderr,"Lottoget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddLottoInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
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
        if ( it->second.satoshis < COIN )
            continue;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( (nValue= IsLottovout(cp,vintx,(int32_t)it->first.index)) > 0 )
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

uint8_t DecodeLottoFundingOpRet(const CScript &scriptPubKey,uint64_t &sbits,int32_t ticketsize,int32_t odds,int32_t firstheight,int32_t period,uint256 hentropy)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> sbits; ss >> ticketsize; ss >> odds; ss >> firstheight; ss >> period; ss >> hentropy) != 0 )
    {
        if ( e == EVAL_LOTTO && f == 'F' )
            return(f);
    }
    return(0);
}

int64_t LottoPlanFunds(uint64_t refsbits,struct CCcontract_info *cp,CPubKey pk,uint256 reffundingtxid)
{
    char coinaddr[64]; uint64_t sbits; int64_t nValue,lockedfunds; uint256 txid,hashBlock,fundingtxid; CTransaction tx; int32_t vout; uint8_t funcid;
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
            // need to implement this! if ( (funcid= DecodeLottoOpRet(txid,tx.vout[tx.vout.size()-1].scriptPubKey,sbits,fundingtxid)) == 'F' || funcid == 'T' )
            {
                if ( refsbits == sbits && (funcid == 'F' && reffundingtxid == txid) || reffundingtxid == fundingtxid )
                {
                    if ( (nValue= IsLottovout(cp,tx,vout)) > 0 )
                        lockedfunds += nValue;
                    else fprintf(stderr,"refsbits.%llx sbits.%llx nValue %.8f\n",(long long)refsbits,(long long)sbits,(double)nValue/COIN);
                } //else fprintf(stderr,"else case\n");
            } //else fprintf(stderr,"funcid.%d %c skipped %.8f\n",funcid,funcid,(double)tx.vout[vout].nValue/COIN);
        }
    }
    return(lockedfunds);
}

UniValue LottoInfo(uint256 lottoid)
{
    UniValue result(UniValue::VOBJ); uint256 hashBlock,hentropy; CTransaction vintx; uint64_t lockedfunds,sbits; int32_t ticketsize,odds,firstheight,period; CPubKey lottopk; struct CCcontract_info *cp,C; char str[67],numstr[65];
    if ( myGetTransaction(lottoid,vintx,hashBlock) == 0 )
    {
        fprintf(stderr,"cant find lottoid\n");
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","cant find lottoid"));
        return(result);
    }
    if ( vintx.vout.size() > 0 && DecodeLottoFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,ticketsize,odds,firstheight,period,hentropy) == 0 )
    {
        fprintf(stderr,"lottoid isnt lotto creation txid\n");
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","lottoid isnt lotto creation txid"));
        return(result);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("lottoid",uint256_str(str,lottoid)));
    unstringbits(str,sbits);
    result.push_back(Pair("name",str));
    result.push_back(Pair("sbits",sbits));
    result.push_back(Pair("ticketsize",ticketsize));
    result.push_back(Pair("odds",odds));
    cp = CCinit(&C,EVAL_LOTTO);
    lottopk = GetUnspendable(cp,0);
    lockedfunds = LottoPlanFunds(sbits,cp,lottopk,lottoid);
    sprintf(numstr,"%.8f",(double)lockedfunds/COIN);
    result.push_back(Pair("jackpot",numstr));
    return(result);
}

UniValue LottoList()
{
    UniValue result(UniValue::VARR); std::vector<uint256> txids; struct CCcontract_info *cp,C; uint256 txid,hashBlock,hentropy; CTransaction vintx; uint64_t sbits; int32_t ticketsize,odds,firstheight,period; char str[65];
    cp = CCinit(&C,EVAL_LOTTO);
    SetCCtxids(txids,cp->normaladdr,true,cp->evalcode,zeroid,'F');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeLottoFundingOpRet(vintx.vout[vintx.vout.size()-1].scriptPubKey,sbits,ticketsize,odds,firstheight,period,hentropy) == 'F' )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

std::string LottoCreate(uint64_t txfee,char *planstr,int64_t funding,int32_t ticketsize,int32_t odds,int32_t firstheight,int32_t period)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    uint256 entropy,hentropy; CPubKey mypk,lottopk; uint64_t sbits; int64_t inputs,CCchange=0,nValue=COIN; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( txfee == 0 )
        txfee = 10000;
    lottopk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    sbits = stringbits(planstr);
    if ( AddNormalinputs(mtx,mypk,funding+txfee,60) > 0 )
    {
        hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
        mtx.vout.push_back(MakeCC1vout(EVAL_LOTTO,funding,lottopk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_LOTTO << (uint8_t)'F' << sbits << ticketsize << odds << firstheight << period << hentropy)));
    }
    return("");
}

std::string LottoTicket(uint64_t txfee,uint256 lottoid,int64_t numtickets)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,lottopk; CScript opret; int64_t inputs,CCchange=0,nValue=COIN; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( txfee == 0 )
        txfee = 10000;
    lottopk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddLottoInputs(cp,mtx,lottopk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_LOTTO,CCchange,lottopk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(-1LL,cp,mtx,mypk,txfee,opret));
    } else fprintf(stderr,"cant find Lotto inputs\n");
    return("");
}

std::string LottoWinner(uint64_t txfee)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,lottopk; int64_t winnings = 0; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_LOTTO);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    lottopk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_LOTTO,winnings,lottopk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}


