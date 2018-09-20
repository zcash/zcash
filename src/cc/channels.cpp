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

int64_t IsChannelsvout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey srcpub, CPubKey destpub,int32_t v)
{
    char destaddr[64],channeladdr[64];

    GetCCaddress1of2(cp,channeladdr,srcpub,destpub);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,channeladdr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

CScript EncodeChannelsOpRet(uint8_t funcid,uint256 opentxid,CPubKey srcpub,CPubKey destpub,int32_t numpayments,int64_t payment,uint256 hashchain)
{
    CScript opret; uint8_t evalcode = EVAL_CHANNELS;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << opentxid << srcpub << destpub << numpayments << payment << hashchain);
    return(opret);
}

uint8_t DecodeChannelsOpRet(const CScript &scriptPubKey,uint256 &opentxid, CPubKey &srcpub,CPubKey &destpub,int32_t &numpayments,int64_t &payment,uint256 &hashchain)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f,funcid;
    GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_CHANNELS )
        {
            if ( E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> opentxid; ss >> srcpub; ss >> destpub; ss >> numpayments; ss >> payment; ss >> hashchain) != 0 )
            {
                return(f);
            }
        } else fprintf(stderr,"script[0] %02x != EVAL_CHANNELS\n",script[0]);
    } else fprintf(stderr,"not enough opret.[%d]\n",(int32_t)vopret.size());
    return(0);
}

