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

#include "CCchannels.h"
#include "CCtokens.h"

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

#define CC_MARKER_VALUE 10000
#define CHANNELCC_VERSION 1
#define CC_TXFEE 10000

int64_t IsChannelsvout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey srcpub, CPubKey destpub,int32_t v)
{
    char destaddr[65],channeladdr[65],tokenschanneladdr[65];

    GetCCaddress1of2(cp,channeladdr,srcpub,destpub);
    GetTokensCCaddress1of2(cp,tokenschanneladdr,srcpub,destpub);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && (strcmp(destaddr,channeladdr) == 0 || strcmp(destaddr,tokenschanneladdr) == 0))
            return(tx.vout[v].nValue);
    }
    return(0); 
}

int64_t IsChannelsMarkervout(struct CCcontract_info *cp,const CTransaction& tx,CPubKey pubkey,int32_t v)
{
    char destaddr[65],ccaddr[65];

    GetCCaddress(cp,ccaddr,pubkey);
    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,ccaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

CScript EncodeChannelsOpRet(uint8_t funcid,uint256 tokenid,uint256 opentxid,CPubKey srcpub,CPubKey destpub,int32_t numpayments,int64_t payment,uint256 hashchain, uint8_t version, uint16_t confirmation)
{
    CScript opret; uint8_t evalcode = EVAL_CHANNELS;
    vscript_t vopret;

    vopret = E_MARSHAL(ss << evalcode << funcid << opentxid << srcpub << destpub << numpayments << payment << hashchain << version << confirmation);
    if (tokenid!=zeroid)
    {
        std::vector<CPubKey> pks;
        pks.push_back(srcpub);
        pks.push_back(destpub);
        return(EncodeTokenOpRetV1(tokenid,pks, { vopret }));
    }
    opret << OP_RETURN << vopret;
    return(opret);
}

uint8_t DecodeChannelsOpRet(const CScript &scriptPubKey, uint256 &tokenid, uint256 &opentxid, CPubKey &srcpub,CPubKey &destpub,int32_t &numpayments,int64_t &payment,uint256 &hashchain, uint8_t &version, uint16_t &confirmation)
{
    std::vector<vscript_t>  oprets;
    std::vector<uint8_t> vopret, vOpretExtra; uint8_t *script,e,f;
    std::vector<CPubKey> pubkeys;

    version=0;
    confirmation=100;
    if (DecodeTokenOpRetV1(scriptPubKey,tokenid,pubkeys,oprets)!=0 && GetOpReturnCCBlob(oprets, vOpretExtra) && vOpretExtra.size()>0)
    {
        vopret=vOpretExtra;
    }
    else GetOpReturnData(scriptPubKey, vopret);
    if ( vopret.size() > 2 )
    {
        script = (uint8_t *)vopret.data();
        if ( script[0] == EVAL_CHANNELS && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> opentxid; ss >> srcpub; ss >> destpub; ss >> numpayments; ss >> payment; ss >> hashchain; ss >> version; ss >> confirmation) != 0 )
        {
            return(f);
        }
        else LOGSTREAM("channelscc",CCLOG_DEBUG2, stream << "script[0] " << script[0] << " != EVAL_CHANNELS" << std::endl);
    } else LOGSTREAM("channelscc",CCLOG_DEBUG2, stream << "not enough opret.[" << vopret.size() << "]" << std::endl);
    return(0);
}

bool ChannelsExactAmounts(Eval* eval,const CTransaction &tx)
{
    uint256 txid,param3,tokenid;
    CPubKey srcpub,destpub; uint16_t confirmation;
    int32_t param1,numvouts; int64_t param2; uint8_t funcid,version;
    CTransaction vinTx; uint256 hashBlock; int64_t inputs=0,outputs=0;

    if ((numvouts=tx.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, param1, param2, param3, version, confirmation))!=0)
    {        
        switch (funcid)
        {
            case 'O':
                return (true);
            case 'P': case 'C': case 'R':
                if ( eval->GetTxUnconfirmed(tx.vin[0].prevout.hash,vinTx,hashBlock) == 0 )
                    return eval->Invalid("cant find vinTx");
                inputs = vinTx.vout[tx.vin[0].prevout.n].nValue;
                outputs = tx.vout[0].nValue;
                if (funcid=='P') outputs+=tx.vout[3].nValue; 
                break;  
            default:
                return (false);
        }
        if ( inputs != outputs )
        {
            LOGSTREAM("channelscc",CCLOG_ERROR, stream << "inputs " << inputs << " vs outputs " << outputs << std::endl);            
            return eval->Invalid("mismatched inputs != outputs");
        } 
        else return (true);       
    }
    else
    {
        return eval->Invalid("invalid op_return data");
    }
    return(false);
}

