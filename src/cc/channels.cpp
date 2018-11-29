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
    char destaddr[65],channeladdr[65];

    GetCCaddress1of2(cp,channeladdr,srcpub,destpub);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,channeladdr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

int64_t IsChannelsMarkervout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey srcpub,int32_t v)
{
    char destaddr[65],ccaddr[65];

    GetCCaddress(cp,ccaddr,srcpub);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,ccaddr) == 0 )
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
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
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

    if ((numvouts=tx.vout.size()) > 0 && DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, param1, param2, param3)!=0)
    {
        for (i=0; i<numvins; i++)
        {
            if ( eval->GetTxUnconfirmed(tx.vin[i].prevout.hash,vinTx,hashBlock) == 0 )
                return eval->Invalid("cant find vinTx");
            else
            {
                inputs += vinTx.vout[tx.vin[i].prevout.n].nValue;
            }
        }
    }
    else
    {
        return eval->Invalid("invalid op_return data");
    }
    for (i=0; i<numvouts; i++)
    {
        outputs += tx.vout[i].nValue;
    }
    if ( inputs != outputs+txfee )
    {
        fprintf(stderr,"inputs %llu vs outputs %llu\n",(long long)inputs,(long long)outputs);
        return eval->Invalid("mismatched inputs != outputs + txfee");
    }
    else return(true);
}