bool ChannelsExactAmounts(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx,int32_t minage,uint64_t txfee)
{
    static uint256 zerohash;
    uint256 txid,param3;
    CPubKey srcpub,destpub;
    int32_t param1; int64_t param2; uint8_t funcid;
    CTransaction vinTx; uint256 hashBlock; int32_t i,numvins,numvouts; int64_t inputs=0,outputs=0,assetoshis;
    numvins = tx.vin.size();
    numvouts = tx.vout.size();

    if ((numvouts=tx.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, param1, param2, param3)) !=0)
    {
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
                    if ( (assetoshis= IsChannelsvout(cp,vinTx,srcpub,destpub,tx.vin[i].prevout.n)) != 0 )
                        inputs += assetoshis;
                }
            }
        }
    }
    else
    {
        return eval->Invalid("invalid op_return data");
    }
    for (i=0; i<numvouts; i++)
    {
        //fprintf(stderr,"i.%d of numvouts.%d\n",i,numvouts);
        if ( (assetoshis= IsChannelsvout(cp,tx,srcpub,destpub,i)) != 0 )
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
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numpayments,param1; bool retval;
    uint256 txid,hashblock,param3,opentxid,tmp_txid,entropy,hentropy,gensecret,genhashchain,hashchain;
    uint8_t funcid,hash[32],hashdest[32];
    int64_t param2,payment;
    CPubKey srcpub, destpub;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    CTransaction channelOpenTx;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    fprintf(stderr,"validateCC\n");
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
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

            if ( (funcid = DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, opentxid, srcpub, destpub, param1, param2, param3)) != 0)
            {
                switch ( funcid )
                {
                    case 'O':
                        //vin.0: normal input
                        //vout.0: CC vout for channel funding on CC1of2 pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'O' zerotxid senderspubkey receiverspubkey totalnumberofpayments paymentamount hashchain
                        return eval->Invalid("unexpected ChannelsValidate for channelsopen");
                        break;
                    case 'P':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vout.0: CC vout change to CC1of2 pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.3: normal output of payment amount to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'P' opentxid senderspubkey receiverspubkey depth numpayments secret
                        if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelPayment");
                        else if ( IsCCInput(tx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for channelPayment");
                        else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.0 is CC for channelPayment");
                        else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.1 is CC for channelPayment (marker to srcPub)");
                        else if ( tx.vout[2].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.1 is CC for channelPayment (marker to dstPub)");
                        else if ( tx.vout[3].scriptPubKey.IsPayToCryptoCondition() != 0 )
                            return eval->Invalid("vout.1 is normal for channelPayment");
                        else if ( tx.vout[3].scriptPubKey!=CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG)
                            return eval->Invalid("payment funds do not go to receiver -");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments");
                        else
                        {
                            if (myGetTransaction(opentxid,channelOpenTx,hashblock) != 0)
                            {
                                if ((numvouts=channelOpenTx.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, numpayments, payment, hashchain)) != 0 && funcid!='O')
                                    return eval->Invalid("invalid channelopen OP_RETURN data");
                                hentropy = DiceHashEntropy(entropy, channelOpenTx.vin[0].prevout.hash);
                                endiancpy(hash, (uint8_t * ) & param3, 32);
                                for (i = 0; i < numpayments-param1; i++)
                                {
                                    vcalc_sha256(0, hashdest, hash, 32);
                                    memcpy(hash, hashdest, 32);
                                }
                                endiancpy((uint8_t*)&genhashchain,hashdest,32);

                                if (hashchain!=genhashchain)
                                    return eval->Invalid("invalid secret for payment, does not reach final hashchain");
                                else if (tx.vout[0].nValue != param1*payment)
                                    return eval->Invalid("vout amount does not match numberofpayments*payment");
                            }
                        }
                        break;
                    case 'C':
                        //vin.0: normal input for 2*txfee (normal fee and marker)
                        //vout.0: CC vout marker to senders pubKey
                        //vout.1: opreturn - 'C' opentxid senderspubkey receiverspubkey 0 0 0
                        return eval->Invalid("unexpected ChannelsValidate for channelsclose");
                    case 'R':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vout.0: normal output of CC input to senders pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: opreturn - 'R' opentxid senderspubkey receiverspubkey 0 0 closetxid
                        return eval->Invalid("unexpected ChannelsValidate for channelsrefund");
                        break;
                }
            }
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

int64_t AddChannelsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx, CTransaction openTx, uint256 &prevtxid)
{
    char coinaddr[64]; int64_t param2,totalinputs = 0,numvouts; uint256 txid=zeroid,tmp_txid,hashBlock,param3; CTransaction tx; int32_t funcid,param1;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CPubKey srcpub,destpub;
    uint8_t myprivkey[32];

    if ((numvouts=openTx.vout.size()) > 0 && DecodeChannelsOpRet(openTx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3)=='O')
    {
        GetCCaddress1of2(cp,coinaddr,srcpub,destpub);
        SetCCunspents(unspentOutputs,coinaddr);
        Myprivkey(myprivkey);
        CCaddr2set(cp,EVAL_CHANNELS,srcpub,myprivkey,coinaddr);
        CCaddr3set(cp,EVAL_CHANNELS,destpub,myprivkey,coinaddr);
    }
    else
    {
        fprintf(stderr,"invalid open tx id\n");
        return 0;
    }

    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        if ( (int32_t)it->first.index==0 && GetTransaction(it->first.txhash,tx,hashBlock,false) != 0 && (numvouts=tx.vout.size()) > 0)
        {
            if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3)!=0 && (totalinputs=IsChannelsvout(cp,tx,srcpub,destpub,0))>0)
            {
                txid = it->first.txhash;
                break;
            }
        }
    }
    if (myIsutxo_spentinmempool(txid,0) != 0)
    {
        fprintf(stderr,"spent in mempool\n");
        txid=zeroid;
        int32_t mindepth=CHANNELS_MAXPAYMENTS;
        BOOST_FOREACH(const CTxMemPoolEntry &e, mempool.mapTx)
        {
            const CTransaction &txmempool = e.GetTx();
            const uint256 &hash = tx.GetHash();
            if ((numvouts=txmempool.vout.size()) > 0 &&
              (funcid=DecodeChannelsOpRet(txmempool.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3)) != 0 &&
              funcid=='P' && param1 < mindepth)
            {
                txid=hash;
                totalinputs=txmempool.vout[0].nValue;
                mindepth=param1;

            }
        }
    }
    else fprintf(stderr,"not spent in mempool\n");
    if (txid != zeroid)
    {
        prevtxid=txid;
        mtx.vin.push_back(CTxIn(txid,0,CScript()));
        return totalinputs;
    }
    else return 0;
}

std::string ChannelOpen(uint64_t txfee,CPubKey destpub,int32_t numpayments,int64_t payment)
{
    CMutableTransaction mtx; uint8_t hash[32],hashdest[32]; uint64_t funds; int32_t i; uint256 hashchain,entropy,hentropy; CPubKey mypk,channelspk; struct CCcontract_info *cp,C;
    if ( numpayments <= 0 || payment <= 0 || numpayments > CHANNELS_MAXPAYMENTS )
    {
        CCerror = strprintf("invalid ChannelOpen param numpayments.%d max.%d payment.%lld\n",numpayments,CHANNELS_MAXPAYMENTS,(long long)payment);
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
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('O',zeroid,mypk,destpub,numpayments,payment,hashchain)));
    }
    return("");
}

