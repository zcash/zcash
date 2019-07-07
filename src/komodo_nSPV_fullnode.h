
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

#ifndef KOMODO_NSPVFULLNODE_H
#define KOMODO_NSPVFULLNODE_H

// NSPV_get... functions need to return the exact serialized length, which is the size of the structure minus size of pointers, plus size of allocated data

#include "notarisationdb.h"

uint256 NSPV_getnotarization_txid(int32_t *ntzheightp,int32_t height)
{
    uint256 txid; Notarisation nota; char *symbol = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
    memset(&txid,0,sizeof(txid));
    *ntzheightp = 0;
    int32_t matchedHeight = ScanNotarisationsDB2(height,symbol,1440,nota);
    if ( matchedHeight != 0 )
    {
        *ntzheightp = matchedHeight;
        txid = nota.first;
    }
    return(txid);
}

uint256 NSPV_extract_desttxid(int32_t *heightp,char *symbol,std::vector<uint8_t> opret)
{
    uint256 desttxid; int32_t i;
    //for (i=0; i<32; i++)
    //    fprintf(stderr,"%02x",opret[i]);
    //fprintf(stderr," blockhash, ");
    //for (i=0; i<4; i++)
    //    fprintf(stderr,"%02x",opret[32+i]);
    //fprintf(stderr," height, ");
    iguana_rwnum(0,&opret[32],sizeof(*heightp),heightp);
    //for (i=0; i<32; i++)
    //    fprintf(stderr,"%02x",opret[36+i]);
    //fprintf(stderr," desttxid\n");
    for (i=0; i<32; i++)
        ((uint8_t *)&desttxid)[i] = opret[4 + 32 + i];
    return(desttxid);
}

int32_t komodo_notarized_bracket(uint256 txids[2],int32_t txidhts[2],uint256 desttxids[2],int32_t ntzheights[2],int32_t height)
{
    int32_t txidht; Notarisation nota; char *symbol;
    symbol = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
    memset(txids,0,sizeof(*txids)*2);
    memset(desttxids,0,sizeof(*desttxids)*2);
    memset(ntzheights,0,sizeof(*ntzheights)*2);
    memset(txidhts,0,sizeof(*txidhts)*2);
    if ( (txidht= ScanNotarisationsDB(height,symbol,1440,nota)) == 0 )
        return(-1);
    txids[0] = nota.first;
    txidhts[0] = txidht;
    desttxids[0] = NSPV_extract_desttxid(&ntzheights[0],symbol,E_MARSHAL(ss << nota.second));
    //if ( height != 2668 )
    //    fprintf(stderr,"scan.%d -> %s txidht.%d ntzht.%d\n",height,desttxids[0].GetHex().c_str(),txidht,ntzheights[0]);
    if ( ntzheights[0] == height-1 ) // offset the +1 from caller
    {
        txids[1] = txids[0];
        txidhts[1] = txidhts[0];
        ntzheights[1] = ntzheights[0];
        desttxids[1] = desttxids[0];
        return(0);
    }
    if ( (txidht= ScanNotarisationsDB2(height,symbol,1440,nota)) != 0 )
    {
        txids[1] = nota.first;
        txidhts[1] = txidht;
        desttxids[1] = NSPV_extract_desttxid(&ntzheights[1],symbol,E_MARSHAL(ss << nota.second));
    }
    return(0);
}

int32_t NSPV_ntzextract(struct NSPV_ntz *ptr,uint256 ntztxid,int32_t txidht,uint256 desttxid,int32_t ntzheight)
{
    ptr->blockhash = *chainActive[ntzheight]->phashBlock;
    ptr->height = ntzheight;
    ptr->txidheight = txidht;
    ptr->othertxid = desttxid;
    ptr->txid = ntztxid;
    return(0);
}

