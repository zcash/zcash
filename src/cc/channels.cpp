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

#include "CCchannels.h"

/*
 The idea here is to allow instant (mempool) payments that are secured by dPoW. In order to simplify things, channels CC will require creating reserves for each payee locked in the destination user's CC address. This will look like the payment is already made, but it is locked until further released. The dPoW protection comes from the cancel channel having a delayed effect until the next notarization. This way, if a payment release is made and the chain reorged, the same payment release will still be valid when it is re-broadcast into the mempool.
 
 In order to achieve this effect, the payment release needs to be in a special form where its input cannot be spent only by the sender.

 Given sender's payment to dest CC address, only the destination is able to spend, so we need to constrain that spending with a release mechanism. One idea is a 2of2 multisig, but that has the issue of needing confirmation and since a sender utxo is involved, subject to doublespend and we lose the speed. Another idea is release on secrets! since once revealed, the secret remains valid, this method is immune from double spend. Also, there is no worry about an MITM attack as the funds are only spendable by the destination pubkey and only with the secret. The secrets can be sent via any means, including OP_RETURN of normal transaction in the mempool.
 
 Now the only remaining issue for sending is how to allocate funds to the secrets. This needs to be sent as hashes of secrets when the channel is created. A bruteforce method would be one secret per COIN, but for large amount channels this is cumbersome. A more practical approach is to have a set of secrets for each order of magnitude:
 
 123.45 channel funds -> 1x secret100, 2x secret10, 3x secret1, 4x secret.1, 5x secret.01
 15 secrets achieves the 123.45 channel funding.
 
 In order to avoid networking issues, the convention can be to send tx to normal address of destination with just an OP_RETURN, for the cost of txfee. For micropayments, a separate method of secret release needs to be established, but that is beyond the scope of this CC.
 
 There is now the dPoW security that needs to be ensured. In order to close the channel, a tx needs to be confirmed that cancels the channel. As soon as this tx is seen, the destination will know that the channel will be closing soon, if the node is online. If not, the payments cant be credited anyway, so it seems no harm. Even after the channel is closed, it is possible for secrets to be releasing funds, but depending on when the notarization happens, it could invalidate the spends, so it is safest that as soon as the channel cancel tx is confirmed to invalidate any further payments released.
 
 Given a channelclose and notarization confirmation (or enough blocks), the remaining funds needs to be able to come back to the sender. this means the funds need to be in a 1of2 CC multisig to allow either party to spend. Cheating is prevented by the additional rules of spending, ie. denomination secrets, or channelclose.
 
 For efficiency we want to allow batch spend with multiple secrets to claim a single total
 
 Second iteration:
 As implementing it, some efficieny gains to be made with a slightly different approach.
 Instead of separate secrets for each amount, a hashchain will be used, each releasing the same amount
 
 To spend, the prior value in the hash chain is published, or can publish N deep. validation takes N hashes.
 
 Also, in order to be able to track open channels, a tag is needed to be sent and better to send to a normal CC address for a pubkey to isolate the transactions for channel opens.
 
Possible third iteration:
 Let us try to setup a single "hot wallet" address to have all the channel funds and use it for payments to any destination. If there are no problems with reorgs and double spends, this would allow everyone to be "connected" to everyone else via the single special address.
 
 So funds -> user's CC address along with hashchain, but likely best to have several utxo to span order of magnitudes.
 
 a micropayment would then spend a utxo and attach a shared secret encoded unhashed link from the hashchain. That makes the receiver the only one that can decode the actual hashchain's prior value.
 
 however, since this spend is only spendable by the sender, it is subject to a double spend attack. It seems it is a dead end. Alternative is to use the global CC address, but that commingles all funds from all users and any accounting error puts all funds at risk.
 
 So, back to the second iteration, which is the only one so far that is immune from doublespend attack as the funds are already in the destination's CC address. One complication is that due to CC sorting of pubkeys, the address for sending and receiving is the same, so the destination pubkey needs to be attached to each opreturn.
 
 Now when the prior hashchain value is sent via payment, it allows the receiver to spend the utxo, so the only protection needed is to prevent channel close from invalidating already made payments.
 
 In order to allow multiple payments included in a single transaction, presentation of the N prior hashchain value can be used to get N payments and all the spends create a spending chain in sequential order of the hashchain.
 
*/

// start of consensus code

int64_t IsChannelsvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

bool ChannelsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
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
                    return eval->Invalid("cant Channels from mempool");
                if ( (assetoshis= IsChannelsvout(cp,vinTx,tx.vin[i].prevout.n)) != 0 )
                    inputs += assetoshis;
            }
        }
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsChannelsvout(cp,tx,i)) != 0 )
            outputs += assetoshis;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool ChannelsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx)
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
        if ( ChannelsExactAmounts(cp,eval,tx,1,10000) == false )
        {
            fprintf(stderr,"Channelsget invalid amount\n");
            return false;
        }
        else
        {
            txid = tx.GetHash();
            memcpy(hash,&txid,sizeof(hash));
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Channelsget validated\n");
            else fprintf(stderr,"Channelsget invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

CScript EncodeChannelsOpRet(uint8_t funcid,CPubKey srcpub,CPubKey destpub,int32_t numpayments,int64_t payment,uint256 hashchain)
{
    CScript opret; uint8_t evalcode = EVAL_CHANNELS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << srcpub << destpub << numpayments << payment << hashchain);
    return(opret);
}

uint8_t DecodeChannelsOpRet(uint256 txid,const CScript &scriptPubKey,CPubKey &srcpub,CPubKey &destpub,int32_t &numpayments,int64_t &payment,uint256 &hashchain)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_CHANNELS )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> srcpub; ss >> destpub; ss >> numpayments; ss >> payment; ss >> hashchain) != 0 )
            {
                return(f);
            }
        } else fprintf(stderr,"script[0] %02x != EVAL_CHANNELS\n",script[0]);
    } else fprintf(stderr,"not enough opret.[%d]\n",(int32_t)vopret.size());
    return(0);
}

