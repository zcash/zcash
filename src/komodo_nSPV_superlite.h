
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

#ifndef KOMODO_NSPVSUPERLITE_H
#define KOMODO_NSPVSUPERLITE_H

// nSPV client. VERY simplistic "single threaded" networking model. for production GUI best to multithread, etc.
// no caching, no optimizations, no reducing the number of ntzsproofs needed by detecting overlaps, etc.
// advantage is that it is simpler to implement and understand to create a design for a more performant version


CAmount AmountFromValue(const UniValue& value);
int32_t bitcoin_base58decode(uint8_t *data,char *coinaddr);

uint32_t NSPV_lastinfo,NSPV_logintime;
CKey NSPV_key;
char NSPV_wifstr[64],NSPV_pubkeystr[67];
std::string NSPV_address;
struct NSPV_inforesp NSPV_inforesult;
struct NSPV_utxosresp NSPV_utxosresult;
struct NSPV_spentinfo NSPV_spentresult;
struct NSPV_ntzsresp NSPV_ntzsresult;
struct NSPV_ntzsproofresp NSPV_ntzsproofresult;
struct NSPV_txproof NSPV_txproofresult;
struct NSPV_broadcastresp NSPV_broadcastresult;


// komodo_nSPVresp is called from async message processing

void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> response) // received a response
{
    struct NSPV_inforesp I; int32_t len; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= response.size()) > 0 )
    {
        switch ( response[0] )
        {
            case NSPV_INFORESP:
                I = NSPV_inforesult;
                NSPV_inforesp_purge(&NSPV_inforesult);
                NSPV_rwinforesp(0,&response[1],&NSPV_inforesult);
                if ( NSPV_inforesult.height < I.height )
                {
                    fprintf(stderr,"got old info response %u size.%d height.%d\n",timestamp,(int32_t)response.size(),NSPV_inforesult.height); // update current height and ntrz status
                    NSPV_inforesp_purge(&NSPV_inforesult);
                    NSPV_inforesult = I;
                }
                else if ( NSPV_inforesult.height > I.height )
                {
                    NSPV_lastinfo = 0;
                }
                break;
            case NSPV_UTXOSRESP:
                NSPV_utxosresp_purge(&NSPV_utxosresult);
                NSPV_rwutxosresp(0,&response[1],&NSPV_utxosresult);
                fprintf(stderr,"got utxos response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos list
                break;
            case NSPV_NTZSRESP:
                NSPV_ntzsresp_purge(&NSPV_ntzsresult);
                NSPV_rwntzsresp(0,&response[1],&NSPV_ntzsresult);
                fprintf(stderr,"got ntzs response %u size.%d %s prev.%d, %s next.%d\n",timestamp,(int32_t)response.size(),NSPV_ntzsresult.prevntz.txid.GetHex().c_str(),NSPV_ntzsresult.prevntz.height,NSPV_ntzsresult.nextntz.txid.GetHex().c_str(),NSPV_ntzsresult.nextntz.height);
                break;
            case NSPV_NTZSPROOFRESP:
                NSPV_ntzsproofresp_purge(&NSPV_ntzsproofresult);
                NSPV_rwntzsproofresp(0,&response[1],&NSPV_ntzsproofresult);
                fprintf(stderr,"got ntzproof response %u size.%d prev.%d next.%d\n",timestamp,(int32_t)response.size(),NSPV_ntzsproofresult.common.prevht,NSPV_ntzsproofresult.common.nextht);
                break;
            case NSPV_TXPROOFRESP:
                NSPV_txproof_purge(&NSPV_txproofresult);
                NSPV_rwtxproof(0,&response[1],&NSPV_txproofresult);
                fprintf(stderr,"got txproof response %u size.%d %s ht.%d\n",timestamp,(int32_t)response.size(),NSPV_txproofresult.txid.GetHex().c_str(),NSPV_txproofresult.height);
                break;
            case NSPV_SPENTINFORESP:
                NSPV_spentinfo_purge(&NSPV_spentresult);
                NSPV_rwspentinfo(0,&response[1],&NSPV_spentresult);
                fprintf(stderr,"got spentinfo response %u size.%d\n",timestamp,(int32_t)response.size());
                break;
            case NSPV_BROADCASTRESP:
                NSPV_broadcast_purge(&NSPV_broadcastresult);
                NSPV_rwbroadcastresp(0,&response[1],&NSPV_broadcastresult);
                fprintf(stderr,"got broadcast response %u size.%d %s retcode.%d\n",timestamp,(int32_t)response.size(),NSPV_broadcastresult.txid.GetHex().c_str(),NSPV_broadcastresult.retcode);
                break;
            default: fprintf(stderr,"unexpected response %02x size.%d at %u\n",response[0],(int32_t)response.size(),timestamp);
                break;
        }
    }
}