bool ValidateChannelOpenTx(Eval* eval,const CTransaction& channelOpenTx, uint256 tokenid, char* channeladdress,char* srcmarker,char* destmarker,char* srctokensaddr,char* srcaddr,int32_t numpayments, int64_t payment)
{
    int32_t i=0,numvouts; uint8_t funcid; struct CCcontract_info *cp,C; CTransaction prevTx; uint256 hashblock,tmptokenid;
    std::vector<CPubKey> keys; std::vector<vscript_t> oprets;

    CCOpretCheck(eval,channelOpenTx,true,true,true);
    ExactAmounts(eval,channelOpenTx,ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE);
    if (tokenid==zeroid)
    {
        for (int i=0;i<channelOpenTx.vin.size();i++)
            if (IsCCInput(channelOpenTx.vin[i].scriptSig) != 0 )
                return eval->Invalid("vin."+std::to_string(i)+" is normal for channelopen!");
        if (channelOpenTx.vout.size()!=5)
            return eval->Invalid("invalid number of vouts for channelsopen tx!");
        else if (ConstrainVout(channelOpenTx.vout[3],0,srcaddr,0)==0 )
            return eval->Invalid("vout.3 is normal change for channelopen!");
    }
    else
    {
        i=0;
        cp = CCinit(&C,EVAL_TOKENS);
        while (i<channelOpenTx.vin.size() && (cp->ismyvin)(channelOpenTx.vin[i].scriptSig) != 0)
        {  
            if (myGetTransaction(channelOpenTx.vin[i].prevout.hash,prevTx,hashblock)!= 0 && (numvouts=prevTx.vout.size()) > 0 
                && (funcid=DecodeTokenOpRetV1(prevTx.vout[numvouts-1].scriptPubKey, tmptokenid, keys, oprets))!=0 
                && ((funcid=='c' && prevTx.GetHash()==tokenid) || (funcid!='c' && tmptokenid==tokenid)))
                i++;
            else break;
        }
        while (i<channelOpenTx.vin.size() && IsCCInput(channelOpenTx.vin[i].scriptSig) == 0) i++;
        if (i!=channelOpenTx.vin.size())
            return eval->Invalid("invalid vin structure for channelopen!");
        else if (channelOpenTx.vout.size()!=6)
            return eval->Invalid("invalid number of vouts for channelsopen tx!");
        else if (ConstrainVout(channelOpenTx.vout[3],1,srctokensaddr,0)==0 )
            return eval->Invalid("vout.3 is Tokens CC change to srcpub for channelopen or invalid amount!");
        else if (ConstrainVout(channelOpenTx.vout[4],0,srcaddr,0)==0 )
            return eval->Invalid("vout.4 is normal change for channelopen!");
    }        
    if (numpayments<1 || numpayments>CHANNELS_MAXPAYMENTS)
        return eval->Invalid("invalid number of payments for channelsopen tx!");
    else if ( ConstrainVout(channelOpenTx.vout[0],1,channeladdress,numpayments*payment)==0 )
        return eval->Invalid("vout.0 is CC for channelopen or invalid amount!");
    else if ( ConstrainVout(channelOpenTx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
        return eval->Invalid("vout.1 is CC marker to srcpub for channelopen or invalid amount !");
    else if ( ConstrainVout(channelOpenTx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
        return eval->Invalid("vout.2 is CC marker to destpub for channelopen or invalid amount!");
    return (true);
}

bool ValidateChannelCloseTx(Eval* eval, uint256 closetxid, uint256 opentxid)
{
    CTransaction tx; uint256 hashblock,tokenid,param3,tmptxid; int32_t numvouts,param1; int64_t param2; CPubKey srcpub,destpub;
    uint8_t version; uint16_t tmp;
    
    if (myGetTransaction(closetxid,tx,hashblock)==0 || (numvouts=tx.vout.size())<1)
        return eval->Invalid("invalid channelsclose tx");
    else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, tokenid, tmptxid, srcpub, destpub, param1, param2, param3, version, tmp)!='C')
        return eval->Invalid("invalid channelsclose OP_RETURN data");
    else if (tmptxid!=opentxid)
        return eval->Invalid("invalid channelsclose not matching this channelsopen txid");
    return (true);
}

bool ValidateNormalVins(Eval* eval, const CTransaction& tx,int32_t index)
{
    for (int i=index;i<tx.vin.size();i++)
        if (IsCCInput(tx.vin[i].scriptSig) != 0 )
            return eval->Invalid("vin."+std::to_string(i)+" is normal for channel tx!");
    return (true);
}

bool ValidateChannelVin(struct CCcontract_info *cp,Eval* eval, const CTransaction& tx,int32_t index, uint256 opentxid, char* fromaddr,int64_t amount)
{
    CTransaction prevTx; uint256 hashblock,tokenid,tmp_txid,p3; CPubKey srcpub,destpub; int32_t p1,numvouts;
    int64_t p2; uint8_t version; uint16_t tmp; char tmpaddr[64];

    if ((*cp->ismyvin)(tx.vin[index].scriptSig) == 0)
        return eval->Invalid("vin."+std::to_string(index)+" is channel CC for channel tx!");
    else if (myGetTransaction(tx.vin[index].prevout.hash,prevTx,hashblock) == 0)
        return eval->Invalid("vin."+std::to_string(index)+" tx does not exist!");
    else if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3, version, tmp) == 0) 
        return eval->Invalid("invalid vin."+std::to_string(index)+" tx OP_RETURN data!");
    else if (fromaddr!=0 && Getscriptaddress(tmpaddr,prevTx.vout[tx.vin[index].prevout.n].scriptPubKey) && strcmp(tmpaddr,fromaddr)!=0)
        return eval->Invalid("invalid vin."+std::to_string(index)+" address!");
    else if (amount>0 && prevTx.vout[tx.vin[index].prevout.n].nValue!=amount)
        return eval->Invalid("vin."+std::to_string(index)+" invalid amount!");
    else if (prevTx.GetHash()!=opentxid && opentxid!=tmp_txid) 
        return eval->Invalid("invalid vin."+std::to_string(index)+" tx, different opent txid!");
    return (true);
}