bool ChannelsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numpayments,p1,param1; bool retval;
    uint256 txid,hashblock,p3,param3,opentxid,tmp_txid,genhashchain,hashchain;
    uint8_t funcid,hash[32],hashdest[32];
    int64_t p2,param2,payment;
    CPubKey srcpub, destpub;
    CTransaction channelOpenTx,channelCloseTx,prevTx;

    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        if (ChannelsExactAmounts(cp,eval,tx,1,10000) == false )
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
                        return eval->Invalid("unexpected ChannelsValidate for channelsopen!");
                    case 'P':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout change to CC1of2 pubkey
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.3: normal output of payment amount to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'P' opentxid senderspubkey receiverspubkey depth numpayments secret
                        if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelOpen is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelPayment!");
                        else if ( IsCCInput(tx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for channelPayment!");
                        else if ( IsCCInput(tx.vin[2].scriptSig) == 0 )
                            return eval->Invalid("vin.2  is CC for channelPayment!");
                        else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.0 is CC for channelPayment!");
                        else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.1 is CC for channelPayment (marker to srcPub)!");
                        else if ( tx.vout[2].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.2 is CC for channelPayment (marker to dstPub)!");
                        else if ( tx.vout[3].scriptPubKey.IsPayToCryptoCondition() != 0 )
                            return eval->Invalid("vout.3 is normal for channelPayment!");
                        else if ( tx.vout[3].scriptPubKey!=CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG)
                            return eval->Invalid("payment funds do not go to receiver!");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments!");
                        else
                        {
                            if (myGetTransaction(opentxid,channelOpenTx,hashblock) != 0)
                            {
                                if ((numvouts=channelOpenTx.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, numpayments, payment, hashchain)) != 0 && funcid!='O')
                                    return eval->Invalid("invalid channelopen OP_RETURN data!");
                                endiancpy(hash, (uint8_t * ) & param3, 32);
                                for (i = 0; i < numpayments-param1; i++)
                                {
                                    vcalc_sha256(0, hashdest, hash, 32);
                                    memcpy(hash, hashdest, 32);
                                }
                                endiancpy((uint8_t*)&genhashchain,hashdest,32);
                                if (hashchain!=genhashchain)
                                    return eval->Invalid("invalid secret for payment, does not reach final hashchain!");
                                else if (tx.vout[3].nValue != param2*payment)
                                    return eval->Invalid("vout amount does not match number_of_payments*payment!");
                            }
                            if (myGetTransaction(tx.vin[1].prevout.hash,prevTx,hashblock) != 0)
                            {
                                if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, p1, p2, p3) == 0)
                                    return eval->Invalid("invalid previous tx OP_RETURN data!");
                                else if (tx.vout[1].scriptPubKey != prevTx.vout[1].scriptPubKey)
                                    return eval->Invalid("invalid destination for sender marker!");
                                else if (tx.vout[2].scriptPubKey != prevTx.vout[2].scriptPubKey)
                                    return eval->Invalid("invalid destination for receiver marker!");
                                else if (param1+param2!=p1)
                                    return eval->Invalid("invalid payment depth!");
                                else if (tx.vout[3].nValue > prevTx.vout[0].nValue)
                                    return eval->Invalid("not enough funds in channel for that amount!");
                            }
                        }
                        break;
                    case 'C':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout for channel funding
                        //vout.1: CC vout marker to senders pubKey
                        //vout.2: CC vout marker to receiver pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'C' opentxid senderspubkey receiverspubkey 0 0 0
                        if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelOpen is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelClose!");
                        else if ( IsCCInput(tx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for channelClose!");
                        else if ( IsCCInput(tx.vin[2].scriptSig) == 0 )
                            return eval->Invalid("vin.2 is CC for channelClose!");
                        else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.0 is CC for channelClose!");
                        else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.1 is CC for channelClose (marker to srcPub)!");
                        else if ( tx.vout[2].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.2 is CC for channelClose (marker to dstPub)!");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments!");
                        else if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0)
                            return eval->Invalid("invalid open txid!");
                        else if ((numvouts=channelOpenTx.vout.size()) > 0 && DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, numpayments, payment, hashchain) != 'O')
                            return eval->Invalid("invalid channelopen OP_RETURN data!");
                        else if (tx.vout[0].nValue != param1*payment)
                            return eval->Invalid("vout amount does not match number_of_payments*payment!");
                        else if (myGetTransaction(tx.vin[1].prevout.hash,prevTx,hashblock) != 0)
                        {
                            if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, p1, p2, p3) == 0)
                                return eval->Invalid("invalid previous tx OP_RETURN data!");
                            else if (tx.vout[1].scriptPubKey != prevTx.vout[1].scriptPubKey)
                                return eval->Invalid("invalid destination for sender marker!");
                            else if (tx.vout[2].scriptPubKey != prevTx.vout[2].scriptPubKey)
                                return eval->Invalid("invalid destination for receiver marker!");
                            else if (tx.vout[0].nValue != prevTx.vout[0].nValue)
                                return eval->Invalid("invalid CC amount, amount must match funds in channel");
                        }
                        break;
                    case 'R':
                        //vin.0: normal input
                        //vin.1: CC input from channel funding
                        //vin.2: CC input from src marker
                        //vout.0: CC vout marker to senders pubKey
                        //vout.1: CC vout marker to receiver pubKey
                        //vout.2: normal output of CC input to senders pubkey
                        //vout.n-2: normal change
                        //vout.n-1: opreturn - 'R' opentxid senderspubkey receiverspubkey numpayments payment closetxid
                        if (komodo_txnotarizedconfirmed(opentxid) == 0)
                            return eval->Invalid("channelOpen is not yet confirmed(notarised)!");
                        else if (komodo_txnotarizedconfirmed(param3) == 0)
                            return eval->Invalid("channelClose is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for channelRefund!");
                        else if ( IsCCInput(tx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for channelRefund!");
                        else if ( IsCCInput(tx.vin[2].scriptSig) == 0 )
                            return eval->Invalid("vin.2 is CC for channelRefund!");
                        else if ( tx.vout[0].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.0 is CC for channelRefund (marker to srcPub)!");
                        else if ( tx.vout[1].scriptPubKey.IsPayToCryptoCondition() == 0 )
                            return eval->Invalid("vout.1 is CC for channelRefund (marker to dstPub)!");
                        else if ( tx.vout[2].scriptPubKey.IsPayToCryptoCondition() != 0 )
                            return eval->Invalid("vout.2 is normal for channelRefund!");
                        else if ( tx.vout[2].scriptPubKey!=CScript() << ParseHex(HexStr(srcpub)) << OP_CHECKSIG)
                            return eval->Invalid("payment funds do not go to sender!");
                        else if ( param1 > CHANNELS_MAXPAYMENTS)
                            return eval->Invalid("too many payment increments!");
                        else if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0)
                            return eval->Invalid("invalid open txid!");
                        else if ((numvouts=channelOpenTx.vout.size()) > 0 && DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, numpayments, payment, hashchain) != 'O')
                            return eval->Invalid("invalid channelopen OP_RETURN data!");
                        else if (myGetTransaction(param3,channelCloseTx,hashblock) == 0)
                            return eval->Invalid("invalid close txid!");
                        else if ((numvouts=channelCloseTx.vout.size()) > 0 && DecodeChannelsOpRet(channelCloseTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, param1, param2, param3) != 'C')
                            return eval->Invalid("invalid channelclose OP_RETURN data!");
                        else if (tmp_txid!=opentxid)
                            return eval->Invalid("invalid close tx, opentxid do not match on close and refund!");
                        else if (tx.vout[2].nValue != param1*payment)
                            return eval->Invalid("vout amount does not match number_of_payments*payment!");
                        else if (myGetTransaction(tx.vin[1].prevout.hash,prevTx,hashblock) != 0)
                        {
                            if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tmp_txid, srcpub, destpub, p1, p2, p3) == 0)
                                return eval->Invalid("invalid previous tx OP_RETURN data!");
                            else if (tx.vout[0].scriptPubKey != prevTx.vout[1].scriptPubKey)
                                return eval->Invalid("invalid destination for sender marker!");
                            else if (tx.vout[1].scriptPubKey != prevTx.vout[2].scriptPubKey)
                                return eval->Invalid("invalid destination for receiver marker!");
                            else if (tx.vout[2].nValue != prevTx.vout[0].nValue)
                                return eval->Invalid("invalid amount, refund amount and funds in channel must match!");
                        }
                        break;
                    default:
                        fprintf(stderr,"illegal channels funcid.(%c)\n",funcid);
                        return eval->Invalid("unexpected channels funcid");
                        break;
                }
            } else return eval->Invalid("unexpected channels missing funcid");
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                fprintf(stderr,"Channel tx validated\n");
            else fprintf(stderr,"Channel tx invalid\n");
            return(retval);
        }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddChannelsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx, CTransaction openTx, uint256 &prevtxid)
{
    char coinaddr[65]; int64_t param2,totalinputs = 0,numvouts; uint256 txid=zeroid,tmp_txid,hashBlock,param3; CTransaction tx; int32_t param1;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    CPubKey srcpub,destpub;
    uint8_t myprivkey[32];

    if ((numvouts=openTx.vout.size()) > 0 && DecodeChannelsOpRet(openTx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3)=='O')
    {
        GetCCaddress1of2(cp,coinaddr,srcpub,destpub);
        SetCCunspents(unspentOutputs,coinaddr);
    }
    else
    {
        fprintf(stderr,"invalid channel open txid\n");
        return 0;
    }

    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        if ( (int32_t)it->first.index==0 && GetTransaction(it->first.txhash,tx,hashBlock,false) != 0 && (numvouts=tx.vout.size()) > 0)
        {
            if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3)!=0 &&
              (tmp_txid==openTx.GetHash() || tx.GetHash()==openTx.GetHash()) &&
              (totalinputs=IsChannelsvout(cp,tx,srcpub,destpub,0)+IsChannelsMarkervout(cp,tx,srcpub,1))>0)
            {
                txid = it->first.txhash;
                break;
            }
        }
    }
    if (txid!=zeroid && myIsutxo_spentinmempool(txid,0) != 0)
    {
        txid=zeroid;
        int32_t mindepth=CHANNELS_MAXPAYMENTS;
        BOOST_FOREACH(const CTxMemPoolEntry &e, mempool.mapTx)
        {
            const CTransaction &txmempool = e.GetTx();
            const uint256 &hash = txmempool.GetHash();

            if ((numvouts=txmempool.vout.size()) > 0 && DecodeChannelsOpRet(txmempool.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3) != 0 &&
              tmp_txid==openTx.GetHash() && param1 < mindepth)
            {
                txid=hash;
                totalinputs=txmempool.vout[0].nValue+txmempool.vout[1].nValue;
                mindepth=param1;
            }
        }
    }
    if (txid != zeroid)
    {
        prevtxid=txid;
        mtx.vin.push_back(CTxIn(txid,0,CScript()));
        mtx.vin.push_back(CTxIn(txid,1,CScript()));
        Myprivkey(myprivkey);
        CCaddr2set(cp,EVAL_CHANNELS,srcpub,myprivkey,coinaddr);
        CCaddr3set(cp,EVAL_CHANNELS,destpub,myprivkey,coinaddr);
        return totalinputs;
    }
    else return 0;
}