std::string ChannelPayment(uint64_t txfee,uint256 opentxid,int64_t amount)
{
    CMutableTransaction mtx; CPubKey mypk,srcpub,destpub; uint256 txid,hashchain,secret,hashblock,entropy,hentropy,prevtxid;
    struct CCcontract_info *cp,C; int32_t i,funcid,prevdepth,numvouts,numpayments;
    int64_t payment,change,funds;
    uint8_t hash[32],hashdest[32];
    CTransaction channelOpenTx,prevTx;

    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());

    if (GetTransaction(opentxid,channelOpenTx,hashblock,false) == 0)
    {
        fprintf(stderr, "invalid channel open txid\n");
        return ("");
    }

    if (AddNormalinputs(mtx,mypk,3*txfee,1) > 0)
    {
        if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid)) !=0 && (change=funds-amount)>=0)
        {
            if (GetTransaction(prevtxid,prevTx,hashblock,false) != 0)
            {
                if ((numvouts=prevTx.vout.size()) > 0 && ((funcid = DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, prevdepth, payment, hashchain)) != 0) && (funcid == 'P' || funcid=='O'))
                {
                    if (mypk != srcpub && mypk != destpub)
                    {
                        fprintf(stderr,"this is not our channel\n");
                        return("");
                    }
                    else if (amount % payment != 0)
                    {
                        fprintf(stderr,"invalid amount, not a magnitude of payment size\n");
                        return ("");
                    }
                }
                else
                {
                    fprintf(stderr,"invalid previous tx\n");
                    return("");
                }

                numpayments=amount/payment;

                hentropy = DiceHashEntropy(entropy, channelOpenTx.vin[0].prevout.hash);
                endiancpy(hash, (uint8_t * ) & hentropy, 32);
                for (i = 0; i < prevdepth-numpayments; i++)
                {
                    vcalc_sha256(0, hashdest, hash, 32);
                    memcpy(hash, hashdest, 32);
                }
                endiancpy((uint8_t * ) & secret, hashdest, 32);
            }
            else
            {
                fprintf(stderr, "cannot find previous tx (channel open or payment)\n");
                return ("");
            }
            // verify lasttxid and origtxid match and src is me
            // also verify hashchain depth and amount, set prevdepth

            mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS, change, mypk, destpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,destpub));
            mtx.vout.push_back(CTxOut(amount, CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG));
            return (FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeChannelsOpRet('P', opentxid, mypk, destpub, prevdepth - numpayments, payment, secret)));
        }
        else
        {
            fprintf(stderr,"error adding CC inputs\n");
            return("");
        }
    }
    fprintf(stderr,"error adding normal inputs\n");
    return("");
}

std::string ChannelClose(uint64_t txfee,uint256 opentxid)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C;
    // verify this is one of our outbound channels
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('S',opentxid,mypk,mypk,0,0,zeroid)));
    }
    return("");
}

std::string ChannelRefund(uint64_t txfee,uint256 opentxid,uint256 closetxid)
{
    CMutableTransaction mtx; CPubKey mypk; struct CCcontract_info *cp,C; int64_t amount;
    // verify stoptxid and origtxid match and are mine
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if ( AddNormalinputs(mtx,mypk,txfee,1) > 0 )
    {
        mtx.vout.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('R',opentxid,mypk,mypk,0,0,closetxid)));
    }
    return("");
}

UniValue ChannelsInfo()
{
    UniValue result(UniValue::VOBJ); CTransaction tx; uint256 txid,hashBlock,hashchain,opentxid; struct CCcontract_info *cp,C; char myCCaddr[64]; int32_t vout,numvouts,numpayments; int64_t nValue,payment; CPubKey srcpub,destpub,mypk;
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
            if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,numpayments,payment,hashchain) == 'O')
            {
                char str[67],str2[67];
                fprintf(stderr,"%s  %s -> %s %lldsat num.%d of %.8lldsat\n","ChannelOpen",pubkey33_str(str,(uint8_t *)&srcpub),pubkey33_str(str2,(uint8_t *)&destpub),(long long)tx.vout[0].nValue,numpayments,(long long)payment);
            }
            else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,numpayments,payment,hashchain) == 'P')
            {
                char str[67],str2[67];
                fprintf(stderr,"%s (%s) %s -> %s %lldsat num.%d of %.8lldsat\n","ChannelPayment",opentxid.ToString().c_str(),pubkey33_str(str,(uint8_t *)&srcpub),pubkey33_str(str2,(uint8_t *)&destpub),(long long)tx.vout[0].nValue,numpayments,(long long)payment);
            }
        }
    }
    return(result);
}