bool ChannelsValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,i,numpayments,p1,param1; uint16_t confirmation,tmp;
    uint256 txid,hashblock,p3,param3,opentxid,tmp_txid,genhashchain,hashchain,tokenid,tmptokenid;
    uint8_t funcid,hash[32],hashdest[32],version; char channeladdress[65],srcmarker[65],destmarker[65],destaddr[65],srcaddr[65],desttokensaddr[65],srctokensaddr[65];
    int64_t p2,param2,payment;
    CPubKey srcpub, destpub;
    CTransaction channelOpenTx,channelCloseTx,prevTx;

    if (strcmp(ASSETCHAINS_SYMBOL, "MORTY") == 0 && GetLatestTimestamp(eval->GetCurrentHeight())<MAY2020_NNELECTION_HARDFORK)
        return (true);
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        CCOpretCheck(eval,tx,true,true,true);
        ExactAmounts(eval,tx,ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE);
        if (ChannelsExactAmounts(eval,tx) == false )
        {
            return (false);            
        }          
        if ( (funcid = DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey, tokenid, opentxid, srcpub, destpub, param1, param2, param3, version, tmp)) != 0)
        {
            if (myGetTransaction(opentxid,channelOpenTx,hashblock)== 0)
                return eval->Invalid("invalid channelopen tx!");
            else if ((channelOpenTx.vout.size()) > 0 && (DecodeChannelsOpRet(channelOpenTx.vout[channelOpenTx.vout.size()-1].scriptPubKey, tmptokenid, tmp_txid, srcpub, destpub, numpayments, payment, hashchain, version, confirmation)) != 'O')
                return eval->Invalid("invalid channelopen OP_RETURN data!");
            else if (tmptokenid!=tokenid)
                return eval->Invalid("invalid different tokenid in channelsopen and current tx!");
            if (tokenid==zeroid) GetCCaddress1of2(cp,channeladdress,srcpub,destpub);
            else GetTokensCCaddress1of2(cp,channeladdress,srcpub,destpub);
            GetCCaddress(cp,srcmarker,srcpub);
            GetCCaddress(cp,destmarker,destpub);
            Getscriptaddress(srcaddr,CScript() << ParseHex(HexStr(srcpub)) << OP_CHECKSIG);
            Getscriptaddress(destaddr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
            _GetCCaddress(srctokensaddr,EVAL_TOKENS,srcpub);
            _GetCCaddress(desttokensaddr,EVAL_TOKENS,destpub);
            switch ( funcid )
            {
                case 'O':
                    //normal coins
                    //vin.0: normal input
                    //...
                    //vin.n-1: normal input
                    //tokens
                    //vin.0: Tokens input
                    //...
                    //vin.m: Tokens input
                    //vin.n-m+1: normal input
                    //...
                    //vin.n-1: normal input
                    //vout.0: CC vout for channel funding on CC1of2 pubkey
                    //vout.1: CC vout marker to senders pubkey
                    //vout.2: CC vout marker to receiver pubkey
                    //normal coins
                    //vout.3: normal change
                    //vout.4: opreturn - 'O' zerotxid senderspubkey receiverspubkey totalnumberofpayments paymentamount hashchain version confirmation
                    //tokens
                    //vout.3: tokens CC change
                    //vout.4: normal change
                    //vout.5: opreturn - 'O' zerotxid senderspubkey receiverspubkey totalnumberofpayments paymentamount hashchain version confirmation
                    return eval->Invalid("unexpected ChannelsValidate for channelsopen!");
                case 'P':
                    //vin.0: CC input from channel funding
                    //vin.1: CC input from src marker
                    //vin.2: normal input
                    //...
                    //vin.n-1: normal input
                    //vout.0: CC vout change to CC1of2 pubkey
                    //vout.1: CC vout marker to senders pubkey
                    //vout.2: CC vout marker to receiver pubkey
                    //vout.3: normal/tokens CC output of payment amount to receiver pubkey
                    //vout.4: normal change
                    //vout.5: opreturn - 'P' opentxid senderspubkey receiverspubkey depth numpayments secret version confirmation
                    if (numvouts!=6)
                        return eval->Invalid("invalid number of vouts for channelspayment tx!");
                    else if (ValidateChannelOpenTx(eval,channelOpenTx,tokenid,channeladdress,srcmarker,destmarker,srctokensaddr,srcaddr,numpayments,payment)==0)
                        return (false);
                    else if (confirmation>0 && komodo_txnotarizedconfirmed(channelOpenTx.GetHash(),confirmation) == 0)
                        return eval->Invalid("channelopen is not yet confirmed(notarised)!");
                    else if ( ValidateChannelVin(cp,eval,tx,0,opentxid,channeladdress,0) == 0 )
                        return (false);
                    else if ( ValidateChannelVin(cp,eval,tx,1,opentxid,0,CC_MARKER_VALUE) == 0 )
                        return (false);
                    else if ( ValidateNormalVins(eval,tx,2) == 0 )
                        return (false);
                    else if ( ConstrainVout(tx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.1 is CC marker for channelpayment to srcpub or invalid amount!");
                    else if ( ConstrainVout(tx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.2 is CC marker for channelpayment to destpub or invalid amount!");
                    else if ( tokenid!=zeroid && ConstrainVout(tx.vout[3],1,desttokensaddr,param2*payment)==0 )
                        return eval->Invalid("vout.3 is Tokens CC for channelpayment or invalid amount or invalid receiver!");
                    else if ( tokenid==zeroid && ConstrainVout(tx.vout[3],0,destaddr,param2*payment)==0 )
                        return eval->Invalid("vout.3 is normal for channelpayment or invalid amount or invalid receiver!");
                    else if ( ConstrainVout(tx.vout[4],0,0,0)==0 )
                        return eval->Invalid("vout.4 is normal change!");
                    else if ( param1 > numpayments || param2 > numpayments)
                        return eval->Invalid("too many payment increments!");
                    else if ( param2 < 1)
                        return eval->Invalid("invalid depth or number of payments!");
                    else
                    {                           
                        endiancpy(hash, (uint8_t * ) & param3, 32);
                        for (i = 0; i < numpayments-param1; i++)
                        {
                            vcalc_sha256(0, hashdest, hash, 32);
                            memcpy(hash, hashdest, 32);
                        }
                        endiancpy((uint8_t*)&genhashchain,hashdest,32);
                        if (hashchain!=genhashchain)
                            return eval->Invalid("invalid secret for payment, does not reach final hashchain!");
                        if (myGetTransaction(tx.vin[0].prevout.hash,prevTx,hashblock) != 0)
                        {
                            if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3, version, tmp) == 0)
                                return eval->Invalid("invalid previous tx OP_RETURN data!");
                            else if ( ConstrainVout(tx.vout[0],1,channeladdress,(p1-param2)*payment)==0 )
                                return eval->Invalid("vout.0 is CC for channelpayment or invalid CC change amount!");
                            else if (param1+param2!=p1)
                                return eval->Invalid("invalid payment depth!");
                            else if (tx.vout[3].nValue > prevTx.vout[0].nValue)
                                return eval->Invalid("not enough funds in channel for that amount!");
                        }
                    }
                    break;
                case 'C':
                    //vin.0: CC input from channel funding
                    //vin.1: CC input from src marker
                    //vin.2: normal input
                    //...
                    //vin.n-1: normal input
                    //vout.0: CC vout for channel funding
                    //vout.1: CC vout marker to senders pubkey
                    //vout.2: CC vout marker to receiver pubkey
                    //vout.3: normal change
                    //vout.4: opreturn - 'C' opentxid senderspubkey receiverspubkey numpayments payment 0 version confirmation
                    if (numvouts!=5)
                        return eval->Invalid("invalid number of vouts for channelsclose tx!");
                    else if (ValidateChannelOpenTx(eval,channelOpenTx,tokenid,channeladdress,srcmarker,destmarker,srctokensaddr,srcaddr,numpayments,payment)==0)
                        return (false);
                    else if ( ValidateChannelVin(cp,eval,tx,0,opentxid,channeladdress,param1*payment) == 0 )
                        return (false);
                    else if ( ValidateChannelVin(cp,eval,tx,1,opentxid,srcmarker,CC_MARKER_VALUE) == 0 )
                        return (false);
                    else if ( ValidateNormalVins(eval,tx,2) == 0 )
                        return (false);
                    else if ( ConstrainVout(tx.vout[0],1,channeladdress,param1*payment)==0 )
                        return eval->Invalid("vout.0 is CC for channelclose or invalid amount!");
                    else if ( ConstrainVout(tx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.1 is CC marker for channelclose to srcpub or invalid amount!");
                    else if ( ConstrainVout(tx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.2 is CC marker for channelclose to destpub or invalid amount!");
                    else if ( ConstrainVout(tx.vout[3],0,srcaddr,0)==0 )
                        return eval->Invalid("vout.3 is normal change!");
                    else if ( param1 > numpayments)
                        return eval->Invalid("too many payment increments!");
                    else if ( param2 > payment)
                        return eval->Invalid("invalid payment size in OP_RETURN data for channelsclose tx!");
                    else if (myGetTransaction(tx.vin[0].prevout.hash,prevTx,hashblock) != 0)
                    {
                        if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3, version, tmp) == 0)
                            return eval->Invalid("invalid previous tx OP_RETURN data!");
                        else if (tx.vout[0].nValue != prevTx.vout[0].nValue)
                            return eval->Invalid("invalid CC amount, amount must match funds in channel");
                    }
                    break;
                case 'R':
                    //vin.0: CC input from channel funding
                    //vin.1: CC input from src marker
                    //vin.2: normal input
                    //...
                    //vin.n-1: normal input
                    //vout.0: normal/tokens CC output to senders pubkey
                    //vout.1: CC vout marker to senders pubkey
                    //vout.2: CC vout marker to receiver pubKey
                    //vout.3: normal change
                    //vout.4: opreturn - 'R' opentxid senderspubkey receiverspubkey numpayments payment closetxid version confirmation
                    if (numvouts!=5)
                        return eval->Invalid("invalid number of vouts for channelsrefund tx!");
                    else if (ValidateChannelOpenTx(eval,channelOpenTx,tokenid,channeladdress,srcmarker,destmarker,srctokensaddr,srcaddr,numpayments,payment)==0)
                        return (false);
                    else if (ValidateChannelCloseTx(eval,param3,opentxid)==0)
                        return (false);
                    else if (komodo_txnotarizedconfirmed(opentxid,confirmation) == 0)
                        return eval->Invalid("channelopen is not yet confirmed(notarised)!");
                    else if (komodo_txnotarizedconfirmed(param3,confirmation) == 0)
                        return eval->Invalid("channelClose is not yet confirmed(notarised)!");
                    else if ( ValidateChannelVin(cp,eval,tx,0,opentxid,channeladdress,param1*param2) == 0 )
                        return (false);
                    else if ( ValidateChannelVin(cp,eval,tx,1,opentxid,srcmarker,CC_MARKER_VALUE) == 0 )
                        return (false);
                    else if ( ValidateNormalVins(eval,tx,2) == 0 )
                        return (false);
                    else if ( tokenid!=zeroid && ConstrainVout(tx.vout[0],1,srctokensaddr,param1*payment)==0 )
                        return eval->Invalid("vout.0 is Tokens CC for channelrefund or invalid amount or invalid receiver!");
                    else if ( tokenid==zeroid && ConstrainVout(tx.vout[0],0,srcaddr,param1*payment)==0 )
                        return eval->Invalid("vout.0 is normal for channelrefund or invalid amount or invalid receiver!");
                    else if ( ConstrainVout(tx.vout[1],1,srcmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.1 is CC marker for channelrefund to srcpub or invalid amount!");
                    else if ( ConstrainVout(tx.vout[2],1,destmarker,CC_MARKER_VALUE)==0 )
                        return eval->Invalid("vout.2 is CC marker for channelrefund to destpub or invalid amount!");
                    else if ( ConstrainVout(tx.vout[3],0,srcaddr,0)==0 )
                        return eval->Invalid("vout.3 is normal change!");
                    else if ( param1 > numpayments)
                        return eval->Invalid("too many payment increments!");
                    else if ( param2 > payment)
                        return eval->Invalid("invalid payment size in OP_RETURN data for channelsrefund tx!");
                    else if (myGetTransaction(tx.vin[0].prevout.hash,prevTx,hashblock) != 0)
                    {
                        if ((numvouts=prevTx.vout.size()) > 0 && DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, tmp_txid, srcpub, destpub, p1, p2, p3, version, tmp) == 0)
                            return eval->Invalid("invalid previous tx OP_RETURN data!");
                        else if (tx.vout[0].nValue != prevTx.vout[0].nValue)
                            return eval->Invalid("invalid amount, refund amount and funds in channel must match!");
                    }
                    break;
                default:
                    fprintf(stderr,"illegal channels funcid.(%c)\n",funcid);
                    return eval->Invalid("unexpected channels funcid");
            }
        }
        else return eval->Invalid("invalid opret vout for channels tx");
        LOGSTREAM("channels",CCLOG_INFO, stream << "Channels tx validated" << std::endl);
        return(true);
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddChannelsInputs(struct CCcontract_info *cp,CMutableTransaction &mtx, CTransaction openTx, uint256 &prevtxid, CPubKey mypk)
{
    char coinaddr[65],funcid; int64_t param2,totalinputs = 0,numvouts; uint256 txid=zeroid,tmp_txid,hashBlock,param3,tokenid; CTransaction tx; int32_t marker,param1;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; CPubKey srcpub,destpub; uint8_t myprivkey[32],version; uint16_t confirmation;

    if ((numvouts=openTx.vout.size()) > 0 && DecodeChannelsOpRet(openTx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,version,confirmation)=='O')
    {
        if (tokenid!=zeroid) GetTokensCCaddress1of2(cp,coinaddr,srcpub,destpub);
        else GetCCaddress1of2(cp,coinaddr,srcpub,destpub);
        SetCCunspents(unspentOutputs,coinaddr,true);
    }
    else
    {
        LOGSTREAM("channelscc",CCLOG_INFO, stream << "invalid channel open txid" << std::endl);
        return 0;
    }
    if (srcpub==mypk) marker=1;
    else marker=2;
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        if ( (int32_t)it->first.index==0 && myGetTransaction(it->first.txhash,tx,hashBlock) != 0 && (numvouts=tx.vout.size()) > 0)
        {
            if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,version,confirmation)!=0 &&
              (tmp_txid==openTx.GetHash() || tx.GetHash()==openTx.GetHash()) && IsChannelsMarkervout(cp,tx,marker==1?srcpub:destpub,marker)>0 &&
              (totalinputs=IsChannelsvout(cp,tx,srcpub,destpub,0))>0)
            {
                txid = it->first.txhash;
                break;
            }
        }
    }
    if (txid!=zeroid && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,0) != 0)
    {
        txid=zeroid;
        int32_t mindepth=CHANNELS_MAXPAYMENTS;
        std::vector<CTransaction> tmp_txs;
        myGet_mempool_txs(tmp_txs,EVAL_CHANNELS,0);
        for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
        {
            const CTransaction &txmempool = *it;
            const uint256 &hash = txmempool.GetHash();

            if ((numvouts=txmempool.vout.size()) > 0 && (funcid=DecodeChannelsOpRet(txmempool.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,version,confirmation))!=0 && (funcid=='P' || funcid=='C') &&
              tmp_txid==openTx.GetHash() && param1 < mindepth)
            {
                txid=hash;
                totalinputs=txmempool.vout[0].nValue;
                mindepth=param1;
            }
        }
    }
    if (txid != zeroid)
    {
        prevtxid=txid;
        mtx.vin.push_back(CTxIn(txid,0,CScript()));
        mtx.vin.push_back(CTxIn(txid,marker,CScript()));
        Myprivkey(myprivkey);        
        if (tokenid!=zeroid) CCaddrTokens1of2set(cp,srcpub,destpub,myprivkey,coinaddr);
        else CCaddr1of2set(cp,srcpub,destpub,myprivkey,coinaddr);
        memset(myprivkey,0,32);
        return totalinputs;
    }
    else return 0;
}

UniValue ChannelOpen(const CPubKey& pk, uint64_t txfee,CPubKey destpub,int32_t numpayments,int64_t payment,uint16_t confirmation,uint256 tokenid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    uint8_t hash[32],hashdest[32]; uint64_t amount,tokens=0,funds; int32_t i; uint256 hashchain,entropy,hentropy;
    CPubKey mypk; struct CCcontract_info *cp,*cpTokens,C,CTokens;
    
    if ( numpayments <= 0 || payment <= 0 || numpayments > CHANNELS_MAXPAYMENTS )
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid ChannelOpen param numpayments." << numpayments << " payment." << payment << " - max_numpayments." << CHANNELS_MAXPAYMENTS);
    if (!destpub.IsFullyValid())
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid destination pubkey");
    if (numpayments <1)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid number of payments, must be greater than 0");
    if (payment <1)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid payment amount, must be greater than 0");
    cp = CCinit(&C,EVAL_CHANNELS);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    funds = numpayments * payment;
    if (tokenid!=zeroid)
    {
        tokens=AddTokenCCInputs(cpTokens, mtx, mypk, tokenid, funds, 64);       
        amount=AddNormalinputs(mtx,mypk,txfee+2*CC_MARKER_VALUE,5,pk.IsValid());
    }
    else amount=AddNormalinputs(mtx,mypk,funds+txfee+2*CC_MARKER_VALUE,64,pk.IsValid());
    if (amount+tokens >= funds+txfee+2*CC_MARKER_VALUE)
    {
        hentropy = DiceHashEntropy(entropy,mtx.vin[0].prevout.hash,mtx.vin[0].prevout.n,1);
        endiancpy(hash,(uint8_t *)&hentropy,32);
        for (i=0; i<numpayments; i++)
        {
            vcalc_sha256(0,hashdest,hash,32);
            memcpy(hash,hashdest,32);
        }
        endiancpy((uint8_t *)&hashchain,hashdest,32);
        if (tokenid!=zeroid) mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_CHANNELS,funds,mypk,destpub));
        else mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS,funds,mypk,destpub));
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,mypk));
        mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,destpub));
        if (tokenid!=zeroid && tokens>=funds) mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,tokens-funds,mypk));
        return(FinalizeCCTxExt(pk.IsValid(),0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('O',tokenid,zeroid,mypk,destpub,numpayments,payment,hashchain,CHANNELCC_VERSION,confirmation)));
    }
    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding funds");
}