// superlite message issuing

CNode *NSPV_req(CNode *pnode,uint8_t *msg,int32_t len,uint64_t mask,int32_t ind)
{
    int32_t n,flag = 0; CNode *pnodes[64]; uint32_t timestamp = (uint32_t)time(NULL);
    if ( pnode == 0 )
    {
        memset(pnodes,0,sizeof(pnodes));
        LOCK(cs_vNodes);
        n = 0;
        BOOST_FOREACH(CNode *ptr,vNodes)
        {
            if ( ptr->prevtimes[ind] > timestamp )
                ptr->prevtimes[ind] = 0;
            if ( ptr->hSocket == INVALID_SOCKET )
                continue;
            if ( (ptr->nServices & mask) == mask && timestamp > ptr->prevtimes[ind] )
            {
                flag = 1;
                pnodes[n++] = ptr;
                if ( n == sizeof(pnodes)/sizeof(*pnodes) )
                    break;
            } // else fprintf(stderr,"nServices %llx vs mask %llx, t%u vs %u, ind.%d\n",(long long)ptr->nServices,(long long)mask,timestamp,ptr->prevtimes[ind],ind);
        }
        if ( n > 0 )
            pnode = pnodes[rand() % n];
    } else flag = 1;
    if ( pnode != 0 )
    {
        std::vector<uint8_t> request;
        request.resize(len);
        memcpy(&request[0],msg,len);
        //fprintf(stderr,"pushmessage [%d] len.%d\n",msg[0],len);
        pnode->PushMessage("getnSPV",request);
        pnode->prevtimes[ind] = timestamp;
        return(pnode);
    } else fprintf(stderr,"no pnodes\n");
    return(0);
}

UniValue NSPV_logout()
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    if ( NSPV_logintime != 0 )
        fprintf(stderr,"scrub wif and privkey from NSPV memory\n");
    else result.push_back(Pair("status","wasnt logged in"));
    memset(NSPV_wifstr,0,sizeof(NSPV_wifstr));
    memset(&NSPV_key,0,sizeof(NSPV_key));
    NSPV_logintime = 0;
    return(result);
}

// komodo_nSPV from main polling loop (really this belongs in its own file, but it is so small, it ended up here)

void komodo_nSPV(CNode *pto) // polling loop from SendMessages
{
    uint8_t msg[256]; int32_t i,len=0; uint32_t timestamp = (uint32_t)time(NULL);
    if ( NSPV_logintime != 0 && timestamp > NSPV_logintime+NSPV_AUTOLOGOUT )
        NSPV_logout();
    if ( (pto->nServices & NODE_NSPV) == 0 )
        return;
    if ( KOMODO_NSPV != 0 )
    {
        if ( timestamp > NSPV_lastinfo + ASSETCHAINS_BLOCKTIME/2 && timestamp > pto->prevtimes[NSPV_INFO>>1] + 2*ASSETCHAINS_BLOCKTIME/3 )
        {
            int32_t reqht;
            reqht = 0;
            len = 0;
            msg[len++] = NSPV_INFO;
            len += iguana_rwnum(1,&msg[len],sizeof(reqht),&reqht);
            fprintf(stderr,"issue getinfo\n");
            NSPV_req(pto,msg,len,NODE_NSPV,NSPV_INFO>>1);
        }
    }
}