std::string ChannelOpen(uint64_t txfee,CPubKey destpub,int32_t numpayments,int64_t payment)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    uint8_t hash[32],hashdest[32]; uint64_t funds; int32_t i; uint256 hashchain,entropy,hentropy;
    CPubKey mypk; struct CCcontract_info *cp,C;
    
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
        hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
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

std::string ChannelPayment(uint64_t txfee,uint256 opentxid,int64_t amount, uint256 secret)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,srcpub,destpub; uint256 txid,hashchain,gensecret,hashblock,entropy,hentropy,prevtxid,param3;
    struct CCcontract_info *cp,C; int32_t i,funcid,prevdepth,numvouts,numpayments,totalnumpayments;
    int64_t payment,change,funds,param2;
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
    if (AddNormalinputs(mtx,mypk,2*txfee,3) > 0)
    {
        if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid)) !=0 && (change=funds-amount-txfee)>=0)
        {
            if ((numvouts=channelOpenTx.vout.size()) > 0 && DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, totalnumpayments, payment, hashchain)=='O')
            {
                if (mypk != srcpub && mypk != destpub)
                {
                    fprintf(stderr,"this is not our channel\n");
                    return("");
                }
                else if (amount % payment != 0 || amount<payment)
                {
                    fprintf(stderr,"invalid amount, not a magnitude of payment size\n");
                    return ("");
                }
                numpayments=amount/payment;
                if (GetTransaction(prevtxid,prevTx,hashblock,false) != 0 && (numvouts=prevTx.vout.size()) > 0 &&
                  ((funcid = DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, prevdepth, param2, param3)) != 0) &&
                  (funcid == 'P' || funcid=='O'))
                {
                    if (numpayments > prevdepth)
                    {
                        fprintf(stderr,"not enough funds in channel for that amount\n");
                        return ("");
                    } else if (numpayments == 0)
                    {
                        fprintf(stderr,"invalid amount\n");
                        return ("");
                    }
                    if (secret!=zeroid)
                    {
                        endiancpy(hash, (uint8_t * ) & secret, 32);
                        for (i = 0; i < totalnumpayments-(prevdepth-numpayments); i++)
                        {
                            vcalc_sha256(0, hashdest, hash, 32);
                            memcpy(hash, hashdest, 32);
                        }
                        endiancpy((uint8_t * ) & gensecret, hashdest, 32);
                        if (gensecret!=hashchain)
                        {
                            fprintf(stderr,"invalid secret supplied\n");
                            return("");
                        }
                    }
                    else
                    {
                        hentropy = DiceHashEntropy(entropy,channelOpenTx.vin[0].prevout.hash,channelOpenTx.vin[0].prevout.n,1);
                        if (prevdepth-numpayments)
                        {
                            endiancpy(hash, (uint8_t * ) & hentropy, 32);
                            for (i = 0; i < prevdepth-numpayments; i++)
                            {
                                vcalc_sha256(0, hashdest, hash, 32);
                                memcpy(hash, hashdest, 32);
                            }
                            endiancpy((uint8_t * ) & secret, hashdest, 32);
                        }
                        else endiancpy((uint8_t * ) & secret, (uint8_t * ) & hentropy, 32);
                    }
                }
                else
                {
                    fprintf(stderr,"invalid previous tx\n");
                    return("");
                }
            }
            else
            {
                fprintf(stderr, "invalid channel open tx\n");
                return ("");
            }
            mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS, change, mypk, destpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,destpub));
            mtx.vout.push_back(CTxOut(amount, CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG));
            return (FinalizeCCTx(0, cp, mtx, mypk, txfee, EncodeChannelsOpRet('P', opentxid, mypk, destpub, prevdepth-numpayments, numpayments, secret)));
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
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,srcpub,destpub; struct CCcontract_info *cp,C;
    CTransaction channelOpenTx;
    uint256 hashblock,tmp_txid,prevtxid,hashchain;
    int32_t numvouts,numpayments;
    int64_t payment,funds;

    // verify this is one of our outbound channels
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if (GetTransaction(opentxid,channelOpenTx,hashblock,false) == 0)
    {
        fprintf(stderr, "invalid channel open txid\n");
        return ("");
    }
    if ((numvouts=channelOpenTx.vout.size()) < 1 || DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,numpayments,payment,hashchain)!='O')
    {
        fprintf(stderr, "invalid channel open tx\n");
        return ("");
    }
    if (mypk != srcpub)
    {
        fprintf(stderr,"cannot close, you are not channel owner\n");
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,2*txfee,3) > 0 )
    {
        if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid)) !=0 && funds-txfee>0)
        {
            mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS, funds-txfee, mypk, destpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,destpub));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('C',opentxid,mypk,destpub,funds/payment,payment,zeroid)));
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