int32_t NSPV_getntzsresp(struct NSPV_ntzsresp *ptr,int32_t height)
{
    uint256 txids[2],desttxids[2]; int32_t ntzheights[2],txidhts[2];
    if ( height < chainActive.LastTip()->GetHeight() )
        height++;
    if ( komodo_notarized_bracket(txids,txidhts,desttxids,ntzheights,height) == 0 )
    {
        if ( ntzheights[0] != 0 )
        {
            if ( NSPV_ntzextract(&ptr->prevntz,txids[0],txidhts[0],desttxids[0],ntzheights[0]) < 0 )
                return(-1);
        }
        if ( ntzheights[1] != 0 )
        {
            if ( NSPV_ntzextract(&ptr->nextntz,txids[1],txidhts[1],desttxids[1],ntzheights[1]) < 0 )
                return(-1);
        }
    }
    return(sizeof(*ptr));
}

int32_t NSPV_getinfo(struct NSPV_inforesp *ptr)
{
    int32_t prevMoMheight,len = 0; CBlockIndex *pindex; struct NSPV_ntzsresp pair;
    if ( (pindex= chainActive.LastTip()) != 0 )
    {
        ptr->height = pindex->GetHeight();
        ptr->blockhash = pindex->GetBlockHash();
        if ( NSPV_getntzsresp(&pair,ptr->height-1) < 0 )
            return(-1);
        ptr->notarization = pair.prevntz;
        return(sizeof(*ptr));
    } else return(-1);
}

int32_t NSPV_getaddressutxos(struct NSPV_utxosresp *ptr,char *coinaddr) // check mempool
{
    int64_t total = 0,interest=0; uint32_t locktime; int32_t tipheight,maxlen,txheight,n = 0,len = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,false);
    maxlen = MAX_BLOCK_SIZE(tipheight) - 512;
    maxlen /= sizeof(*ptr->utxos);
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    if ( (ptr->numutxos= (int32_t)unspentOutputs.size()) > 0 && ptr->numutxos < maxlen )
    {
        tipheight = chainActive.LastTip()->GetHeight();
        ptr->nodeheight = tipheight;
        ptr->utxos = (struct NSPV_utxoresp *)calloc(ptr->numutxos,sizeof(*ptr->utxos));
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            ptr->utxos[n].txid = it->first.txhash;
            ptr->utxos[n].vout = (int32_t)it->first.index;
            ptr->utxos[n].satoshis = it->second.satoshis;
            ptr->utxos[n].height = it->second.blockHeight;
            if ( ASSETCHAINS_SYMBOL[0] == 0 && it->second.satoshis >= 10*COIN )
            {
                ptr->utxos[n].extradata = komodo_accrued_interest(&txheight,&locktime,ptr->utxos[n].txid,ptr->utxos[n].vout,ptr->utxos[n].height,ptr->utxos[n].satoshis,tipheight);
                interest += ptr->utxos[n].extradata;
            }
            total += it->second.satoshis;
            n++;
        }
        if ( len < maxlen )
        {
            len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->utxos)*ptr->numutxos - sizeof(ptr->utxos));
            fprintf(stderr,"getaddressutxos for %s -> n.%d:%d total %.8f interest %.8f len.%d\n",coinaddr,n,ptr->numutxos,dstr(total),dstr(interest),len);
            if ( n == ptr->numutxos )
            {
                ptr->total = total;
                ptr->interest = interest;
                return(len);
            }
        }
    }
    if ( ptr->utxos != 0 )
        free(ptr->utxos);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}

uint8_t *NSPV_getrawtx(uint256 &hashBlock,uint16_t *txlenp,uint256 txid)
{
    CTransaction tx; uint8_t *rawtx = 0;
    *txlenp = 0;
    {
        LOCK(cs_main);
        if (!GetTransaction(txid, tx, hashBlock, false))
            return(0);
        string strHex = EncodeHexTx(tx);
        *txlenp = (int32_t)strHex.size() >> 1;
        if ( *txlenp > 0 )
        {
            rawtx = (uint8_t *)calloc(1,*txlenp);
            decode_hex(rawtx,*txlenp,(char *)strHex.c_str());
        }
    }
    return(rawtx);
}