UniValue NSPV_txproof_json(struct NSPV_txproof *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("txid",ptr->txid.GetHex()));
    result.push_back(Pair("height",(int64_t)ptr->height));
    result.push_back(Pair("txlen",(int64_t)ptr->txlen));
    result.push_back(Pair("txprooflen",(int64_t)ptr->txprooflen));
    return(result);
}

UniValue NSPV_spentinfo_json(struct NSPV_spentinfo *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("txid",ptr->txid.GetHex()));
    result.push_back(Pair("vout",(int64_t)ptr->vout));
    result.push_back(Pair("spentheight",(int64_t)ptr->spent.height));
    result.push_back(Pair("spenttxid",ptr->spent.txid.GetHex()));
    result.push_back(Pair("spentvini",(int64_t)ptr->spentvini));
    result.push_back(Pair("spenttxlen",(int64_t)ptr->spent.txlen));
    result.push_back(Pair("spenttxprooflen",(int64_t)ptr->spent.txprooflen));
    return(result);
}

UniValue NSPV_ntz_json(struct NSPV_ntz *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("notarized_height",(int64_t)ptr->height));
    result.push_back(Pair("notarized_blockhash",ptr->blockhash.GetHex()));
    result.push_back(Pair("notarization_txid",ptr->txid.GetHex()));
    result.push_back(Pair("notarization_txidheight",(int64_t)ptr->txidheight));
    result.push_back(Pair("notarization_desttxid",ptr->othertxid.GetHex()));
    return(result);
}

UniValue NSPV_header_json(struct NSPV_equihdr *hdr,int32_t height)
{
    UniValue item(UniValue::VOBJ);
    item.push_back(Pair("height",(int64_t)height));
    item.push_back(Pair("blockhash",NSPV_hdrhash(hdr).GetHex()));
    item.push_back(Pair("hashPrevBlock",hdr->hashPrevBlock.GetHex()));
    item.push_back(Pair("hashMerkleRoot",hdr->hashMerkleRoot.GetHex()));
    item.push_back(Pair("nTime",(int64_t)hdr->nTime));
    item.push_back(Pair("nBits",(int64_t)hdr->nBits));
    return(item);
}

UniValue NSPV_headers_json(struct NSPV_equihdr *hdrs,int32_t numhdrs,int32_t height)
{
    UniValue array(UniValue::VARR); int32_t i;
    for (i=0; i<numhdrs; i++)
        array.push_back(NSPV_header_json(&hdrs[i],height+i));
    return(array);
}

UniValue NSPV_getinfo_json(struct NSPV_inforesp *ptr)
{
    UniValue result(UniValue::VOBJ); int32_t expiration; uint32_t timestamp = (uint32_t)time(NULL);
    result.push_back(Pair("result","success"));
    if ( NSPV_address.size() != 0 )
    {
        result.push_back(Pair("address",NSPV_address));
        result.push_back(Pair("pubkey",NSPV_pubkeystr));
    }
    if ( NSPV_logintime != 0 )
    {
        expiration = (NSPV_logintime + NSPV_AUTOLOGOUT - timestamp);
        result.push_back(Pair("wifexpires",expiration));
    }
    result.push_back(Pair("height",(int64_t)ptr->height));
    result.push_back(Pair("chaintip",ptr->blockhash.GetHex()));
    result.push_back(Pair("notarization",NSPV_ntz_json(&ptr->notarization)));
    result.push_back(Pair("header",NSPV_header_json(&ptr->H,ptr->hdrheight)));
    return(result);
}

