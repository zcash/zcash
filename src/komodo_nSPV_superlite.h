
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

// interest calculations are currently just using what is returned, it should calculate it from scratch
// need to validate incoming data and update only if it is valid and more recent

#define NSPV_POLLITERS 15
#define NSPV_POLLMICROS 100000
#define NSPV_MAXVINS 64

CAmount AmountFromValue(const UniValue& value);
int32_t bitcoin_base58decode(uint8_t *data,char *coinaddr);

uint32_t NSPV_lastinfo,NSPV_logintime;
char NSPV_wifstr[64],NSPV_pubkeystr[67];
std::string NSPV_address;
CKey NSPV_key;
struct NSPV_inforesp NSPV_inforesult;
struct NSPV_utxosresp NSPV_utxosresult;
struct NSPV_spentinfo NSPV_spentresult;
struct NSPV_ntzsresp NSPV_ntzsresult;
struct NSPV_ntzsproofresp NSPV_ntzsproofresult;
struct NSPV_txproof NSPV_txproofresult;
struct NSPV_broadcastresp NSPV_broadcastresult;

CNode *NSPV_req(CNode *pnode,uint8_t *msg,int32_t len,uint64_t mask,int32_t ind)
{
    int32_t flag = 0; uint32_t timestamp = (uint32_t)time(NULL);
    if ( pnode == 0 )
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode *ptr,vNodes)
        {
            if ( ptr->prevtimes[ind] > timestamp )
                ptr->prevtimes[ind] = 0;
            if ( ptr->hSocket == INVALID_SOCKET )
                continue;
            if ( (ptr->nServices & mask) == mask && timestamp > ptr->prevtimes[ind] )
            {
                flag = 1;
                pnode = ptr;
                break;
            }  //else fprintf(stderr,"nServices %llx vs mask %llx, t%u vs %u, ind.%d\n",(long long)ptr->nServices,(long long)mask,timestamp,ptr->prevtimes[ind],ind);
        }
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

UniValue _NSPV_getinfo_json(struct NSPV_inforesp *ptr)
{
    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("height",(int64_t)ptr->height));
    result.push_back(Pair("chaintip",ptr->blockhash.GetHex()));
    result.push_back(Pair("notarization",NSPV_ntz_json(&ptr->notarization)));
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

UniValue NSPV_headers_json(struct NSPV_equihdr *hdrs,int32_t numhdrs)
{
    UniValue array(UniValue::VARR); int32_t i;
    for (i=0; i<numhdrs; i++)
    {
        UniValue item(UniValue::VOBJ);
        item.push_back(Pair("hashPrevBlock",hdrs[i].hashPrevBlock.GetHex()));
        item.push_back(Pair("hashMerkleRoot",hdrs[i].hashMerkleRoot.GetHex()));
        item.push_back(Pair("nTime",(int64_t)hdrs[i].nTime));
        array.push_back(item);
    }
    return(array);
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
    result.push_back(Pair("headers",NSPV_headers_json(ptr->common.hdrs,ptr->common.numhdrs)));
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
    strncpy(NSPV_wifstr,wifstr,sizeof(NSPV_wifstr)-1);
    NSPV_logintime = (uint32_t)time(NULL);
    result.push_back(Pair("result","success"));
    result.push_back(Pair("status","wif will expire in 60 seconds"));
    NSPV_key = DecodeSecret(wifstr);
    CPubKey pubkey = NSPV_key.GetPubKey();
    CKeyID vchAddress = pubkey.GetID();
    NSPV_address = EncodeDestination(vchAddress);
    result.push_back(Pair("address",NSPV_address));
    result.push_back(Pair("pubkey",HexStr(pubkey)));
    strcpy(NSPV_pubkeystr,HexStr(pubkey).c_str());
    result.push_back(Pair("wifprefix",(int64_t)data[0]));
    result.push_back(Pair("compressed",(int64_t)(data[len-5] == 1)));
    memset(data,0,sizeof(data));
    return(result);
}