UniValue ChannelPayment(const CPubKey& pk, uint64_t txfee,uint256 opentxid,int64_t amount, uint256 secret)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,srcpub,destpub; uint256 txid,hashchain,gensecret,hashblock,entropy,hentropy,prevtxid,param3,tokenid;
    struct CCcontract_info *cp,C; int32_t i,funcid,prevdepth,numvouts,numpayments,totalnumpayments; uint16_t confirmation,tmpc; 
    int64_t payment,change,funds,param2;
    uint8_t hash[32],hashdest[32],version,tmpv;
    CTransaction channelOpenTx,prevTx;

    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if (amount <1)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid payment amount, must be greater than 0");
    if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0) 
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open txid");
    if ((numvouts=channelOpenTx.vout.size()) > 0 && DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, totalnumpayments, payment, hashchain, version, confirmation)=='O')
    {
        if (mypk != srcpub && mypk != destpub) 
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "this is not our channel");
        else if (amount % payment != 0 || amount<payment)
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid amount, not a magnitude of payment size");
        else if (mypk == destpub && secret==zeroid) CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid secret, secret is necessary when making payment from destination");
    }
    else 
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open tx");
    if (confirmation>0 && komodo_txnotarizedconfirmed(opentxid,confirmation)==false)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "channelsopen tx not yet confirmed/notarized");
    if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid,mypk)) !=0 && (change=funds-amount)>=0)
    {
        if (AddNormalinputs(mtx,mypk,txfee+CC_MARKER_VALUE,3,pk.IsValid()) >= txfee+CC_MARKER_VALUE)
        {            
            numpayments=amount/payment;
            if (myGetTransaction(prevtxid,prevTx,hashblock) != 0 && (numvouts=prevTx.vout.size()) > 0 &&
                ((funcid = DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, prevdepth, param2, param3, tmpv, tmpc)) != 0) &&
                (funcid == 'O' || funcid=='P' || funcid=='C'))
            {
                if (numpayments > prevdepth)
                    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "not enough funds in channel for that amount");
                else if (numpayments == 0)
                    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid amount");
                if (secret!=zeroid)
                {
                    endiancpy(hash, (uint8_t * ) & secret, 32);
                    for (i = 0; i < totalnumpayments-(prevdepth-numpayments); i++)
                    {
                        vcalc_sha256(0, hashdest, hash, 32);
                        memcpy(hash, hashdest, 32);
                    }
                    endiancpy((uint8_t * ) & gensecret, hashdest, 32);
                    if (gensecret!=hashchain) CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid secret supplied");
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
                CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid previous tx");
            if (tokenid!=zeroid) mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_CHANNELS, change, srcpub, destpub));
            else mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS, change, srcpub, destpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,srcpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,destpub));
            if (tokenid!=zeroid) mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS, amount, destpub));
            else mtx.vout.push_back(CTxOut(amount, CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG));
            return (FinalizeCCTxExt(pk.IsValid(), 0, cp, mtx, mypk, txfee, EncodeChannelsOpRet('P', tokenid, opentxid, srcpub, destpub, prevdepth-numpayments, numpayments, secret, CHANNELCC_VERSION, confirmation)));
        }
        else CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding normal inputs");
    }
    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding CC inputs");
}