UniValue NSPV_utxoresp_json(struct NSPV_utxoresp *utxos,int32_t numutxos)
{
    UniValue array(UniValue::VARR); int32_t i;
    for (i=0; i<numutxos; i++)
    {
        UniValue item(UniValue::VOBJ);
        item.push_back(Pair("height",(int64_t)utxos[i].height));
        item.push_back(Pair("txid",utxos[i].txid.GetHex()));
        item.push_back(Pair("vout",(int64_t)utxos[i].vout));
        item.push_back(Pair("value",(double)utxos[i].satoshis/COIN));
        if ( ASSETCHAINS_SYMBOL[0] == 0 )
            item.push_back(Pair("interest",(double)utxos[i].extradata/COIN));
        array.push_back(item);
    }
    return(array);
}

UniValue NSPV_utxosresp_json(struct NSPV_utxosresp *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("utxos",NSPV_utxoresp_json(ptr->utxos,ptr->numutxos)));
    result.push_back(Pair("address",ptr->coinaddr));
    result.push_back(Pair("height",(int64_t)ptr->nodeheight));
    result.push_back(Pair("numutxos",(int64_t)ptr->numutxos));
    result.push_back(Pair("balance",(double)ptr->total/COIN));
    if ( ASSETCHAINS_SYMBOL[0] == 0 )
        result.push_back(Pair("interest",(double)ptr->interest/COIN));
    return(result);
}

UniValue NSPV_ntzs_json(struct NSPV_ntzsresp *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("prev",NSPV_ntz_json(&ptr->prevntz)));
    result.push_back(Pair("next",NSPV_ntz_json(&ptr->nextntz)));
    return(result);
}

UniValue NSPV_ntzsproof_json(struct NSPV_ntzsproofresp *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("prevht",(int64_t)ptr->common.prevht));
    result.push_back(Pair("nextht",(int64_t)ptr->common.nextht));
    result.push_back(Pair("prevtxid",ptr->prevtxid.GetHex()));
    result.push_back(Pair("prevtxidht",(int64_t)ptr->prevtxidht));
    result.push_back(Pair("prevtxlen",(int64_t)ptr->prevtxlen));
    result.push_back(Pair("nexttxid",ptr->nexttxid.GetHex()));
    result.push_back(Pair("nexttxidht",(int64_t)ptr->nexttxidht));
    result.push_back(Pair("nexttxlen",(int64_t)ptr->prevtxlen));
    result.push_back(Pair("numhdrs",(int64_t)ptr->common.numhdrs));
    result.push_back(Pair("headers",NSPV_headers_json(ptr->common.hdrs,ptr->common.numhdrs,ptr->common.prevht)));
    return(result);
}

UniValue NSPV_broadcast_json(struct NSPV_broadcastresp *ptr,uint256 txid)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("expected",txid.GetHex()));
    result.push_back(Pair("broadcast",ptr->txid.GetHex()));
    result.push_back(Pair("retcode",(int64_t)ptr->retcode));
    switch ( ptr->retcode )
    {
        case 1: result.push_back(Pair("type","broadcast and mempool")); break;
        case 0: result.push_back(Pair("type","broadcast")); break;
        case -1: result.push_back(Pair("type","decode error")); break;
        case -2: result.push_back(Pair("type","timeout")); break;
        default: result.push_back(Pair("type","unknown")); break;
    }
    return(result);
}

UniValue NSPV_login(char *wifstr)
{
    UniValue result(UniValue::VOBJ); char coinaddr[64]; uint8_t data[128]; int32_t len,valid = 0;
    len = bitcoin_base58decode(data,wifstr);
    if ( strlen(wifstr) < 64 && (len == 38 && data[len-5] == 1) || (len == 37 && data[len-5] != 1) )
        valid = 1;
    if ( valid == 0 || data[0] != 188 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid wif"));
        result.push_back(Pair("len",(int64_t)len));
        result.push_back(Pair("prefix",(int64_t)data[0]));
        return(result);
    }
    memset(NSPV_wifstr,0,sizeof(NSPV_wifstr));
    NSPV_logintime = (uint32_t)time(NULL);
    if ( strcmp(NSPV_wifstr,wifstr) != 0 )
    {
        strncpy(NSPV_wifstr,wifstr,sizeof(NSPV_wifstr)-1);
        NSPV_key = DecodeSecret(wifstr);
    }
    result.push_back(Pair("result","success"));
    result.push_back(Pair("status","wif will expire in 777 seconds"));
    CPubKey pubkey = NSPV_key.GetPubKey();
    CKeyID vchAddress = pubkey.GetID();
    NSPV_address = EncodeDestination(vchAddress);
    result.push_back(Pair("address",NSPV_address));
    result.push_back(Pair("pubkey",HexStr(pubkey)));
    strcpy(NSPV_pubkeystr,HexStr(pubkey).c_str());
    if ( KOMODO_NSPV != 0 )
        decode_hex(NOTARY_PUBKEY33,33,NSPV_pubkeystr);
    result.push_back(Pair("wifprefix",(int64_t)data[0]));
    result.push_back(Pair("compressed",(int64_t)(data[len-5] == 1)));
    memset(data,0,sizeof(data));
    return(result);
}

