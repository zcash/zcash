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

#include "CCImportGateway.h"
#include "key_io.h"
#include "../importcoin.h"

// start of consensus code

#define KMD_PUBTYPE 60
#define KMD_P2SHTYPE 85
#define KMD_WIFTYPE 188
#define KMD_TADDR 0
#define CC_MARKER_VALUE 10000

extern uint256 KOMODO_EARLYTXID;

CScript EncodeImportGatewayBindOpRet(uint8_t funcid,std::string coin,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> importgatewaypubkeys,uint8_t taddr,uint8_t prefix,uint8_t prefix2,uint8_t wiftype)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;    
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << coin << oracletxid << M << N << importgatewaypubkeys << taddr << prefix << prefix2 << wiftype);
    return(opret);
}

uint8_t DecodeImportGatewayBindOpRet(char *burnaddr,const CScript &scriptPubKey,std::string &coin,uint256 &oracletxid,uint8_t &M,uint8_t &N,std::vector<CPubKey> &importgatewaypubkeys,uint8_t &taddr,uint8_t &prefix,uint8_t &prefix2,uint8_t &wiftype)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f; std::vector<CPubKey> pubkeys;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    burnaddr[0] = 0;
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> coin; ss >> oracletxid; ss >> M; ss >> N; ss >> importgatewaypubkeys; ss >> taddr; ss >> prefix; ss >> prefix2; ss >> wiftype) != 0 )
    {
        if ( prefix == KMD_PUBTYPE && prefix2 == KMD_P2SHTYPE )
        {
            if ( N > 1 )
            {
                strcpy(burnaddr,CBitcoinAddress(CScriptID(GetScriptForMultisig(M,importgatewaypubkeys))).ToString().c_str());
                LOGSTREAM("importgateway", CCLOG_DEBUG1, stream << "f." << f << " M." << (int)M << " of N." << (int)N << " size." << (int32_t)importgatewaypubkeys.size() << " -> " << burnaddr << std::endl);
            } else Getscriptaddress(burnaddr,CScript() << ParseHex(HexStr(importgatewaypubkeys[0])) << OP_CHECKSIG);
        }
        else
        {
            if ( N > 1 ) strcpy(burnaddr,CCustomBitcoinAddress(CScriptID(GetScriptForMultisig(M,importgatewaypubkeys)),taddr,prefix,prefix2).ToString().c_str());
            else GetCustomscriptaddress(burnaddr,CScript() << ParseHex(HexStr(importgatewaypubkeys[0])) << OP_CHECKSIG,taddr,prefix,prefix2);
        }
        return(f);
    } else LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "error decoding bind opret" << std::endl);
    return(0);
}

CScript EncodeImportGatewayDepositOpRet(uint8_t funcid,uint256 bindtxid,std::string refcoin,std::vector<CPubKey> publishers,std::vector<uint256>txids,int32_t height,uint256 burntxid,int32_t claimvout,std::string deposithex,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << refcoin << bindtxid << publishers << txids << height << burntxid << claimvout << deposithex << proof << destpub << amount);
    return(opret);
}

uint8_t DecodeImportGatewayDepositOpRet(const CScript &scriptPubKey,uint256 &bindtxid,std::string &refcoin,std::vector<CPubKey>&publishers,std::vector<uint256>&txids,int32_t &height,uint256 &burntxid, int32_t &claimvout,std::string &deposithex,std::vector<uint8_t> &proof,CPubKey &destpub,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> refcoin; ss >> bindtxid; ss >> publishers; ss >> txids; ss >> height; ss >> burntxid; ss >> claimvout; ss >> deposithex; ss >> proof; ss >> destpub; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayWithdrawOpRet(uint8_t funcid,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << bindtxid << refcoin << withdrawpub << amount);        
    return(opret);
}

uint8_t DecodeImportGatewayWithdrawOpRet(const CScript &scriptPubKey,uint256 &bindtxid,std::string &refcoin,CPubKey &withdrawpub,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> bindtxid; ss >> refcoin; ss >> withdrawpub; ss >> amount) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayPartialOpRet(uint8_t funcid, uint256 withdrawtxid,std::string refcoin,uint8_t K, CPubKey signerpk,std::string hex)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin  << K << signerpk << hex);        
    return(opret);
}

uint8_t DecodeImportGatewayPartialOpRet(const CScript &scriptPubKey,uint256 &withdrawtxid,std::string &refcoin,uint8_t &K,CPubKey &signerpk,std::string &hex)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;
    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> K; ss >> signerpk; ss >> hex) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayCompleteSigningOpRet(uint8_t funcid,uint256 withdrawtxid,std::string refcoin,uint8_t K,std::string hex)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;
    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin << K << hex);        
    return(opret);
}

uint8_t DecodeImportGatewayCompleteSigningOpRet(const CScript &scriptPubKey,uint256 &withdrawtxid,std::string &refcoin,uint8_t &K,std::string &hex)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> K; ss >> hex) != 0 )
    {
        return(f);
    }
    return(0);
}

CScript EncodeImportGatewayMarkDoneOpRet(uint8_t funcid,uint256 withdrawtxid,std::string refcoin,uint256 completetxid)
{
    CScript opret; uint8_t evalcode = EVAL_IMPORTGATEWAY;

    opret << OP_RETURN << E_MARSHAL(ss << evalcode << funcid << withdrawtxid << refcoin << completetxid);        
    return(opret);
}

uint8_t DecodeImportGatewayMarkDoneOpRet(const CScript &scriptPubKey, uint256 &withdrawtxid, std::string &refcoin, uint256 &completetxid)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> withdrawtxid; ss >> refcoin; ss >> completetxid;) != 0 )
    {
        return(f);
    }
    return(0);
}

uint8_t DecodeImportGatewayOpRet(const CScript &scriptPubKey)
{
    std::vector<uint8_t> vopret; uint8_t *script,e,f;

    GetOpReturnData(scriptPubKey, vopret);
    script = (uint8_t *)vopret.data();
    if ( vopret.size() > 2 && script[0] == EVAL_IMPORTGATEWAY)
    {
        f=script[1];
        if (f == 'B' || f == 'D' || f == 'C' || f == 'W' || f == 'P' || f == 'S' || f == 'M')
          return(f);
    }
    return(0);
}

int64_t IsImportGatewayvout(struct CCcontract_info *cp,const CTransaction& tx,int32_t v)
{
    char destaddr[64];

    if ( tx.vout[v].scriptPubKey.IsPayToCryptoCondition() != 0 )
    {
        if ( Getscriptaddress(destaddr,tx.vout[v].scriptPubKey) > 0 && strcmp(destaddr,cp->unspendableCCaddr) == 0 )
            return(tx.vout[v].nValue);
    }
    return(0);
}