UniValue ChannelClose(const CPubKey& pk, uint64_t txfee,uint256 opentxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,srcpub,destpub; struct CCcontract_info *cp,C;
    CTransaction channelOpenTx; uint256 hashblock,tmp_txid,prevtxid,hashchain,tokenid; int32_t numvouts,numpayments;
    int64_t payment,funds; uint16_t confirmation; uint8_t version;

    // verify this is one of our outbound channels
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0) 
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open txid");
    if ((numvouts=channelOpenTx.vout.size()) < 1 || DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,numpayments,payment,hashchain,version,confirmation)!='O')
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open tx");
    if (mypk != srcpub)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "cannot close, you are not channel owner");
    if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid,mypk)) !=0 && funds>0)
    {
        if ( AddNormalinputs(mtx,mypk,txfee+CC_MARKER_VALUE,3,pk.IsValid()) >= txfee+CC_MARKER_VALUE )
        {
            if (tokenid!=zeroid) mtx.vout.push_back(MakeTokensCC1of2vout(EVAL_CHANNELS, funds, mypk, destpub));
            else mtx.vout.push_back(MakeCC1of2vout(EVAL_CHANNELS, funds, mypk, destpub));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,mypk));
            mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,destpub));
            return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('C',tokenid,opentxid,mypk,destpub,funds/payment,payment,zeroid,CHANNELCC_VERSION,confirmation)));
        }
        else
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding normal inputs");
    }
    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding CC inputs");
}