UniValue NSPV_getinfo_req(int32_t reqht)
{
    uint8_t msg[64]; int32_t i,iter,len = 0; struct NSPV_inforesp I;
    NSPV_inforesp_purge(&NSPV_inforesult);
    msg[len++] = NSPV_INFO;
    len += iguana_rwnum(1,&msg[len],sizeof(reqht),&reqht);
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_inforesult.height != 0 )
                return(NSPV_getinfo_json(&NSPV_inforesult));
        }
    } else sleep(1);
    memset(&I,0,sizeof(I));
    return(NSPV_getinfo_json(&NSPV_inforesult));
}

UniValue NSPV_addressutxos(char *coinaddr)
{
    UniValue result(UniValue::VOBJ); uint8_t msg[64]; int32_t i,iter,slen,len = 0;
    //fprintf(stderr,"utxos %s NSPV addr %s\n",coinaddr,NSPV_address.c_str());
    if ( bitcoin_base58decode(msg,coinaddr) != 25 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid address"));
        return(result);
    }
    slen = (int32_t)strlen(coinaddr);
    msg[len++] = NSPV_UTXOS;
    msg[len++] = slen;
    memcpy(&msg[len],coinaddr,slen), len += slen;
    msg[len] = 0;
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_ADDRINDEX,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_utxosresult.nodeheight >= NSPV_inforesult.height && strcmp(coinaddr,NSPV_utxosresult.coinaddr) == 0 )
                return(NSPV_utxosresp_json(&NSPV_utxosresult));
        }
    } else sleep(1);
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","no utxos result"));
    return(result);
}

UniValue NSPV_notarizations(int32_t height)
{
    uint8_t msg[64]; int32_t i,iter,len = 0; struct NSPV_ntzsresp N;
    //if ( NSPV_ntzsresult.prevntz.height <= height && NSPV_ntzsresult.nextntz.height >= height )
    //    return(NSPV_ntzs_json(&NSPV_ntzsresult));
    msg[len++] = NSPV_NTZS;
    len += iguana_rwnum(1,&msg[len],sizeof(height),&height);
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_ntzsresult.prevntz.height <= height && NSPV_ntzsresult.nextntz.height >= height )
                return(NSPV_ntzs_json(&NSPV_ntzsresult));
        }
    } else sleep(1);
    memset(&N,0,sizeof(N));
    return(NSPV_ntzs_json(&N));
}

UniValue NSPV_txidhdrsproof(uint256 prevtxid,uint256 nexttxid)
{
    uint8_t msg[64]; int32_t i,iter,len = 0; struct NSPV_ntzsproofresp H;
    msg[len++] = NSPV_NTZSPROOF;
    len += iguana_rwbignum(1,&msg[len],sizeof(prevtxid),(uint8_t *)&prevtxid);
    len += iguana_rwbignum(1,&msg[len],sizeof(nexttxid),(uint8_t *)&nexttxid);
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_ntzsproofresult.prevtxid == prevtxid && NSPV_ntzsproofresult.nexttxid == nexttxid )
                return(NSPV_ntzsproof_json(&NSPV_ntzsproofresult));
        }
    } else sleep(1);
    memset(&H,0,sizeof(H));
    return(NSPV_ntzsproof_json(&H));
}