int64_t ImportGatewayVerify(char *refburnaddr,uint256 oracletxid,int32_t claimvout,std::string refcoin,uint256 burntxid,const std::string deposithex,std::vector<uint8_t>proof,uint256 merkleroot,CPubKey destpub,uint8_t taddr,uint8_t prefix,uint8_t prefix2)
{
    std::vector<uint256> txids; uint256 proofroot,hashBlock,txid = zeroid; CTransaction tx; std::string name,description,format;
    char destaddr[64],destpubaddr[64],claimaddr[64]; int32_t i,numvouts; int64_t nValue = 0;
    
    if ( myGetTransaction(oracletxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify cant find oracletxid " << oracletxid.GetHex() << std::endl);
        return(0);
    }
    if ( DecodeOraclesCreateOpRet(tx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' || name != refcoin )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify mismatched oracle name " << name << " != " << refcoin << std::endl);
        return(0);
    }
    proofroot = BitcoinGetProofMerkleRoot(proof,txids);
    if ( proofroot != merkleroot )
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify mismatched merkleroot " << proofroot.GetHex() << " != " << merkleroot.GetHex() << std::endl);
        return(0);
    }
    if (std::find(txids.begin(), txids.end(), burntxid) == txids.end())
    {
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGatewayVerify invalid proof for this burntxid " << burntxid.GetHex() << std::endl);
        return 0;
    }
    if ( DecodeHexTx(tx,deposithex) != 0 )
    {
        GetCustomscriptaddress(claimaddr,tx.vout[claimvout].scriptPubKey,taddr,prefix,prefix2);
        GetCustomscriptaddress(destpubaddr,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG,taddr,prefix,prefix2);
        if ( strcmp(claimaddr,destpubaddr) == 0 )
        {
            for (i=0; i<numvouts; i++)
            {
                GetCustomscriptaddress(destaddr,tx.vout[i].scriptPubKey,taddr,prefix,prefix2);
                if ( strcmp(refburnaddr,destaddr) == 0 )
                {
                    txid = tx.GetHash();
                    nValue = tx.vout[i].nValue;
                    break;
                }
            }
            if ( txid == burntxid )
            {
                LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "verified proof for burntxid in merkleroot" << std::endl);
                return(nValue);
            } else LOGSTREAM("importgateway",CCLOG_INFO, stream << "(" << refburnaddr << ") != (" << destaddr << ") or txid " << txid.GetHex() << " mismatch." << (txid!=burntxid) << " or script mismatch" << std::endl);
        } else LOGSTREAM("importgateway",CCLOG_INFO, stream << "claimaddr." << claimaddr << " != destpubaddr." << destpubaddr << std::endl);
    }    
    return(0);
}

int64_t ImportGatewayDepositval(CTransaction tx,CPubKey mypk)
{
    int32_t numvouts,claimvout,height; int64_t amount; std::string coin,deposithex; std::vector<CPubKey> publishers; std::vector<uint256>txids; uint256 bindtxid,burntxid; std::vector<uint8_t> proof; CPubKey claimpubkey;
    if ( (numvouts= tx.vout.size()) > 0 )
    {
        if ( DecodeImportGatewayDepositOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,publishers,txids,height,burntxid,claimvout,deposithex,proof,claimpubkey,amount) == 'D' && claimpubkey == mypk )
        {
            return(amount);
        }
    }
    return(0);
}

int32_t ImportGatewayBindExists(struct CCcontract_info *cp,CPubKey importgatewaypk,std::string refcoin)
{
    char markeraddr[64],burnaddr[64]; std::string coin; int32_t numvouts; int64_t totalsupply; uint256 tokenid,oracletxid,hashBlock; 
    uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys; CTransaction tx;
    std::vector<uint256> txids;

    _GetCCaddress(markeraddr,EVAL_IMPORTGATEWAY,importgatewaypk);
    SetCCtxids(txids,markeraddr,true,cp->evalcode,zeroid,'B');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        if ( myGetTransaction(*it,tx,hashBlock) != 0 && (numvouts= tx.vout.size()) > 0 && DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)=='B' )
        {
            if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) == 'B' )
            {
                if ( coin == refcoin )
                {
                    LOGSTREAM("importgateway",CCLOG_INFO, stream << "trying to bind an existing import for coin" << std::endl);
                    return(1);
                }
            }
        }
    }
    std::vector<CTransaction> tmp_txs;
    myGet_mempool_txs(tmp_txs,EVAL_IMPORTGATEWAY,'B');
    for (std::vector<CTransaction>::const_iterator it=tmp_txs.begin(); it!=tmp_txs.end(); it++)
    {
        const CTransaction &txmempool = *it;

        if ((numvouts=txmempool.vout.size()) > 0 && DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)=='B')
            if (DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) == 'B')
                return(1);
    }

    return(0);
}