std::string ChannelRefund(uint64_t txfee,uint256 opentxid,uint256 closetxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; struct CCcontract_info *cp,C; int64_t funds,payment,param2;
    int32_t i,numpayments,numvouts,param1;
    uint256 hashchain,hashblock,txid,prevtxid,param3,entropy,hentropy,secret;
    CTransaction channelOpenTx,channelCloseTx,prevTx;
    CPubKey srcpub,destpub;
    uint8_t funcid,hash[32],hashdest[32];;

    // verify stoptxid and origtxid match and are mine
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if (GetTransaction(closetxid,channelCloseTx,hashblock,false) == 0)
    {
        fprintf(stderr, "invalid channel close txid\n");
        return ("");
    }
    if ((numvouts=channelCloseTx.vout.size()) < 1 || DecodeChannelsOpRet(channelCloseTx.vout[numvouts-1].scriptPubKey,txid,srcpub,destpub,param1,param2,param3)!='C')
    {
        fprintf(stderr, "invalid channel close tx\n");
        return ("");
    }
    if (txid!=opentxid)
    {
        fprintf(stderr, "open and close txid are not from same channel\n");
        return ("");
    }
    if (GetTransaction(opentxid,channelOpenTx,hashblock,false) == 0)
    {
        fprintf(stderr, "invalid channel open txid\n");
        return ("");
    }
    if ((numvouts=channelOpenTx.vout.size()) < 1 || DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey,txid,srcpub,destpub,numpayments,payment,hashchain)!='O')
    {
        fprintf(stderr, "invalid channel open tx\n");
        return ("");
    }
    if (mypk != srcpub)
    {
        fprintf(stderr,"cannot refund, you are not the channel owenr\n");
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,2*txfee,3) > 0 )
    {
        if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid)) !=0 && funds-txfee>0)
        {
            if ((GetTransaction(prevtxid,prevTx,hashblock,false) != 0) && (numvouts=prevTx.vout.size()) > 0 &&
                DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, txid, srcpub, destpub, param1, param2, param3) != 0)
            {
                hentropy = DiceHashEntropy(entropy, channelOpenTx.vin[0].prevout.hash, channelOpenTx.vin[0].prevout.n,1);
                endiancpy(hash, (uint8_t * ) & hentropy, 32);
                for (i = 0; i < param1; i++)
                {
                    vcalc_sha256(0, hashdest, hash, 32);
                    memcpy(hash, hashdest, 32);
                }
                endiancpy((uint8_t * ) & secret, hashdest, 32);
                mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,mypk));
                mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,txfee,destpub));
                mtx.vout.push_back(CTxOut(funds-txfee,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('R',opentxid,mypk,destpub,param1,payment,closetxid)));
            }
            else
            {
                fprintf(stderr,"previous tx is invalid\n");
                return("");
            }
        }
        else
        {
            fprintf(stderr,"error adding CC inputs\n");
            return("");
        }
    }
    return("");
}

