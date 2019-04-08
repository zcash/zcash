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

#include "CCPegs.h"

/*
pegs CC is able to create a coin backed (by any supported coin with gateways CC deposits) and pegged to any synthetic price that is able to be calculated based on prices CC
 
 First, the prices CC needs to be understood, so the extensive comments at the top of ~/src/cc/prices.cpp needs to be understood.
 
 The second aspect is the ability to import coins, as used by the crosschain burn/import and the -ac_import chains.
 
 <one hour later...>
 
 OK, now we are ready to describe the pegs CC. Let us imagine an -ac_import sidechain with KMD gateways CC. Now we have each native coin fungible with the real KMD via the gateways deposit/withdraw mechanism. Let us start with that and make a pegged and backed USD chain.
 
 <alternatively we can start from a CC enabled chain with external value>
 
 Here the native coin is KMD, but we want USD, so there needs to be a way to convert the KMD amounts into USD amounts. Something like "KMDBTC, BTCUSD, *, 1" which is the prices CC syntax to calculate KMD/USD, which is exactly what we need. So now we can assume that we have a block by block usable KMD/USD price. implementationwise, there can be an -ac option like -ac_peg="KMDBTC, BTCUSD, *, 1" and in conjunction with -ac_import=KMD gateways CC sidechain, we now have a chain where deposit of KMD issues the correct USD coins and redeem of USD coins releases the correct number of KMD coins.
 
 Are we done yet?
 
 Not quite, as the prices of KMD will be quite volatile relative to USD, which is good during bull markets, not so happy during bear markets. There are 2 halves to this problem, how to deal with massive price increase (easy to solve), how to solve 90% price drop (a lot harder).
 
 In order to solve both, what is needed is an "account" based tracking which updates based on both price change, coins issued, payments made. So let us create an account that is based on a specific pubkey, where all relevant deposits, issuances, withdraws are tracked via appropriate vin/vout markers.
 
 Let us modify the USD chain above so that only 80% of the possible USD is issued and 20% held in reserve. This 80% should be at least some easily changeable #define, or even an -ac parameter. We want the issued coins to be released without any encumberances, but the residual 20% value is still controlled (owned) but the depositor. This account has the amount of KMD deposited and USD issued. At the moment of deposit, there will still be 20% equity left. Let us start with 1000 KMD deposit, $1.5 per KMD -> 800 KMD converted to 1200 USD into depositor pubkey and the account of (1000 KMD, -1200 USD) = 200 KMD or $300 equity.
 
 Now it becomes easy for the bull market case, which is to allow (for a fee like 1%) issuance of more USD as the equity increases, so let us imagine KMD at $10:
 
 (1000 KMD, -1200 USD, 200KMD reserve) -> $2000 equity, issue 80% -> $1600 using 160 KMD
 (1000 KMD, -1200 USD, 200KMD reserve, -160KMD, issue $1600 USD, 40 KMD reserve)
 
 we have $2800 USD in circulation, 40 KMD reserve left against $10000 marketcap of the original deposit. It it easy to see that there are never any problems with lack of KMD to redeem the issued USD in a world where prices only go up. Total USD issuance can be limited by using a decentralized account tracking based on each deposit.
 
 What is evident though is that with the constantly changing price and the various times that all the various deposits issue USD, the global reserves are something that will be hard to predict and in fact needs to be specifically tracked. Let us combine all accounts exposure in to a global reserves factor. This factor will control various max/min/ allowed and fee percentages.
 
 Now we are prepared to handle the price goes down scenario. We can rely on the global equity/reserve ratio to be changing relatively slowly as the reference price is the smooted trustless oracles price. This means there will be enough blocks to adjust the global reserves percentage. What we need to do is liquidate specific positions that have the least reserves.
 
 What does liquidation mean? It means a specific account will be purchased at below its current value and the KMD withdrawn. Let us assume the price drops to $5:
 
 (1000 KMD, -1200 USD, 200KMD reserve, -160KMD, issue $1600 USD, 40 KMD reserve) 1000 KMD with 2800 USD issued so $2200 reserves. Let us assume it can be liquidated at a 10% discount, so for $2000 in addition to the $2800, the 5000 KMD is able to be withdrawn. This removes 4800 USD coins for 1000 KMD, which is a very low reserve amount of 4%. If a low reserve amount is removed from the system, then the global reserve amount must be improved.
 
 Now, we have a decentralized mechanism to handle the price going lower! Combined with the fully decentralized method new USD coins are issued, makes this argubably the first decentralized blockchain that is both backed and pegged. There is the reliance on the gateways CC multisig signers, so there is a fundamental federated trust for chains without intrinsic value.
 
 Also, notice that the flexibly syntax of prices CC allows to define pegs easily for virtually any type of synthetic, and all the ECB fiats can easily get a backed and pegged coin.
 
 Let us now consider how to enforce a peg onto a specific gateways CC token. If this can also be achieved, then a full DEX for all the different gateways CC supported coins can be created onto a single fiat denominated chain.
 
 

 
 */