UniValue ChannelRefund(const CPubKey& pk, uint64_t txfee,uint256 opentxid,uint256 closetxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; struct CCcontract_info *cp,C; int64_t funds,payment,param2;
    int32_t i,numpayments,numvouts,param1; uint16_t confirmation,tmpc; uint8_t version,tmpv;
    uint256 hashchain,hashblock,txid,prevtxid,param3,tokenid;
    CTransaction channelOpenTx,channelCloseTx,prevTx;
    CPubKey srcpub,destpub;

    // verify stoptxid and origtxid match and are mine
    cp = CCinit(&C,EVAL_CHANNELS);
    if ( txfee == 0 )
        txfee = ASSETCHAINS_CCZEROTXFEE[EVAL_CHANNELS]?0:CC_TXFEE;
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open txid");
    if ((numvouts=channelOpenTx.vout.size()) < 1 || DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey,tokenid,txid,srcpub,destpub,numpayments,payment,hashchain,version,confirmation)!='O')
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open tx");
    if (komodo_txnotarizedconfirmed(opentxid,confirmation)==false)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "channelsopen tx not yet confirmed/notarized");
    if (myGetTransaction(closetxid,channelCloseTx,hashblock) == 0)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel close txid");
    if ((numvouts=channelCloseTx.vout.size()) < 1 || DecodeChannelsOpRet(channelCloseTx.vout[numvouts-1].scriptPubKey,tokenid,txid,srcpub,destpub,param1,param2,param3,tmpv,tmpc)!='C')
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel close tx");
    if (komodo_txnotarizedconfirmed(closetxid,confirmation)==false)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "channelsclose tx not yet confirmed/notarized");
    if (txid!=opentxid)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "open and close txid are not from same channel");
    if (mypk != srcpub)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "cannot refund, you are not the channel owner");
    if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid,mypk)) !=0 && funds>0)
    {
        if ( AddNormalinputs(mtx,mypk,txfee+CC_MARKER_VALUE,3,pk.IsValid()) >= txfee+CC_MARKER_VALUE )
        {
            if ((myGetTransaction(prevtxid,prevTx,hashblock) != 0) && (numvouts=prevTx.vout.size()) > 0 &&
                DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, param1, param2, param3,tmpv,tmpc) != 0)
            {
                if (tokenid!=zeroid) mtx.vout.push_back(MakeCC1vout(EVAL_TOKENS,funds,mypk));
                else mtx.vout.push_back(CTxOut(funds,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,mypk));
                mtx.vout.push_back(MakeCC1vout(EVAL_CHANNELS,CC_MARKER_VALUE,destpub));
                return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeChannelsOpRet('R',tokenid,opentxid,mypk,destpub,funds/payment,payment,closetxid,CHANNELCC_VERSION,confirmation)));
            }
            else
                CCERR_RESULT("channelscc",CCLOG_INFO, stream << "previous tx is invalid");
        }
        else
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding normal inputs");
    }
    CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding CC inputs");
}