UniValue NSPV_hdrsproof(int32_t prevht,int32_t nextht)
{
    uint256 prevtxid,nexttxid;
    NSPV_notarizations(prevht);
    prevtxid = NSPV_ntzsresult.prevntz.txid;
    NSPV_notarizations(nextht);
    nexttxid = NSPV_ntzsresult.nextntz.txid;
    return(NSPV_txidhdrsproof(prevtxid,nexttxid));
}

UniValue NSPV_txproof(int32_t vout,uint256 txid,int32_t height)
{
    uint8_t msg[64]; int32_t i,iter,len = 0; struct NSPV_txproof P;
    //if ( NSPV_txproofresult.txid == txid && NSPV_txproofresult.height == height )
    //    return(NSPV_txproof_json(&NSPV_txproofresult));
    msg[len++] = NSPV_TXPROOF;
    len += iguana_rwnum(1,&msg[len],sizeof(height),&height);
    len += iguana_rwnum(1,&msg[len],sizeof(vout),&vout);
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    //fprintf(stderr,"req txproof %s/v%d at height.%d\n",txid.GetHex().c_str(),vout,height);
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_txproofresult.txid == txid && NSPV_txproofresult.height == height )
                return(NSPV_txproof_json(&NSPV_txproofresult));
        }
    } else sleep(1);
    //fprintf(stderr,"txproof timeout\n");
    memset(&P,0,sizeof(P));
    return(NSPV_txproof_json(&P));
}

UniValue NSPV_spentinfo(uint256 txid,int32_t vout)
{
    uint8_t msg[64]; int32_t i,iter,len = 0; struct NSPV_spentinfo I;
    //if ( NSPV_spentresult.txid == txid && NSPV_spentresult.vout == vout )
    //    return(NSPV_spentinfo_json(&NSPV_spentresult));
    msg[len++] = NSPV_SPENTINFO;
    len += iguana_rwnum(1,&msg[len],sizeof(vout),&vout);
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_SPENTINDEX,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_spentresult.txid == txid && NSPV_spentresult.vout == vout )
                return(NSPV_spentinfo_json(&NSPV_spentresult));
        }
    } else sleep(1);
    memset(&I,0,sizeof(I));
    return(NSPV_spentinfo_json(&I));
}

UniValue NSPV_broadcast(char *hex)
{
    uint8_t *msg,*data; uint256 txid; uint16_t n; int32_t i,iter,len = 0; struct NSPV_broadcastresp B;
    n = (int32_t)strlen(hex) >> 1;
    data = (uint8_t *)malloc(n);
    decode_hex(data,n,hex);
    txid = NSPV_doublesha256(data,n);
    msg = (uint8_t *)malloc(1 + sizeof(txid) + sizeof(n) + n);
    msg[len++] = NSPV_BROADCAST;
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    len += iguana_rwnum(1,&msg[len],sizeof(n),&n);
    memcpy(&msg[len],data,n), len += n;
    free(data);
    //fprintf(stderr,"send txid.%s\n",txid.GetHex().c_str());
    for (iter=0; iter<3; iter++);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_broadcastresult.txid == txid )
            {
                free(msg);
                return(NSPV_broadcast_json(&NSPV_broadcastresult,txid));
            }
        }
    } else sleep(1);
    free(msg);
    memset(&B,0,sizeof(B));
    B.retcode = -2;
    return(NSPV_broadcast_json(&B,txid));
}