bool ImportGatewayValidate(struct CCcontract_info *cp,Eval *eval,const CTransaction &tx, uint32_t nIn)
{
    int32_t numvins,numvouts,preventCCvins,preventCCvouts,i,numblocks,height,claimvout; bool retval; uint8_t funcid,hash[32],K,M,N,taddr,prefix,prefix2,wiftype;
    char str[65],destaddr[65],burnaddr[65],importgatewayaddr[65],validationError[512];
    std::vector<uint256> txids; std::vector<CPubKey> pubkeys,publishers,tmppublishers; std::vector<uint8_t> proof; int64_t amount,tmpamount;  
    uint256 hashblock,txid,bindtxid,deposittxid,withdrawtxid,completetxid,tmptokenid,oracletxid,bindtokenid,burntxid,tmptxid,merkleroot,mhash; CTransaction bindtx,tmptx;
    std::string refcoin,tmprefcoin,hex,name,description,format; CPubKey pubkey,tmppubkey,importgatewaypk;

    return (true);
    numvins = tx.vin.size();
    numvouts = tx.vout.size();
    preventCCvins = preventCCvouts = -1;
    if ( numvouts < 1 )
        return eval->Invalid("no vouts");
    else
    {
        //LogPrint("importgateway-1","check amounts\n");
        // if ( ImportGatewayExactAmounts(cp,eval,tx,1,10000) == false )
        // {
        //      return eval->Invalid("invalid inputs vs. outputs!");   
        // }
        // else
        // {  
            importgatewaypk = GetUnspendable(cp,0);      
            GetCCaddress(cp, importgatewayaddr, importgatewaypk);              
            if ( (funcid = DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey)) != 0)
            {
                switch ( funcid )
                {
                    case 'B':
                        //vin.0: normal input
                        //vout.0: CC vout marker                        
                        //vout.n-1: opreturn - 'B' coin oracletxid M N pubkeys taddr prefix prefix2 wiftype
                        return eval->Invalid("unexpected ImportGatewayValidate for gatewaysbind!");
                        break;
                    case 'W':
                        //vin.0: normal input
                        //vin.1: CC input of tokens        
                        //vout.0: CC vout marker to gateways CC address                
                        //vout.1: CC vout of gateways tokens back to gateways tokens CC address                  
                        //vout.2: CC vout change of tokens back to owners pubkey (if any)                                                
                        //vout.n-1: opreturn - 'W' bindtxid refcoin withdrawpub amount
                        return eval->Invalid("unexpected ImportGatewayValidate for gatewaysWithdraw!");                 
                        break;
                    case 'P':
                        //vin.0: normal input
                        //vin.1: CC input of marker from previous tx (withdraw or partialsing)
                        //vout.0: CC vout marker to gateways CC address                        
                        //vout.n-1: opreturn - 'P' withdrawtxid refcoin number_of_signs mypk hex
                        if ((numvouts=tx.vout.size()) > 0 && DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,refcoin,K,pubkey,hex)!='P')
                            return eval->Invalid("invalid gatewaysPartialSign OP_RETURN data!");
                        else if (myGetTransaction(withdrawtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid withdraw txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,tmprefcoin,pubkey,amount)!='W')
                            return eval->Invalid("invalid gatewayswithdraw OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");                        
                        else if ( IsCCInput(tmptx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for gatewaysWithdraw!");
                        else if ( IsCCInput(tmptx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE)==0)
                            return eval->Invalid("invalid marker vout for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[1],1,importgatewayaddr,amount)==0)
                            return eval->Invalid("invalid tokens to gateways vout for gatewaysWithdraw!");
                        else if (tmptx.vout[1].nValue!=amount)
                            return eval->Invalid("amount in opret not matching tx tokens amount!");
                        else if (komodo_txnotarizedconfirmed(withdrawtxid) == false)
                            return eval->Invalid("gatewayswithdraw tx is not yet confirmed(notarised)!");
                        else if (myGetTransaction(bindtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid gatewaysbind txid!");
                        else if ((numvouts=tmptx.vout.size()) < 1 || DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,tmprefcoin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B')
                            return eval->Invalid("invalid gatewaysbind OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");
                        else if (komodo_txnotarizedconfirmed(bindtxid) == false)
                            return eval->Invalid("gatewaysbind tx is not yet confirmed(notarised)!");
                        else if (IsCCInput(tx.vin[0].scriptSig) != 0)
                            return eval->Invalid("vin.0 is normal for gatewayspartialsign!");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 0 || myGetTransaction(tx.vin[1].prevout.hash,tmptx,hashblock)==0 || tmptx.vout[tx.vin[1].prevout.n].nValue!=CC_MARKER_VALUE)
                            return eval->Invalid("vin.1 is CC marker for gatewayspartialsign or invalid marker amount!");
                        else if (ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE) == 0 )
                            return eval->Invalid("vout.0 invalid marker for gatewayspartialsign!");
                        else if (K>M)
                            return eval->Invalid("invalid number of signs!"); 
                        break;
                    case 'S':          
                        //vin.0: normal input              
                        //vin.1: CC input of marker from previous tx (withdraw or partialsing)
                        //vout.0: CC vout marker to gateways CC address                       
                        //vout.n-1: opreturn - 'S' withdrawtxid refcoin hex
                        if ((numvouts=tx.vout.size()) > 0 && DecodeImportGatewayCompleteSigningOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,refcoin,K,hex)!='S')
                            return eval->Invalid("invalid gatewayscompletesigning OP_RETURN data!"); 
                        else if (myGetTransaction(withdrawtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid withdraw txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,tmprefcoin,pubkey,amount)!='W')
                            return eval->Invalid("invalid gatewayswithdraw OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");                        
                        else if ( IsCCInput(tmptx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for gatewaysWithdraw!");
                        else if ( IsCCInput(tmptx.vin[1].scriptSig) == 0 )
                            return eval->Invalid("vin.1 is CC for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE)==0)
                            return eval->Invalid("invalid marker vout for gatewaysWithdraw!");
                        else if ( ConstrainVout(tmptx.vout[1],1,importgatewayaddr,amount)==0)
                            return eval->Invalid("invalid tokens to gateways vout for gatewaysWithdraw!");
                        else if (tmptx.vout[1].nValue!=amount)
                            return eval->Invalid("amount in opret not matching tx tokens amount!");
                        else if (komodo_txnotarizedconfirmed(withdrawtxid) == false)
                            return eval->Invalid("gatewayswithdraw tx is not yet confirmed(notarised)!");
                        else if (myGetTransaction(bindtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid gatewaysbind txid!");
                        else if ((numvouts=tmptx.vout.size()) < 1 || DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,tmprefcoin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B')
                            return eval->Invalid("invalid gatewaysbind OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");
                        else if (komodo_txnotarizedconfirmed(bindtxid) == false)
                            return eval->Invalid("gatewaysbind tx is not yet confirmed(notarised)!");
                        else if (IsCCInput(tx.vin[0].scriptSig) != 0)
                            return eval->Invalid("vin.0 is normal for gatewayscompletesigning!");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 0 || myGetTransaction(tx.vin[1].prevout.hash,tmptx,hashblock)==0 || tmptx.vout[tx.vin[1].prevout.n].nValue!=CC_MARKER_VALUE)
                            return eval->Invalid("vin.1 is CC marker for gatewayscompletesigning or invalid marker amount!");
                        else if (ConstrainVout(tx.vout[0],1,cp->unspendableCCaddr,CC_MARKER_VALUE) == 0 )
                            return eval->Invalid("vout.0 invalid marker for gatewayscompletesigning!");
                        else if (K<M)
                            return eval->Invalid("invalid number of signs!");
                        break;                    
                    case 'M':                        
                        //vin.0: normal input
                        //vin.1: CC input of gatewayscompletesigning tx marker to gateways CC address                                               
                        //vout.0: opreturn - 'M' withdrawtxid refcoin completetxid   
                        if ((numvouts=tx.vout.size()) > 0 && DecodeImportGatewayMarkDoneOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,refcoin,completetxid)!='M')
                            return eval->Invalid("invalid gatewaysmarkdone OP_RETURN data!");
                        else if (myGetTransaction(completetxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid gatewayscompletesigning txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayCompleteSigningOpRet(tmptx.vout[numvouts-1].scriptPubKey,withdrawtxid,tmprefcoin,K,hex)!='S')
                            return eval->Invalid("invalid gatewayscompletesigning OP_RETURN data!"); 
                        else if (komodo_txnotarizedconfirmed(completetxid) == false)
                            return eval->Invalid("gatewayscompletesigning tx is not yet confirmed(notarised)!");
                        else if (myGetTransaction(withdrawtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid withdraw txid!");
                        else if ((numvouts=tmptx.vout.size()) > 0 && DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,tmprefcoin,pubkey,amount)!='W')
                            return eval->Invalid("invalid gatewayswithdraw OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");
                        else if (komodo_txnotarizedconfirmed(withdrawtxid) == false)
                            return eval->Invalid("gatewayswithdraw tx is not yet confirmed(notarised)!");
                        else if (myGetTransaction(bindtxid,tmptx,hashblock) == 0)
                            return eval->Invalid("invalid gatewaysbind txid!");
                        else if ((numvouts=tmptx.vout.size()) < 1 || DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,tmprefcoin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B')
                            return eval->Invalid("invalid gatewaysbind OP_RETURN data!"); 
                        else if (tmprefcoin!=refcoin)
                            return eval->Invalid("refcoin different than in bind tx");
                        else if (komodo_txnotarizedconfirmed(bindtxid) == false)
                            return eval->Invalid("gatewaysbind tx is not yet confirmed(notarised)!");
                        else if ( IsCCInput(tx.vin[0].scriptSig) != 0 )
                            return eval->Invalid("vin.0 is normal for gatewaysmarkdone!");
                        else if ((*cp->ismyvin)(tx.vin[1].scriptSig) == 0 || myGetTransaction(tx.vin[1].prevout.hash,tmptx,hashblock)==0 || tmptx.vout[tx.vin[1].prevout.n].nValue!=CC_MARKER_VALUE)
                            return eval->Invalid("vin.1 is CC marker for gatewaysmarkdone or invalid marker amount!");
                        else if (K<M)
                            return eval->Invalid("invalid number of signs!");
                        break;                    
                }
            }
            retval = PreventCC(eval,tx,preventCCvins,numvins,preventCCvouts,numvouts);
            if ( retval != 0 )
                LOGSTREAM("importgateway",CCLOG_INFO, stream << "ImportGateway tx validated" << std::endl);
            else fprintf(stderr,"ImportGateway tx invalid\n");
            return(retval);
        // }
    }
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

std::string ImportGatewayBind(uint64_t txfee,std::string coin,uint256 oracletxid,uint8_t M,uint8_t N,std::vector<CPubKey> pubkeys,uint8_t p1,uint8_t p2,uint8_t p3,uint8_t p4)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction oracletx; uint8_t taddr,prefix,prefix2,wiftype; CPubKey mypk,importgatewaypk; CScript opret; uint256 hashBlock;
    struct CCcontract_info *cp,*cpTokens,C,CTokens; std::string name,description,format; int32_t i,numvouts;
    char destaddr[64],coinaddr[64],myTokenCCaddr[64],str[65],*fstr;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    cpTokens = CCinit(&CTokens,EVAL_TOKENS);
    if (coin=="KMD")
    {
        prefix = KMD_PUBTYPE;
        prefix2 = KMD_P2SHTYPE;
        wiftype = KMD_WIFTYPE;
        taddr = KMD_TADDR;
    }
    else
    {
        prefix = p1;
        prefix2 = p2;
        wiftype = p3;
        taddr = p4;
        LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "set prefix " << prefix << ", prefix2 " << prefix2 << ", wiftype " << wiftype << ", taddr " << taddr << " for " << coin << std::endl);
    }
    if ( N == 0 || N > 15 || M > N )
    {
        CCerror = strprintf("illegal M.%d or N.%d",M,N);
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( pubkeys.size() != N )
    {
        CCerror = strprintf("M.%d N.%d but pubkeys[%d]",M,N,(int32_t)pubkeys.size());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    for (i=0; i<N; i++)
    {
        Getscriptaddress(coinaddr,CScript() << ParseHex(HexStr(pubkeys[i])) << OP_CHECKSIG);
        if ( CCaddress_balance(coinaddr,0) == 0 )
        {
            CCerror = strprintf("M.%d N.%d but pubkeys[%d] has no balance",M,N,i);
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
    }
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    _GetCCaddress(myTokenCCaddr,EVAL_TOKENS,mypk);
    importgatewaypk = GetUnspendable(cp,0);
    if ( _GetCCaddress(destaddr,EVAL_IMPORTGATEWAY,importgatewaypk) == 0 )
    {
        CCerror = strprintf("ImportGateway bind.%s can't create globaladdr",coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( myGetTransaction(oracletxid,oracletx,hashBlock) == 0 || (numvouts= oracletx.vout.size()) <= 0 )
    {
        CCerror = strprintf("cant find oracletxid %s",uint256_str(str,oracletxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeOraclesCreateOpRet(oracletx.vout[numvouts-1].scriptPubKey,name,description,format) != 'C' )
    {
        CCerror = strprintf("mismatched oracle name %s != %s",name.c_str(),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( (fstr=(char *)format.c_str()) == 0 || strncmp(fstr,"Ihh",3) != 0 )
    {
        CCerror = strprintf("illegal format (%s) != (%s)",fstr,(char *)"Ihh");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( ImportGatewayBindExists(cp,importgatewaypk,coin) != 0 )
    {
        CCerror = strprintf("Gateway bind.%s already exists",coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( AddNormalinputs(mtx,mypk,txfee+CC_MARKER_VALUE,2) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(cp->evalcode,CC_MARKER_VALUE,importgatewaypk));
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeImportGatewayBindOpRet('B',coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype)));
    }
    CCerror = strprintf("cant find enough inputs");
    LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
    return("");
}

std::string ImportGatewayDeposit(uint64_t txfee,uint256 bindtxid,int32_t height,std::string refcoin,uint256 burntxid,int32_t claimvout,std::string rawburntx,std::vector<uint8_t>proof,CPubKey destpub,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight()), burntx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction bindtx; CPubKey mypk; uint256 oracletxid,merkleroot,mhash,hashBlock,txid; std::vector<CTxOut> vouts;
    int32_t i,m,n,numvouts; uint8_t M,N,taddr,prefix,prefix2,wiftype; std::string coin; struct CCcontract_info *cp,C;
    std::vector<CPubKey> pubkeys,publishers; std::vector<uint256> txids; char str[128],burnaddr[64];

    if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
    {
        CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    if (!E_UNMARSHAL(ParseHex(rawburntx), ss >> burntx))
    {
        return std::string("");
    }
    LOGSTREAM("importgateway",CCLOG_DEBUG1, stream << "ImportGatewayDeposit ht." << height << " " << refcoin << " " << (double)amount/COIN << " numpks." << (int32_t)pubkeys.size() << std::endl);
    if ( myGetTransaction(bindtxid,bindtx,hashBlock) == 0 || (numvouts= bindtx.vout.size()) <= 0 )
    {
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,bindtx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B' || refcoin != coin )
    {
        CCerror = strprintf("invalid coin - bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (komodo_txnotarizedconfirmed(bindtxid)==false)
    {
        CCerror = strprintf("gatewaysbind tx not yet confirmed/notarized");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    n = (int32_t)pubkeys.size();
    merkleroot = zeroid;
    for (i=m=0; i<n; i++)
    {
        pubkey33_str(str,(uint8_t *)&pubkeys[i]);
        LOGSTREAM("importgateway",CCLOG_INFO, stream << "pubkeys[" << i << "] " << str << std::endl);
        if ( (mhash= CCOraclesReverseScan("importgateway-2",txid,height,oracletxid,OraclesBatontxid(oracletxid,pubkeys[i]))) != zeroid )
        {
            if ( merkleroot == zeroid )
                merkleroot = mhash, m = 1;
            else if ( mhash == merkleroot )
                m++;
            publishers.push_back(pubkeys[i]);
            txids.push_back(txid);
        }
    }
    LOGSTREAM("importgateway",CCLOG_INFO, stream << "burntxid." << burntxid.GetHex() << " m." << m << " of n." << n << std::endl);
    if ( merkleroot == zeroid || m < n/2 )
    {
        CCerror = strprintf("couldnt find merkleroot for ht.%d %s oracle.%s m.%d vs n.%d",height,refcoin.c_str(),uint256_str(str,oracletxid),m,n);
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( ImportGatewayVerify(burnaddr,oracletxid,claimvout,refcoin,burntxid,rawburntx,proof,merkleroot,destpub,taddr,prefix,prefix2) != amount )
    {
        CCerror = strprintf("burntxid didnt validate!");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    vouts.push_back(CTxOut(amount,CScript() << ParseHex(HexStr(destpub)) << OP_CHECKSIG));
    burntx.vout.push_back(MakeBurnOutput((CAmount)amount,0xffffffff,refcoin,vouts,proof,bindtxid,publishers,txids,burntxid,height,claimvout,rawburntx,destpub,amount));
    std::vector<uint256> leaftxids;
    BitcoinGetProofMerkleRoot(proof, leaftxids);
    MerkleBranch newBranch(0, leaftxids);
    TxProof txProof = std::make_pair(burntxid, newBranch);    
    return  HexStr(E_MARSHAL(ss << MakeImportCoinTransaction(txProof, burntx, vouts)));
}

std::string ImportGatewayWithdraw(uint64_t txfee,uint256 bindtxid,std::string refcoin,CPubKey withdrawpub,int64_t amount)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CTransaction tx; CPubKey mypk,importgatewaypk,signerpk; uint256 txid,hashBlock,oracletxid,tmptokenid,tmpbindtxid,withdrawtxid; int32_t vout,numvouts;
    int64_t nValue,inputs,CCchange=0,tmpamount; uint8_t funcid,K,M,N,taddr,prefix,prefix2,wiftype; std::string coin,hex;
    std::vector<CPubKey> msigpubkeys; char burnaddr[64],str[65],coinaddr[64]; struct CCcontract_info *cp,C;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
    {
        CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    importgatewaypk = GetUnspendable(cp, 0);

    if( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B' || coin != refcoin )
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (komodo_txnotarizedconfirmed(bindtxid)==false)
    {
        CCerror = strprintf("gatewaysbind tx not yet confirmed/notarized");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    _GetCCaddress(coinaddr,EVAL_IMPORTGATEWAY,importgatewaypk);
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        K=0;
        if ( vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size())>0 &&
            (funcid=DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey))!=0 && (funcid=='W' || funcid=='P'))
        {
            if (funcid=='W' && DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,tmpbindtxid,coin,withdrawpub,tmpamount)=='W'
                && refcoin==coin && tmpbindtxid==bindtxid)
            {
                CCerror = strprintf("unable to create withdraw, another withdraw pending");
                LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
                return("");
            }
            
            else if (funcid=='P' && DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,signerpk,hex)=='P' &&
                myGetTransaction(withdrawtxid,tx,hashBlock)!=0 && (numvouts=tx.vout.size())>0 && DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,tmpbindtxid,coin,withdrawpub,tmpamount)=='W'
                && refcoin==coin && tmpbindtxid==bindtxid)                    
            {
                CCerror = strprintf("unable to create withdraw, another withdraw pending");
                LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
                return("");
            }
        }
    }
    if( AddNormalinputs(mtx, mypk, txfee+CC_MARKER_VALUE+amount, 64) > 0 )
    {
        mtx.vout.push_back(MakeCC1vout(EVAL_IMPORTGATEWAY,CC_MARKER_VALUE,importgatewaypk));
        mtx.vout.push_back(MakeCC1vout(EVAL_IMPORTGATEWAY,amount,CCtxidaddr(str,bindtxid)));                   
        return(FinalizeCCTx(0, cp, mtx, mypk, txfee,EncodeImportGatewayWithdrawOpRet('W',bindtxid,refcoin,withdrawpub,amount)));
    }
    CCerror = strprintf("cant find enough normal inputs");
    LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
    return("");
}

std::string ImportGatewayPartialSign(uint64_t txfee,uint256 lasttxid,std::string refcoin, std::string hex)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,withdrawpub,signerpk,importgatewaypk; struct CCcontract_info *cp,C; CTransaction tx,tmptx;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs; char funcid,str[65],burnaddr[64];
    int32_t numvouts; uint256 withdrawtxid,hashBlock,bindtxid,tokenid,oracletxid; std::string coin,tmphex; int64_t amount;
    uint8_t K=0,M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( txfee == 0 )
        txfee = 10000;
    mypk = pubkey2pk(Mypubkey());
    importgatewaypk = GetUnspendable(cp,0);
    if (myGetTransaction(lasttxid,tx,hashBlock)==0 || (numvouts= tx.vout.size())<=0
        || (funcid=DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey))==0 || (funcid!='W' && funcid!='P'))
    {
        CCerror = strprintf("can't find last tx %s",uint256_str(str,lasttxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (funcid=='W')
    {
        withdrawtxid=lasttxid;
        if (DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount)!='W' || refcoin!=coin)
        {
            CCerror = strprintf("invalid withdraw tx %s",uint256_str(str,lasttxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
        {
            CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (komodo_txnotarizedconfirmed(withdrawtxid)==false)
        {
            CCerror = strprintf("gatewayswithdraw tx not yet confirmed/notarized");
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(bindtxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B'
            || refcoin!=coin)
        {
            CCerror = strprintf("invalid bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
    }
    else if (funcid=='P')
    {
        if (DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,signerpk,tmphex)!='P'  || refcoin!=coin)
        {
            CCerror = strprintf("cannot decode partialsign tx opret %s",uint256_str(str,lasttxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(withdrawtxid,tmptx,hashBlock)==0 || (numvouts= tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find withdraw tx %s",uint256_str(str,withdrawtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount)!='W'
            || refcoin!=coin)
        {
            CCerror = strprintf("invalid withdraw tx %s",uint256_str(str,lasttxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
        {
            CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (komodo_txnotarizedconfirmed(withdrawtxid)==false)
        {
            CCerror = strprintf("gatewayswithdraw tx not yet confirmed/notarized");
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(bindtxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B'
            || refcoin!=coin)
        {
            CCerror = strprintf("invalid bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
    }
    if (AddNormalinputs(mtx,mypk,txfee,3)!=0)
    {
        mtx.vin.push_back(CTxIn(tx.GetHash(),0,CScript()));
        mtx.vout.push_back(MakeCC1vout(EVAL_IMPORTGATEWAY,CC_MARKER_VALUE,importgatewaypk));       
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeImportGatewayPartialOpRet('P',withdrawtxid,refcoin,K+1,mypk,hex)));
    }
    CCerror = strprintf("error adding funds for partialsign");    
    LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
    return("");
}

std::string ImportGatewayCompleteSigning(uint64_t txfee,uint256 lasttxid,std::string refcoin,std::string hex)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk,importgatewaypk,signerpk,withdrawpub; struct CCcontract_info *cp,C; char funcid,str[65],burnaddr[64]; int64_t amount;
    std::string coin,tmphex; CTransaction tx,tmptx; uint256 withdrawtxid,hashBlock,tokenid,bindtxid,oracletxid; int32_t numvouts;
    uint8_t K=0,M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    mypk = pubkey2pk(Mypubkey());
    importgatewaypk = GetUnspendable(cp,0);
    if ( txfee == 0 )
        txfee = 10000;
    if (myGetTransaction(lasttxid,tx,hashBlock)==0 || (numvouts= tx.vout.size())<=0
        || (funcid=DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey))==0 || (funcid!='W' && funcid!='P'))
    {
        CCerror = strprintf("invalid last txid %s",uint256_str(str,lasttxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (funcid=='W')
    {
        withdrawtxid=lasttxid;
        if (DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount)!='W' || refcoin!=coin)
        {
            CCerror = strprintf("cannot decode withdraw tx opret %s",uint256_str(str,lasttxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(bindtxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
        {
            CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (komodo_txnotarizedconfirmed(withdrawtxid)==false)
        {
            CCerror = strprintf("gatewayswithdraw tx not yet confirmed/notarized");
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (GetTransaction(bindtxid,tmptx,hashBlock,false)==0 || (numvouts=tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B'
            || refcoin!=coin)
        {
            CCerror = strprintf("invalid bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
    }
    else if (funcid=='P')
    {
        if (DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,signerpk,tmphex)!='P' || refcoin!=coin)
        {
            CCerror = strprintf("cannot decode partialsign tx opret %s",uint256_str(str,lasttxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(withdrawtxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())==0)
        {
            CCerror = strprintf("invalid withdraw txid %s",uint256_str(str,withdrawtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayWithdrawOpRet(tmptx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount)!='W' || refcoin!=coin)
        {
            CCerror = strprintf("cannot decode withdraw tx opret %s",uint256_str(str,withdrawtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
        {
            CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (komodo_txnotarizedconfirmed(withdrawtxid)==false)
        {
            CCerror = strprintf("gatewayswithdraw tx not yet confirmed/notarized");
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (myGetTransaction(bindtxid,tmptx,hashBlock)==0 || (numvouts=tmptx.vout.size())<=0)
        {
            CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
        else if (DecodeImportGatewayBindOpRet(burnaddr,tmptx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B'
            || refcoin!=coin)
        {
            CCerror = strprintf("invalid bind tx %s",uint256_str(str,bindtxid));
            LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
            return("");
        }
    }
    if (AddNormalinputs(mtx,mypk,txfee,3)!=0) 
    {
        mtx.vin.push_back(CTxIn(lasttxid,0,CScript()));
        mtx.vout.push_back(MakeCC1vout(EVAL_IMPORTGATEWAY,CC_MARKER_VALUE,importgatewaypk));       
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeImportGatewayCompleteSigningOpRet('S',withdrawtxid,refcoin,K+1,hex)));
    }
    CCerror = strprintf("error adding funds for completesigning");
    LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
    return("");
}

std::string ImportGatewayMarkDone(uint64_t txfee,uint256 completetxid,std::string refcoin)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    CPubKey mypk; struct CCcontract_info *cp,C; char str[65],burnaddr[64]; CTransaction tx; int32_t numvouts;
    uint256 withdrawtxid,bindtxid,oracletxid,tokenid,hashBlock; std::string coin,hex;
    uint8_t K,M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys; int64_t amount; CPubKey withdrawpub;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    mypk = pubkey2pk(Mypubkey());    
    if ( txfee == 0 )
        txfee = 10000;
    if (myGetTransaction(completetxid,tx,hashBlock)==0 || (numvouts= tx.vout.size())<=0)
    {
        CCerror = strprintf("invalid completesigning txid %s",uint256_str(str,completetxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (DecodeImportGatewayCompleteSigningOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,hex)!='S' || refcoin!=coin)
    {
        CCerror = strprintf("cannot decode completesigning tx opret %s",uint256_str(str,completetxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (komodo_txnotarizedconfirmed(completetxid)==false)
    {
        CCerror = strprintf("gatewayscompletesigning tx not yet confirmed/notarized");
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (myGetTransaction(withdrawtxid,tx,hashBlock)==0 || (numvouts= tx.vout.size())==0)
    {
        CCerror = strprintf("invalid withdraw txid %s",uint256_str(str,withdrawtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount)!='W' || refcoin!=coin)
    {
        CCerror = strprintf("cannot decode withdraw tx opret %s\n",uint256_str(str,withdrawtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (KOMODO_EARLYTXID!=zeroid && bindtxid!=KOMODO_EARLYTXID)
    {
        CCerror = strprintf("invalid import gateway. On this chain only valid import gateway is %s",KOMODO_EARLYTXID.GetHex());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (myGetTransaction(bindtxid,tx,hashBlock)==0 || (numvouts=tx.vout.size())<=0)
    {
        CCerror = strprintf("can't find bind tx %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    else if (DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 'B'
        || refcoin!=coin)
    {
        CCerror = strprintf("invalid bind tx %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if (AddNormalinputs(mtx,mypk,txfee,3)!=0) 
    {
        mtx.vin.push_back(CTxIn(completetxid,0,CScript()));
        mtx.vout.push_back(CTxOut(CC_MARKER_VALUE,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));        
        return(FinalizeCCTx(0,cp,mtx,mypk,txfee,EncodeImportGatewayMarkDoneOpRet('M',withdrawtxid,refcoin,completetxid)));
    }
    CCerror = strprintf("error adding funds for markdone");
    LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
    return("");
}

UniValue ImportGatewayPendingWithdraws(uint256 bindtxid,std::string refcoin)
{
    UniValue result(UniValue::VOBJ),pending(UniValue::VARR); CTransaction tx; std::string coin,hex; CPubKey mypk,importgatewaypk,withdrawpub,signerpk;
    std::vector<CPubKey> msigpubkeys; uint256 hashBlock,txid,tmpbindtxid,tmptokenid,oracletxid,withdrawtxid; uint8_t K,M,N,taddr,prefix,prefix2,wiftype;
    char funcid,burnaddr[65],coinaddr[65],destaddr[65],str[65],withaddr[65],numstr[32],signeraddr[65],txidaddr[65];
    int32_t i,n,numvouts,vout,queueflag; int64_t amount,nValue; struct CCcontract_info *cp,C;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    mypk = pubkey2pk(Mypubkey());
    importgatewaypk = GetUnspendable(cp,0);
    _GetCCaddress(coinaddr,EVAL_IMPORTGATEWAY,importgatewaypk);
    if ( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B' || refcoin != coin )
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    n = msigpubkeys.size();
    queueflag = 0;
    for (i=0; i<n; i++)
        if ( msigpubkeys[i] == mypk )
        {
            queueflag = 1;
            break;
        }    
    SetCCunspents(unspentOutputs,coinaddr,true);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        K=0;
        if ( vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size())>0 &&
            (funcid=DecodeImportGatewayOpRet(tx.vout[numvouts-1].scriptPubKey))!=0 && (funcid=='W' || funcid=='P') && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0)
        {
            if (funcid=='W')
            {
                if (DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,tmpbindtxid,coin,withdrawpub,amount)==0 || refcoin!=coin || tmpbindtxid!=bindtxid) continue;
            }
            else if (funcid=='P')
            {
                if (DecodeImportGatewayPartialOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,signerpk,hex)!='P' || myGetTransaction(withdrawtxid,tx,hashBlock)==0
                    || (numvouts=tx.vout.size())<=0 || DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,tmpbindtxid,coin,withdrawpub,amount)!='W'
                    || refcoin!=coin || tmpbindtxid!=bindtxid)
                    continue;                    
            }      
            Getscriptaddress(destaddr,tx.vout[1].scriptPubKey);
            GetCustomscriptaddress(withaddr,CScript() << ParseHex(HexStr(withdrawpub)) << OP_CHECKSIG,taddr,prefix,prefix2);
            if ( strcmp(destaddr,coinaddr) == 0 )
            {
                UniValue obj(UniValue::VOBJ);
                obj.push_back(Pair("withdrawtxid",uint256_str(str,tx.GetHash())));
                CCCustomtxidaddr(txidaddr,tx.GetHash(),taddr,prefix,prefix2);
                obj.push_back(Pair("withdrawtxidaddr",txidaddr));
                obj.push_back(Pair("withdrawaddr",withaddr));
                sprintf(numstr,"%.8f",(double)tx.vout[1].nValue/COIN);
                obj.push_back(Pair("amount",numstr));                
                obj.push_back(Pair("confirmed_or_notarized",komodo_txnotarizedconfirmed(tx.GetHash())));
                if ( queueflag != 0 )
                {
                    obj.push_back(Pair("depositaddr",burnaddr));
                    GetCustomscriptaddress(signeraddr,CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG,taddr,prefix,prefix2);
                    obj.push_back(Pair("signeraddr",signeraddr));
                }
                if (N>1)
                {
                    obj.push_back(Pair("number_of_signs",K));
                    obj.push_back(Pair("last_txid",uint256_str(str,txid)));
                    if (K>0) obj.push_back(Pair("hex",hex));
                }
                pending.push_back(obj);
            }
        }
    }
    result.push_back(Pair("coin",refcoin));
    result.push_back(Pair("pending",pending));
    result.push_back(Pair("queueflag",queueflag));
    return(result);
}

UniValue ImportGatewayProcessedWithdraws(uint256 bindtxid,std::string refcoin)
{
    UniValue result(UniValue::VOBJ),processed(UniValue::VARR); CTransaction tx; std::string coin,hex; 
    CPubKey mypk,importgatewaypk,withdrawpub; std::vector<CPubKey> msigpubkeys;
    uint256 withdrawtxid,hashBlock,txid,tmptokenid,oracletxid; uint8_t K,M,N,taddr,prefix,prefix2,wiftype;
    char burnaddr[65],coinaddr[65],str[65],numstr[32],withaddr[65],txidaddr[65];
    int32_t i,n,numvouts,vout,queueflag; int64_t nValue,amount; struct CCcontract_info *cp,C;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    mypk = pubkey2pk(Mypubkey());
    importgatewaypk = GetUnspendable(cp,0);
    _GetCCaddress(coinaddr,EVAL_IMPORTGATEWAY,importgatewaypk);
    if ( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {        
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B' || refcoin != coin)
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    n = msigpubkeys.size();
    queueflag = 0;
    for (i=0; i<n; i++)
        if ( msigpubkeys[i] == mypk )
        {
            queueflag = 1;
            break;
        }    
    SetCCunspents(unspentOutputs,coinaddr,true);    
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        nValue = (int64_t)it->second.satoshis;
        if ( vout == 0 && nValue == CC_MARKER_VALUE && myGetTransaction(txid,tx,hashBlock) != 0 && (numvouts= tx.vout.size())>0 &&
            DecodeImportGatewayCompleteSigningOpRet(tx.vout[numvouts-1].scriptPubKey,withdrawtxid,coin,K,hex) == 'S' && refcoin == coin && myIsutxo_spentinmempool(ignoretxid,ignorevin,txid,vout) == 0)
        {   
            if (myGetTransaction(withdrawtxid,tx,hashBlock) != 0 && (numvouts= tx.vout.size())>0
                && DecodeImportGatewayWithdrawOpRet(tx.vout[numvouts-1].scriptPubKey,bindtxid,coin,withdrawpub,amount) == 'W' || refcoin!=coin)          
            {
                UniValue obj(UniValue::VOBJ);
                obj.push_back(Pair("completesigningtxid",uint256_str(str,txid)));
                obj.push_back(Pair("withdrawtxid",uint256_str(str,withdrawtxid)));  
                CCCustomtxidaddr(txidaddr,withdrawtxid,taddr,prefix,prefix2);
                obj.push_back(Pair("withdrawtxidaddr",txidaddr));              
                GetCustomscriptaddress(withaddr,CScript() << ParseHex(HexStr(withdrawpub)) << OP_CHECKSIG,taddr,prefix,prefix2);
                obj.push_back(Pair("withdrawaddr",withaddr));
                obj.push_back(Pair("confirmed_or_notarized",komodo_txnotarizedconfirmed(txid)));
                sprintf(numstr,"%.8f",(double)tx.vout[1].nValue/COIN);
                obj.push_back(Pair("amount",numstr));
                obj.push_back(Pair("hex",hex));                
                processed.push_back(obj);            
            }
        }
    }
    result.push_back(Pair("coin",refcoin));
    result.push_back(Pair("processed",processed));
    result.push_back(Pair("queueflag",queueflag));
    return(result);
}

UniValue ImportGatewayList()
{
    UniValue result(UniValue::VARR); std::vector<uint256> txids;
    struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid; CTransaction vintx; std::string coin;
    char str[65],burnaddr[64]; uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> pubkeys;
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    SetCCtxids(txids,cp->unspendableCCaddr,true,cp->evalcode,zeroid,'B');
    for (std::vector<uint256>::const_iterator it=txids.begin(); it!=txids.end(); it++)
    {
        txid = *it;
        if ( myGetTransaction(txid,vintx,hashBlock) != 0 )
        {
            if ( vintx.vout.size() > 0 && DecodeImportGatewayBindOpRet(burnaddr,vintx.vout[vintx.vout.size()-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 0 )
            {
                result.push_back(uint256_str(str,txid));
            }
        }
    }
    return(result);
}

UniValue ImportGatewayExternalAddress(uint256 bindtxid,CPubKey pubkey)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid; CTransaction tx;
    std::string coin; int64_t numvouts; char str[65],addr[65],burnaddr[65]; uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> msigpubkeys;
    
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {        
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B')
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    GetCustomscriptaddress(addr,CScript() << ParseHex(HexStr(pubkey)) << OP_CHECKSIG,taddr,prefix,prefix2);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("address",addr));
    return(result);
}

UniValue ImportGatewayDumpPrivKey(uint256 bindtxid,CKey key)
{
    UniValue result(UniValue::VOBJ); struct CCcontract_info *cp,C; uint256 txid,hashBlock,oracletxid; CTransaction tx;
    std::string coin,priv; int64_t numvouts; char str[65],addr[65],burnaddr[65]; uint8_t M,N,taddr,prefix,prefix2,wiftype; std::vector<CPubKey> msigpubkeys;
    
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    if ( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {        
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B')
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }

    priv=EncodeCustomSecret(key,wiftype);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("privkey",priv.c_str()));
    return(result);
}

UniValue ImportGatewayInfo(uint256 bindtxid)
{
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), komodo_nextheight());
    UniValue result(UniValue::VOBJ),a(UniValue::VARR); std::string coin; char str[67],numstr[65],burnaddr[64],gatewaystokens[64];
    uint8_t M,N; std::vector<CPubKey> pubkeys; uint8_t taddr,prefix,prefix2,wiftype; uint256 oracletxid,hashBlock; CTransaction tx;
    CPubKey ImportGatewaypk; struct CCcontract_info *cp,C; int32_t i; int64_t numvouts,remaining; std::vector<CPubKey> msigpubkeys;
  
    cp = CCinit(&C,EVAL_IMPORTGATEWAY);
    ImportGatewaypk = GetUnspendable(cp,0);
    GetTokensCCaddress(cp,gatewaystokens,ImportGatewaypk);
    if ( myGetTransaction(bindtxid,tx,hashBlock) == 0 || (numvouts= tx.vout.size()) <= 0 )
    {        
        CCerror = strprintf("cant find bindtxid %s",uint256_str(str,bindtxid));
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( DecodeImportGatewayBindOpRet(burnaddr,tx.vout[numvouts-1].scriptPubKey,coin,oracletxid,M,N,msigpubkeys,taddr,prefix,prefix2,wiftype) != 'B')
    {
        CCerror = strprintf("invalid bindtxid %s coin.%s",uint256_str(str,bindtxid),coin.c_str());
        LOGSTREAM("importgateway",CCLOG_INFO, stream << CCerror << std::endl);
        return("");
    }
    if ( myGetTransaction(bindtxid,tx,hashBlock) != 0 )
    {
        result.push_back(Pair("result","success"));
        result.push_back(Pair("name","ImportGateway"));
        burnaddr[0] = 0;
        if ( tx.vout.size() > 0 && DecodeImportGatewayBindOpRet(burnaddr,tx.vout[tx.vout.size()-1].scriptPubKey,coin,oracletxid,M,N,pubkeys,taddr,prefix,prefix2,wiftype) != 0 && M <= N && N > 0 )
        {
            result.push_back(Pair("M",M));
            result.push_back(Pair("N",N));
            for (i=0; i<N; i++)
                a.push_back(pubkey33_str(str,(uint8_t *)&pubkeys[i]));
            result.push_back(Pair("pubkeys",a));
            result.push_back(Pair("coin",coin));
            result.push_back(Pair("oracletxid",uint256_str(str,oracletxid)));
            result.push_back(Pair("taddr",taddr));
            result.push_back(Pair("prefix",prefix));
            result.push_back(Pair("prefix2",prefix2));
            result.push_back(Pair("wiftype",wiftype));
            result.push_back(Pair("deposit",burnaddr));
        }
    }
    return(result);
}