UniValue ChannelGenerateSecret(const CPubKey& pk,uint256 opentxid,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,srcpub,destpub; uint256 txid,hashchain,gensecret,hashblock,entropy,hentropy,prevtxid,param3,tokenid;
    struct CCcontract_info *cp,C; int32_t i,funcid,prevdepth,numvouts,numpayments,totalnumpayments; uint16_t confirmation,tmpc;
    int64_t payment,change,funds,param2; UniValue result(UniValue::VOBJ);
    uint8_t hash[32],hashdest[32],version,tmpv;
    CTransaction channelOpenTx,prevTx;

    result.push_back(Pair("result","success"));
    cp = CCinit(&C,EVAL_CHANNELS);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    if (amount <1)
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid payment amount, must be greater than 0");
    if (myGetTransaction(opentxid,channelOpenTx,hashblock) == 0) 
        CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open txid");
    if ((numvouts=channelOpenTx.vout.size()) > 0 && DecodeChannelsOpRet(channelOpenTx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, totalnumpayments, payment, hashchain, version, confirmation)=='O')
    {
        if (mypk != srcpub) 
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "this is not our channel");
        else if (amount % payment != 0 || amount<payment)
            CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid amount, not a magnitude of payment size");
    }
    else CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid channel open tx");
    numpayments=amount/payment;
    if ((funds=AddChannelsInputs(cp,mtx,channelOpenTx,prevtxid,mypk)) !=0 && (change=funds-amount)>=0)
    {      
        if (myGetTransaction(prevtxid,prevTx,hashblock) != 0 && (numvouts=prevTx.vout.size()) > 0 &&
            ((funcid = DecodeChannelsOpRet(prevTx.vout[numvouts-1].scriptPubKey, tokenid, txid, srcpub, destpub, prevdepth, param2, param3, tmpv, tmpc)) != 0) &&
            (funcid == 'O' || funcid=='P' || funcid=='C'))
        {
            if (numpayments > prevdepth)
                CCERR_RESULT("channelscc",CCLOG_INFO, stream << "not enough funds in channel for that amount");
            else if (numpayments == 0)
                CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid amount");
            hentropy = DiceHashEntropy(entropy,channelOpenTx.vin[0].prevout.hash,channelOpenTx.vin[0].prevout.n,1);
            if (prevdepth-numpayments)
            {
                endiancpy(hash, (uint8_t * ) & hentropy, 32);
                for (i = 0; i < prevdepth-numpayments; i++)
                {
                    vcalc_sha256(0, hashdest, hash, 32);
                    memcpy(hash, hashdest, 32);
                }
                endiancpy((uint8_t * ) & gensecret, hashdest, 32);
            }
            else endiancpy((uint8_t * ) & gensecret, (uint8_t * ) & hentropy, 32);
            result.push_back(Pair("payment secret",gensecret.GetHex()));
            return (result);
        }
        else CCERR_RESULT("channelscc",CCLOG_INFO, stream << "invalid previous channel tx");
    }
    else CCERR_RESULT("channelscc",CCLOG_INFO, stream << "error adding CC inputs");
}