int32_t NSPV_sendrawtransaction(struct NSPV_broadcastresp *ptr,uint8_t *data,int32_t n)
{
    CTransaction tx; std::vector<uint8_t> rawdata;
    ptr->retcode = 0;
    rawdata.resize(n);
    memcpy(&rawdata[0],data,n);
    fprintf(stderr,"sendraw.(%s)\n",HexStr(rawdata).c_str());
    if ( DecodeHexTx(tx,HexStr(rawdata)) != 0 )
    {
        LOCK(cs_main);
        ptr->txid = tx.GetHash();
        fprintf(stderr,"addmempool transaction %s\n",ptr->txid.GetHex().c_str());
        if ( myAddtomempool(tx) != 0 )
            ptr->retcode = 1;
        else ptr->retcode = 0;
        //fprintf(stderr,"relay transaction %s retcode.%d\n",ptr->txid.GetHex().c_str(),ptr->retcode);
        RelayTransaction(tx);
    } else ptr->retcode = -1;
    return(sizeof(*ptr));
}

int32_t NSPV_gettxproof(struct NSPV_txproof *ptr,uint256 txid,int32_t height)
{
    int32_t flag = 0,len = 0; uint256 hashBlock; CBlock block; CBlockIndex *pindex;
    if ( (ptr->tx= NSPV_getrawtx(hashBlock,&ptr->txlen,txid)) == 0 )
        return(-1);
    ptr->txid = txid;
    ptr->height = height;
    if ( (pindex= komodo_chainactive(height)) != 0 && komodo_blockload(block,pindex) == 0 )
    {
        BOOST_FOREACH(const CTransaction&tx, block.vtx)
        {
            if ( tx.GetHash() == txid )
            {
                flag = 1;
                break;
            }
        }
        if ( flag != 0 )
        {
            set<uint256> setTxids;
            CDataStream ssMB(SER_NETWORK, PROTOCOL_VERSION);
            setTxids.insert(txid);
            CMerkleBlock mb(block, setTxids);
            ssMB << mb;
            std::vector<uint8_t> proof(ssMB.begin(), ssMB.end());
            ptr->txprooflen = (int32_t)proof.size();
            if ( ptr->txprooflen > 0 )
            {
                ptr->txproof = (uint8_t *)calloc(1,ptr->txprooflen);
                memcpy(ptr->txproof,&proof[0],ptr->txprooflen);
            }
            //fprintf(stderr,"gettxproof slen.%d\n",(int32_t)(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen));
            return(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen);
        }
    }
    return(-1);
}

int32_t NSPV_setequihdr(struct NSPV_equihdr *hdr,int32_t height)
{
    CBlockIndex *pindex;
    if ( (pindex= komodo_chainactive(height)) != 0 )
    {
        hdr->nVersion = pindex->nVersion;
        if ( pindex->pprev == 0 )
            return(-1);
        hdr->hashPrevBlock = pindex->pprev->GetBlockHash();
        hdr->hashMerkleRoot = pindex->hashMerkleRoot;
        hdr->hashFinalSaplingRoot = pindex->hashFinalSaplingRoot;
        hdr->nTime = pindex->nTime;
        hdr->nBits = pindex->nBits;
        hdr->nNonce = pindex->nNonce;
        memcpy(hdr->nSolution,&pindex->nSolution[0],sizeof(hdr->nSolution));
        return(sizeof(*hdr));
    }
    return(-1);
}