UniValue NSPV_getinfo_json()
{
    uint8_t msg[64]; int32_t i,iters,len = 0; struct NSPV_inforesp I;
    NSPV_inforesp_purge(&NSPV_inforesult);
    msg[len++] = NSPV_INFO;
    for (iters=0; iters<3; iters++)
    {
        //fprintf(stderr,"issue getinfo\n");
        if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
        {
            for (i=0; i<NSPV_POLLITERS; i++)
            {
                usleep(NSPV_POLLMICROS);
                if ( NSPV_inforesult.height != 0 )
                    return(_NSPV_getinfo_json(&NSPV_inforesult));
            }
        } else sleep(1);
    }
    memset(&I,0,sizeof(I));
    return(_NSPV_getinfo_json(&NSPV_inforesult));
}

UniValue NSPV_addressutxos(char *coinaddr)
{
    UniValue result(UniValue::VOBJ); uint8_t msg[64]; int32_t i,slen,len = 0;
    if ( bitcoin_base58decode(msg,coinaddr) != 25 )
    {
        result.push_back(Pair("result","error"));
        result.push_back(Pair("error","invalid address"));
        return(result);
    }
    if ( NSPV_inforesult.height == 0 )
    {
        msg[0] = NSPV_INFO;
        fprintf(stderr,"issue getinfo\n");
        NSPV_req(0,msg,1,NODE_NSPV,msg[0]>>1);
    }
    slen = (int32_t)strlen(coinaddr);
    msg[len++] = NSPV_UTXOS;
    msg[len++] = slen;
    memcpy(&msg[len],coinaddr,slen), len += slen;
    msg[len] = 0;
    if ( NSPV_req(0,msg,len,NODE_ADDRINDEX,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_utxosresult.nodeheight >= NSPV_inforesult.height )
                return(NSPV_utxosresp_json(&NSPV_utxosresult));
        }
    }
    result.push_back(Pair("result","error"));
    result.push_back(Pair("error","no utxos result"));
    return(result);
}

UniValue NSPV_notarizations(int32_t height)
{
    uint8_t msg[64]; int32_t i,len = 0; struct NSPV_ntzsresp N;
    if ( NSPV_ntzsresult.prevntz.height <= height && NSPV_ntzsresult.nextntz.height >= height )
        return(NSPV_ntzs_json(&NSPV_ntzsresult));
    msg[len++] = NSPV_NTZS;
    len += iguana_rwnum(1,&msg[len],sizeof(height),&height);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_ntzsresult.prevntz.height <= height && NSPV_ntzsresult.nextntz.height >= height )
                return(NSPV_ntzs_json(&NSPV_ntzsresult));
        }
    }
    memset(&N,0,sizeof(N));
    return(NSPV_ntzs_json(&N));
}

UniValue NSPV_hdrsproof(int32_t prevheight,int32_t nextheight)
{
    uint8_t msg[64]; int32_t i,len = 0; struct NSPV_ntzsproofresp H;
    if ( NSPV_ntzsproofresult.common.prevht == prevheight && NSPV_ntzsproofresult.common.nextht >= nextheight )
        return(NSPV_ntzsproof_json(&NSPV_ntzsproofresult));
    msg[len++] = NSPV_NTZSPROOF;
    len += iguana_rwnum(1,&msg[len],sizeof(prevheight),&prevheight);
    len += iguana_rwnum(1,&msg[len],sizeof(nextheight),&nextheight);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_ntzsproofresult.common.prevht == prevheight && NSPV_ntzsproofresult.common.nextht >= nextheight )
                return(NSPV_ntzsproof_json(&NSPV_ntzsproofresult));
        }
    }
    memset(&H,0,sizeof(H));
    return(NSPV_ntzsproof_json(&H));
}

UniValue NSPV_txproof(uint256 txid,int32_t height)
{
    uint8_t msg[64]; int32_t i,len = 0; struct NSPV_txproof P;
    if ( NSPV_txproofresult.txid == txid && NSPV_txproofresult.height == height )
        return(NSPV_txproof_json(&NSPV_txproofresult));
    msg[len++] = NSPV_TXPROOF;
    len += iguana_rwnum(1,&msg[len],sizeof(height),&height);
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    //fprintf(stderr,"req txproof %s at height.%d\n",txid.GetHex().c_str(),height);
    if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_txproofresult.txid == txid && NSPV_txproofresult.height == height )
                return(NSPV_txproof_json(&NSPV_txproofresult));
        }
    }
    memset(&P,0,sizeof(P));
    return(NSPV_txproof_json(&P));
}