UniValue ChannelsInfo(uint256 channeltxid)
{
    UniValue result(UniValue::VOBJ); CTransaction tx,opentx; uint256 txid,tmp_txid,hashBlock,param3,opentxid,hashchain,prevtxid;
    struct CCcontract_info *cp,C; char myCCaddr[65],addr[65],str1[512],str2[256]; int32_t vout,numvouts,param1,numpayments;
    int64_t nValue,param2,payment; CPubKey srcpub,destpub,mypk;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;

    result.push_back(Pair("result","success"));
    cp = CCinit(&C,EVAL_CHANNELS);
    mypk = pubkey2pk(Mypubkey());
    if (channeltxid==zeroid)
    {
        result.push_back(Pair("name","Channels Info"));
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
                if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3) == 'O')
                {
                    GetCCaddress1of2(cp,addr,srcpub,destpub);
                    sprintf(str1,"%s - %lld payments of %lld satoshi - %s",addr,(long long)param1,(long long)param2,tx.GetHash().ToString().c_str());
                    result.push_back(Pair("Channel", str1));
                }
            }
        }
    }
    else
    {
        if (GetTransaction(channeltxid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 &&
            (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,param1,param2,param3) == 'O'))
        {
            GetCCaddress1of2(cp,addr,srcpub,destpub);
            sprintf(str1,"Channel %s",addr);
            result.push_back(Pair("name",str1));
            SetCCtxids(txids,addr);
            prevtxid=zeroid;
            for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
            {

                txid = it->first.txhash;
                if (txid!=prevtxid && GetTransaction(txid,tx,hashBlock,false) != 0 && (numvouts= tx.vout.size()) > 0 )
                {
                    if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,param1,param2,param3) == 'O' && tx.GetHash()==channeltxid)
                    {
                        sprintf(str1,"%lld payments of %lld satoshi",(long long)param1,(long long)param2);
                        result.push_back(Pair("Open", str1));
                    }
                    else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,param1,param2,param3) == 'P' && opentxid==channeltxid)
                    {
                        if (GetTransaction(opentxid,opentx,hashBlock,false) != 0 && (numvouts=opentx.vout.size()) > 0 &&
                            DecodeChannelsOpRet(opentx.vout[numvouts-1].scriptPubKey,tmp_txid,srcpub,destpub,numpayments,payment,hashchain) == 'O')
                        {
                            Getscriptaddress(str2,tx.vout[3].scriptPubKey);
                            sprintf(str1,"%lld satoshi to %s, %lld payments left",(long long)(param2*payment),str2,(long long)param1);
                            result.push_back(Pair("Payment",str1));
                        }
                    }
                    else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,param1,param2,param3) == 'C' && opentxid==channeltxid)
                    {
                        result.push_back(Pair("Close","channel"));
                    }
                    else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,opentxid,srcpub,destpub,param1,param2,param3) == 'R' && opentxid==channeltxid)
                    {
                        Getscriptaddress(str2,tx.vout[2].scriptPubKey);
                        sprintf(str1,"%lld satoshi back to %s",(long long)(param1*param2),str2);
                        result.push_back(Pair("Refund",str1));
                    }
                }
                prevtxid=txid;
            }
        }
    }
    return(result);
}