int32_t NSPV_getntzsproofresp(struct NSPV_ntzsproofresp *ptr,int32_t prevht,int32_t nextht)
{
    int32_t i; uint256 hashBlock;
    if ( prevht > nextht || (nextht-prevht) > 1440 )
    {
        fprintf(stderr,"illegal prevht.%d nextht.%d\n",prevht,nextht);
        return(-1);
    }
    ptr->common.prevht = prevht;
    ptr->common.nextht = nextht;
    ptr->common.numhdrs = (nextht - prevht + 1);
    ptr->common.hdrs = (struct NSPV_equihdr *)calloc(ptr->common.numhdrs,sizeof(*ptr->common.hdrs));
    //fprintf(stderr,"prev.%d next.%d allocate numhdrs.%d\n",prevht,nextht,ptr->common.numhdrs);
    for (i=0; i<ptr->common.numhdrs; i++)
    {
        if ( NSPV_setequihdr(&ptr->common.hdrs[i],prevht+i) < 0 )
        {
            fprintf(stderr,"error setting hdr.%d\n",prevht+i);
            free(ptr->common.hdrs);
            ptr->common.hdrs = 0;
            return(-1);
        }
    }
    ptr->prevtxid = NSPV_getnotarization_txid(&ptr->prevtxidht,prevht);
    ptr->prevntz = NSPV_getrawtx(hashBlock,&ptr->prevtxlen,ptr->prevtxid);
    ptr->nexttxid = NSPV_getnotarization_txid(&ptr->nexttxidht,nextht);
    ptr->nextntz = NSPV_getrawtx(hashBlock,&ptr->nexttxlen,ptr->nexttxid);
    //fprintf(stderr,"prevtxlen.%d nexttxlen.%d size %ld -> %ld\n",ptr->prevtxlen,ptr->nexttxlen,sizeof(*ptr),sizeof(*ptr) - sizeof(ptr->common.hdrs) - sizeof(ptr->prevntz) - sizeof(ptr->nextntz) + ptr->prevlen + ptr->nextlen);
    return(sizeof(*ptr) + sizeof(*ptr->common.hdrs)*ptr->common.numhdrs - sizeof(ptr->common.hdrs) - sizeof(ptr->prevntz) - sizeof(ptr->nextntz) + ptr->prevtxlen + ptr->nexttxlen);
}

int32_t NSPV_getspentinfo(struct NSPV_spentinfo *ptr,uint256 txid,int32_t vout)
{
    int32_t len = 0;
    ptr->txid = txid;
    ptr->vout = vout;
    ptr->spentvini = -1;
    len = (int32_t)(sizeof(*ptr) - sizeof(ptr->spent.tx) - sizeof(ptr->spent.txproof));
    if ( CCgetspenttxid(ptr->spent.txid,ptr->spentvini,ptr->spent.height,txid,vout) == 0 )
    {
        if ( NSPV_gettxproof(&ptr->spent,ptr->spent.txid,ptr->spent.height) > 0 )
            len += ptr->spent.txlen + ptr->spent.txprooflen;
        else
        {
            NSPV_txproof_purge(&ptr->spent);
            return(-1);
        }
    }
    return(len);
}