UniValue ChannelsList(const CPubKey& pk)
{
    UniValue result(UniValue::VOBJ); std::vector<uint256> txids; struct CCcontract_info *cp,C; uint256 txid,hashBlock,tmp_txid,param3,tokenid;
    CTransaction tx; char myCCaddr[65],str[512],pub[34]; CPubKey mypk,srcpub,destpub; int32_t vout,numvouts,param1;
    int64_t nValue,param2; uint16_t confirmation; uint8_t version;

    cp = CCinit(&C,EVAL_CHANNELS);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    GetCCaddress(cp,myCCaddr,mypk);
    SetCCtxids(txids,myCCaddr,true,EVAL_CHANNELS,CC_MARKER_VALUE,zeroid,'O');
    result.push_back(Pair("result","success"));
    result.push_back(Pair("name","Channels List"));
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 )
        {
            if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,version,confirmation) == 'O')
            {                
                sprintf(str,"%lld payments of %lld satoshi to %s",(long long)param1,(long long)param2,pubkey33_str(pub,(uint8_t *)&destpub));                
                result.push_back(Pair(txid.GetHex().data(),str));
            }
        }
    }
    return(result);
}

UniValue ChannelsInfo(const CPubKey& pk,uint256 channeltxid)
{
    UniValue result(UniValue::VOBJ),array(UniValue::VARR); CTransaction tx,opentx; uint256 txid,tmp_txid,hashBlock,param3,opentxid,hashchain,tokenid;
    struct CCcontract_info *cp,C; char CCaddr[65],addr[65],str[512],funcid; int32_t vout,numvouts,param1,numpayments;
    int64_t param2,payment; CPubKey srcpub,destpub,mypk; uint16_t confirmation,tmpc; uint8_t version,tmpv;
    std::vector<uint256> txids; std::vector<CTransaction> txs;
    
    cp = CCinit(&C,EVAL_CHANNELS);
    mypk = pk.IsValid()?pk:pubkey2pk(Mypubkey());
    
    if (myGetTransaction(channeltxid,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 &&
        (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,opentxid,srcpub,destpub,param1,param2,param3,version,confirmation) == 'O'))
    {    
        GetCCaddress1of2(cp,CCaddr,srcpub,destpub);
        Getscriptaddress(addr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG);
        result.push_back(Pair("result","success"));
        result.push_back(Pair("Channel CC address",CCaddr));
        result.push_back(Pair("Destination address",addr));
        result.push_back(Pair("Number of payments",param1));
        if(tokenid!=zeroid)
        {
            result.push_back(Pair("Token id",tokenid.GetHex().data()));
            result.push_back(Pair("Denomination (token satoshi)",i64tostr(param2)));
            result.push_back(Pair("Amount (token satoshi)",i64tostr(param1*param2)));
        }
        else
        {
            result.push_back(Pair("Denomination (satoshi)",i64tostr(param2)));
            result.push_back(Pair("Amount (satoshi)",i64tostr(param1*param2)));
        }
        GetCCaddress(cp,CCaddr,mypk);
        SetCCtxids(txids,CCaddr,true,EVAL_CHANNELS,CC_MARKER_VALUE,channeltxid,0);                      
        for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
        {
            if (myGetTransaction(*it,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 &&
                DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,tmpv,tmpc)!=0 && (tmp_txid==channeltxid || tx.GetHash()==channeltxid))
                    txs.push_back(tx);               
        }
        std::vector<CTransaction> tmp_txs;
        myGet_mempool_txs(tmp_txs,EVAL_CHANNELS,0);
        for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
        {
            const CTransaction &txmempool = *it;

            if ((numvouts=txmempool.vout.size()) > 0 && txmempool.vout[1].nValue==CC_MARKER_VALUE && (funcid=DecodeChannelsOpRet(txmempool.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,tmpv,tmpc)) != 0 && (funcid=='P' || funcid=='C') && tmp_txid==channeltxid)
                txs.push_back(txmempool);                
        }
        for (std::vector<CTransaction>::const_iterator it=txs.begin(); it!=txs.end(); it++)
        {
            tx=*it;
            if ((numvouts= tx.vout.size()) > 0 )
            {
                UniValue obj(UniValue::VOBJ);               
                if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,param1,param2,param3,version,confirmation) == 'O' && tx.GetHash()==channeltxid)
                {
                    obj.push_back(Pair("Open",tx.GetHash().GetHex().data()));
                }
                else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,opentxid,srcpub,destpub,param1,param2,param3,tmpv,tmpc) == 'P' && opentxid==channeltxid)
                {
                    if (myGetTransaction(opentxid,opentx,hashBlock) != 0 && (numvouts=opentx.vout.size()) > 0 &&
                            DecodeChannelsOpRet(opentx.vout[numvouts-1].scriptPubKey,tokenid,tmp_txid,srcpub,destpub,numpayments,payment,hashchain,version,confirmation) == 'O')
                    {
                        Getscriptaddress(str,tx.vout[3].scriptPubKey);  
                        obj.push_back(Pair("Payment",tx.GetHash().GetHex().data()));
                        obj.push_back(Pair("Number of payments",param2));
                        obj.push_back(Pair("Amount",param2*payment));
                        obj.push_back(Pair("Destination",str));
                        obj.push_back(Pair("Secret",param3.ToString().c_str()));
                        obj.push_back(Pair("Payments left",param1));
                    }
                }
                else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,opentxid,srcpub,destpub,param1,param2,param3,tmpv,tmpc) == 'C' && opentxid==channeltxid)
                {
                    obj.push_back(Pair("Close",tx.GetHash().GetHex().data()));
                }
                else if (DecodeChannelsOpRet(tx.vout[numvouts-1].scriptPubKey,tokenid,opentxid,srcpub,destpub,param1,param2,param3,tmpv,tmpc) == 'R' && opentxid==channeltxid)
                {
                    Getscriptaddress(str,tx.vout[2].scriptPubKey);                        
                    obj.push_back(Pair("Refund",tx.GetHash().GetHex().data()));                        
                    obj.push_back(Pair("Amount",param1*param2));
                    obj.push_back(Pair("Destination",str));
                }
                array.push_back(obj);
            }
        }        
        result.push_back(Pair("Transactions",array));
    }
    else
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("Error","Channel not found!"));
    }
    return(result);
}