UniValue NSPV_spentinfo(uint256 txid,int32_t vout)
{
    uint8_t msg[64]; int32_t i,len = 0; struct NSPV_spentinfo I;
    if ( NSPV_spentresult.txid == txid && NSPV_spentresult.vout == vout )
        return(NSPV_spentinfo_json(&NSPV_spentresult));
    msg[len++] = NSPV_SPENTINFO;
    len += iguana_rwnum(1,&msg[len],sizeof(vout),&vout);
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    if ( NSPV_req(0,msg,len,NODE_SPENTINDEX,msg[0]>>1) != 0 )
    {
        for (i=0; i<NSPV_POLLITERS; i++)
        {
            usleep(NSPV_POLLMICROS);
            if ( NSPV_spentresult.txid == txid && NSPV_spentresult.vout == vout )
                return(NSPV_spentinfo_json(&NSPV_spentresult));
        }
    }
    memset(&I,0,sizeof(I));
    return(NSPV_spentinfo_json(&I));
}

UniValue NSPV_broadcast(char *hex)
{
    uint8_t *msg,*data; bits256 _txid; uint256 txid; uint16_t n; int32_t i,len = 0; struct NSPV_broadcastresp B;
    n = (int32_t)strlen(hex) >> 1;
    data = (uint8_t *)malloc(n);
    decode_hex(data,n,hex);
    _txid = bits256_doublesha256(0,data,n);
    for (i=0; i<32; i++)
        ((uint8_t *)&txid[i] = _txid.bytes[31 - i];
    msg = (uint8_t *)malloc(1 + sizeof(txid) + sizeof(n) + n);
    msg[len++] = NSPV_BROADCAST;
    len += iguana_rwbignum(1,&msg[len],sizeof(txid),(uint8_t *)&txid);
    len += iguana_rwnum(1,&msg[len],sizeof(n),&n);
    memcpy(&msg[len],data,n), len += n;
    free(data);
    fprintf(stderr,"send txid.%s\n",txid.GetHex().c_str());
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
    }
    free(msg);
    memset(&B,0,sizeof(B));
    B.retcode = -2;
    return(NSPV_broadcast_json(&B,txid));
}

void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> response) // received a response
{
    int32_t len; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= response.size()) > 0 )
    {
        switch ( response[0] )
        {
        case NSPV_INFORESP:
            NSPV_inforesp_purge(&NSPV_inforesult);
            NSPV_rwinforesp(0,&response[1],&NSPV_inforesult);
            //fprintf(stderr,"got info response %u size.%d height.%d\n",timestamp,(int32_t)response.size(),NSPV_inforesult.height); // update current height and ntrz status
            break;
        case NSPV_UTXOSRESP:
            NSPV_utxosresp_purge(&NSPV_utxosresult);
            NSPV_rwutxosresp(0,&response[1],&NSPV_utxosresult);
            fprintf(stderr,"got utxos response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos list
            break;
        case NSPV_NTZSRESP:
            NSPV_ntzsresp_purge(&NSPV_ntzsresult);
            NSPV_rwntzsresp(0,&response[1],&NSPV_ntzsresult);
            fprintf(stderr,"got ntzs response %u size.%d\n",timestamp,(int32_t)response.size());
            break;
        case NSPV_NTZSPROOFRESP:
            NSPV_ntzsproofresp_purge(&NSPV_ntzsproofresult);
            NSPV_rwntzsproofresp(0,&response[1],&NSPV_ntzsproofresult);
            fprintf(stderr,"got ntzproof response %u size.%d prev.%d next.%d\n",timestamp,(int32_t)response.size(),NSPV_ntzsproofresult.common.prevht,NSPV_ntzsproofresult.common.nextht);
            break;
        case NSPV_TXPROOFRESP:
            NSPV_txproof_purge(&NSPV_txproofresult);
            NSPV_rwtxproof(0,&response[1],&NSPV_txproofresult);
            fprintf(stderr,"got txproof response %u size.%d\n",timestamp,(int32_t)response.size());
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

#endif // KOMODO_NSPVSUPERLITE_H