int64_t AddChannelsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx,CPubKey pk,int64_t total,int32_t maxinputs)
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
            if ( (nValue= IsChannelsvout(cp,vintx,vout)) > 0 && myIsutxo_spentinmempool(txid,vout) == 0 )
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

std::string ChannelOpen(uint64_t txfee,CPubKey destpub,int32_t numpayments,int64_t payment)
{
    CMutableTransaction mtx; uint8_t hash[32],hashdest[32]; uint64_t funds; int32_t i; uint256 hashchain,entropy,hentropy; CPubKey mypk; struct CCcontract_info *cp,C;
    if ( numpayments <= 0 || payment <= 0 || numpayments > CHANNELS_MAXPAYMENTS )
    {
        CCerror = strprintf("invalid ChannelsFund param numpayments.%d max.%d payment.%lld\n",numpayments,CHANNELS_MAXPAYMENTS,(long long)payment);
        fprintf(stderr,"%s\n",CCerror.c_str());
        return("");
    }
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    funds = numpayments * payment;
    if ( AddNormalinputs(mtx,mypk,funds+3*txfee,64) > 0 )
    {
        hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash);
        endiancpy(hash,(uint8_t *)&hentropy,32);
        for (i=0; i<numpayments; i++)
        {
            vcalc_sha256(0,hashdest,hash,32);
            memcpy(hash,hashdest,32);
        }
        endiancpy((uint8_t *)&hashchain,hashdest,32);
        mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS,funds,mypk,destpub));
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,destpub));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('O',mypk,destpub,numpayments,payment,hashchain)));
    }
    return("");
}

std::string ChannelStop(uint64_t txfee,CPubKey destpub,uint256 origtxid)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C;
    // verify this is one of our outbound channels
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('S',mypk,destpub,0,0,zeroid)));
    }
    return("");
}

std::string ChannelPayment(uint64_t txfee,uint256 prevtxid,uint256 origtxid,int32_t n,int64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,destpub; uint256 secret; struct CCcontract_info *cp,C; int32_t prevdepth;
    // verify lasttxid and origtxid match and src is me
    // also verify hashchain depth and amount, set prevdepth
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        // add locked funds inputs
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('P',mypk,destpub,prevdepth-n,amount,secret)));
    }
    return("");
}

std::string ChannelCollect(uint64_t txfee,uint256 paytxid,uint256 origtxid,int32_t n,int64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,senderpub; struct CCcontract_info *cp,C; int32_t prevdepth;
    // verify paytxid and origtxid match and dest is me
    // also verify hashchain depth and amount
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        // add locked funds inputs
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
        mtx.vout.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('C',senderpub,mypk,prevdepth-n,amount,paytxid)));
    }
    return("");
}

std::string ChannelRefund(uint64_t txfee,uint256 stoptxid,uint256 origtxid)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C; int64_t amount;
    // verify stoptxid and origtxid match and are mine
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,2*txfee,1) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
        mtx.vout.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('R',mypk,mypk,0,0,stoptxid)));
    }
    return("");
}

UniValue ChannelsInfo()
{
    UniValue result(UniValue::VOBJ); CTransaction tx; uint256 txid,hashBlock,hashchain; struct CCcontract_info *cp,C; uint8_t funcid; char myCCaddr[64]; int32_t vout,numvouts,numpayments; int64_t nValue,payment; CPubKey srcpub,destpub,mypk;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Channels"));
    cp = CCinit(&C,EVAL_CHANNELS);
    mypk = pubkey2pk(Mypubkey());
    GetCCaddress(cp,myCCaddr,mypk);
    SetCCtxids(txids,myCCaddr);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        //int height = it->first.blockHeight;
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second;
        if ( (vout == 1 || vout == 2) && nValue == 10000 && GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 )
        {
            if ( DecodeChannelsOpRet(txid,tx.vout[numvouts-1].scriptPubKey,srcpub,destpub,numpayments,payment,hashchain) == 'O' || funcid == 'P' )
            {
                char str[67],str2[67];
                fprintf(stderr,"%s func.%c %s -> %s %.8f num.%d of %.8f\n",mypk == srcpub ? "send" : "recv",funcid,pubkey33_str(str,(uint8_t *)&srcpub),pubkey33_str(str2,(uint8_t *)&destpub),(double)tx.vout[0].nValue/COIN,numpayments,(double)payment/COIN);
            }
        }
    }
    return(result);
}