// start of consensus code

int64_t IsPegsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool PegsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Pegs from mempool");
                if ( (assetoshis= IsPegsvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsPegsvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool PegsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks; bool retval; uint256 txid; uint8_t hash[32]; char str[65],destaddr[64];
    return eval->Invalid("no validation yet");
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
        if ( PegsExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Pegsget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Pegsget validated\n");
            else fprintf(stderr,"Pegsget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPegsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
{
    // add threshold check
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
            if ( (nValue= IsPegsvout(cp,vintx,vout)) > 1000000 && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0 )
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

std::string PegsGet(uint64_t txfee,int64_t nValue)
{
    CMutableTransaction tmpmtx,mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,Pegspk; int64_t inputs,CCchange=0; struct CCcontract_info *cp,C; std::string rawhex; uint32_t j; int32_t i,len; uint8_t buf[32768]; bits256 hash;
    cp = CCinit(&C,EVAL_PEGS);
    if ( txfee == 0 )
        txfee = 10000;
    Pegspk = GetUnspendable(cp,0);
    mypk = pubkey2pk(Mypubkey());
    if ( (inputs= AddPegsInputs(cp,mtx,Pegspk,nValue+txfee,60)) > 0 )
    {
        if ( inputs > nValue )
            CCchange = (inputs - nValue - txfee);
        if ( CCchange != 0 )
            mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,CCchange,Pegspk));
        mtx.vout.push_back(CTxOut(nValue,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        fprintf(stderr,"start at %u\n",(uint32_t)time(NULL));
        j = rand() & 0xfffffff;
        for (i=0; i<1000000; i++,j++)
        {
            tmpmtx = mtx;
            rawhex = FinalizeCCTx(-1LL,cp,tmpmtx,mypk,txfee,CScript() << OP_RETURN << E_MARSHAL(ss << (uint8_t)EVAL_PEGS << (uint8_t)'G' << j));
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
    } else fprintf(stderr,"cant find Pegs inputs\n");
    return("");
}

std::string PegsFund(uint64_t txfee,int64_t funds)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,Pegspk; CScript opret; struct CCcontract_info *cp,C;
    cp = CCinit(&C,EVAL_PEGS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    Pegspk = GetUnspendable(cp,0);
    if ( AddNormalinputs(mtx,mypk,funds+txfee,64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_PEGS,funds,Pegspk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,opret));
    }
    return("");
}

UniValue PegsInfo()
{
    UniValue result(UniValue::VOBJ); char numstr[64];
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey Pegspk; struct CCcontract_info *cp,C; int64_t funding;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Pegs"));
    cp = CCinit(&C,EVAL_PEGS);
    Pegspk = GetUnspendable(cp,0);
    funding = AddPegsInputs(cp,mtx,Pegspk,0,0);
    sprintf(numstr,"%.8f",(double)funding/COIN);
    result.push_back(Pair("funding",numstr));
    return(result);
}