int32_t NSPV_validatehdrs(struct NSPV_ntzsproofresp *ptr)
{
    int32_t i,height,txidht; CTransaction tx; uint256 blockhash,txid,desttxid;
    if ( (ptr->common.nextht-ptr->common.prevht+1) != ptr->common.numhdrs )
        return(-1);
    else if ( NSPV_txextract(tx,ptr->nextntz,ptr->nexttxlen) < 0 )
        return(-2);
    else if ( tx.GetHash() != ptr->nexttxid )
        return(-3);
    else if ( NSPV_notarizationextract(&height,&blockhash,&desttxid,tx) < 0 )
        return(-4);
    else if ( height != ptr->common.nextht )
        return(-5);
    else if ( NSPV_hdrhash(&ptr->common.hdrs[ptr->common.numhdrs-1]) != blockhash )
        return(-6);
    for (i=ptr->common.numhdrs-1; i>0; i--)
    {
        blockhash = NSPV_hdrhash(&ptr->common.hdrs[i-1]);
        if ( blockhash != ptr->common.hdrs[i].hashPrevBlock )
            return(-i-12);
    }
    if ( NSPV_txextract(tx,ptr->prevntz,ptr->prevtxlen) < 0 )
        return(-7);
    else if ( tx.GetHash() != ptr->prevtxid )
        return(-8);
    else if ( NSPV_notarizationextract(&height,&blockhash,&desttxid,tx) < 0 )
        return(-9);
    else if ( height != ptr->common.prevht )
        return(-10);
    else if ( NSPV_hdrhash(&ptr->common.hdrs[0]) != blockhash )
        return(-11);
    return(0);
}

int32_t NSPV_gettransaction(int32_t skipvalidation,int32_t vout,uint256 txid,int32_t height,CTransaction &tx)
{
    int32_t offset,retval = 0;
    NSPV_txproof(vout,txid,height);
    if ( NSPV_txproofresult.txid != txid || NSPV_txproofresult.unspentvalue <= 0 )
        return(-1);
    else if ( NSPV_txextract(tx,NSPV_txproofresult.tx,NSPV_txproofresult.txlen) < 0 || NSPV_txproofresult.txlen <= 0 )
        retval = -20;
    else if ( skipvalidation == 0 )
    {
        NSPV_notarizations(height); // gets the prev and next notarizations
        if ( NSPV_inforesult.notarization.height >= height && (NSPV_ntzsresult.prevntz.height == 0 || NSPV_ntzsresult.prevntz.height >= NSPV_ntzsresult.nextntz.height) )
        {
            fprintf(stderr,"issue manual bracket\n");
            NSPV_notarizations(height-1);
            NSPV_notarizations(height+1);
            NSPV_notarizations(height); // gets the prev and next notarizations
        }
        if ( NSPV_ntzsresult.prevntz.height != 0 && NSPV_ntzsresult.prevntz.height <= NSPV_ntzsresult.nextntz.height )
        {
            fprintf(stderr,"gettx ht.%d prev.%d next.%d\n",height,NSPV_ntzsresult.prevntz.height, NSPV_ntzsresult.nextntz.height);
            offset = (height - NSPV_ntzsresult.prevntz.height);
            if ( offset >= 0 && height <= NSPV_ntzsresult.nextntz.height )
            {
                NSPV_txidhdrsproof(NSPV_ntzsresult.prevntz.txid,NSPV_ntzsresult.nextntz.txid);
                if ( (retval= NSPV_validatehdrs(&NSPV_ntzsproofresult)) == 0 )
                {
                    std::vector<uint256> txids; std::vector<uint8_t> proof; uint256 proofroot;
                    proof.resize(NSPV_txproofresult.txprooflen);
                    memcpy(&proof[0],NSPV_txproofresult.txproof,NSPV_txproofresult.txprooflen);
                    proofroot = BitcoinGetProofMerkleRoot(proof,txids);
                    if ( proofroot != NSPV_ntzsproofresult.common.hdrs[offset].hashMerkleRoot )
                    {
                        fprintf(stderr,"prooflen.%d proofroot.%s vs %s\n",NSPV_txproofresult.txprooflen,proofroot.GetHex().c_str(),NSPV_ntzsproofresult.common.hdrs[offset].hashMerkleRoot.GetHex().c_str());
                        retval = -23;
                    }
                }
            } else retval = -22;
        } else retval = -2;
    }
    return(retval);
}

#endif // KOMODO_NSPVSUPERLITE_H