void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> request) // received a request
{
    int32_t len,slen,ind; std::vector<uint8_t> response; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        if ( (ind= request[0]>>1) >= sizeof(pfrom->prevtimes)/sizeof(*pfrom->prevtimes) )
            ind = (int32_t)(sizeof(pfrom->prevtimes)/sizeof(*pfrom->prevtimes)) - 1;
        if ( pfrom->prevtimes[ind] > timestamp )
            pfrom->prevtimes[ind] = 0;
        if ( request[0] == NSPV_INFO ) // info
        {
            //fprintf(stderr,"check info %u vs %u, ind.%d\n",timestamp,pfrom->prevtimes[ind],ind);
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_inforesp I;
                memset(&I,0,sizeof(I));
                if ( (slen= NSPV_getinfo(&I)) > 0 )
                {
                    response.resize(1 + slen);
                    response[0] = NSPV_INFORESP;
                    if ( NSPV_rwinforesp(1,&response[1],&I) == slen )
                    {
                        pfrom->PushMessage("nSPV",response);
                        pfrom->prevtimes[ind] = timestamp;
                    }
                    NSPV_inforesp_purge(&I);
                }
            }
        }
        else if ( request[0] == NSPV_UTXOS )
        {
            //fprintf(stderr,"utxos: %u > %u, ind.%d, len.%d\n",timestamp,pfrom->prevtimes[ind],ind,len);
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_utxosresp U; char coinaddr[64];
                if ( len < 64 && request[1] == len-2 )
                {
                    memcpy(coinaddr,&request[2],request[1]);
                    coinaddr[request[1]] = 0;
                    memset(&U,0,sizeof(U));
                    if ( (slen= NSPV_getaddressutxos(&U,coinaddr)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_UTXOSRESP;
                        if ( NSPV_rwutxosresp(1,&response[1],&U) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_utxosresp_purge(&U);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_NTZS )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_ntzsresp N; int32_t height;
                if ( len == 1+sizeof(height) )
                {
                    iguana_rwnum(0,&request[1],sizeof(height),&height);
                    memset(&N,0,sizeof(N));
                    if ( (slen= NSPV_getntzsresp(&N,height)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_NTZSRESP;
                        if ( NSPV_rwntzsresp(1,&response[1],&N) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_ntzsresp_purge(&N);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_NTZSPROOF )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_ntzsproofresp P; int32_t prevht,nextht;
                if ( len == 1+sizeof(prevht)+sizeof(nextht) )
                {
                    iguana_rwnum(0,&request[1],sizeof(prevht),&prevht);
                    iguana_rwnum(0,&request[1+sizeof(prevht)],sizeof(nextht),&nextht);
                    if ( prevht != 0 && nextht != 0 && nextht >= prevht )
                    {
                        memset(&P,0,sizeof(P));
                        if ( (slen= NSPV_getntzsproofresp(&P,prevht,nextht)) > 0 )
                        {
                            response.resize(1 + slen);
                            response[0] = NSPV_NTZSPROOFRESP;
                            P.common.numhdrs = (nextht - prevht + 1);
                            if ( NSPV_rwntzsproofresp(1,&response[1],&P) == slen )
                            {
                                pfrom->PushMessage("nSPV",response);
                                pfrom->prevtimes[ind] = timestamp;
                            }
                            NSPV_ntzsproofresp_purge(&P);
                        }
                    }
                }
            }
        }
        else if ( request[0] == NSPV_TXPROOF )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_txproof P; uint256 txid; int32_t height;
                if ( len == 1+sizeof(txid)+sizeof(height) )
                {
                    iguana_rwnum(0,&request[1],sizeof(height),&height);
                    iguana_rwbignum(0,&request[1+sizeof(height)],sizeof(txid),(uint8_t *)&txid);
                    //fprintf(stderr,"got txid ht.%d\n",txid.GetHex().c_str(),height);
                    memset(&P,0,sizeof(P));
                    if ( (slen= NSPV_gettxproof(&P,txid,height)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_TXPROOFRESP;
                        if ( NSPV_rwtxproof(1,&response[1],&P) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_txproof_purge(&P);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_SPENTINFO )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_spentinfo S; int32_t vout; uint256 txid;
                if ( len == 1+sizeof(txid)+sizeof(vout) )
                {
                    iguana_rwnum(0,&request[1],sizeof(vout),&vout);
                    iguana_rwbignum(0,&request[1+sizeof(vout)],sizeof(txid),(uint8_t *)&txid);
                    memset(&S,0,sizeof(S));
                    if ( (slen= NSPV_getspentinfo(&S,txid,vout)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_SPENTINFORESP;
                        if ( NSPV_rwspentinfo(1,&response[1],&S) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_spentinfo_purge(&S);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_BROADCAST )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_broadcastresp B; uint16_t n,offset; uint256 txid;
                if ( len > 1+sizeof(txid)+sizeof(n) )
                {
                    iguana_rwbignum(0,&request[1],sizeof(txid),(uint8_t *)&txid);
                    iguana_rwnum(0,&request[1+sizeof(txid)],sizeof(n),&n);
                    memset(&B,0,sizeof(B));
                    offset = 1 + sizeof(txid) + sizeof(n);
                    if ( request.size() == offset+n && (slen= NSPV_sendrawtransaction(&B,&request[offset],n)) > 0 )
                    {
                        response.resize(1 + slen);
                        response[0] = NSPV_BROADCASTRESP;
                        if ( NSPV_rwbroadcastresp(1,&response[1],&B) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_broadcast_purge(&B);
                    }
                }
            }
        }
   }
}

#endif // KOMODO_NSPVFULLNODE_H
