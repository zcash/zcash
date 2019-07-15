
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

struct NSPV_ntzargs
{
    uint256 txid,desttxid,blockhash;
    int32_t txidht,ntzheight;
};

int32_t NSPV_notarization_find(struct NSPV_ntzargs *args,int32_t height,int32_t dir)
{
    int32_t ntzheight = 0; uint256 hashBlock; CTransaction tx; Notarisation nota; char *symbol; std::vector<uint8_t> opret;
    symbol = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
    memset(args,0,sizeof(*args));
    if ( dir > 0 )
        height += 10;
    if ( (args->txidht= ScanNotarisationsDB(height,symbol,1440,nota)) == 0 )
        return(-1);
    args->txid = nota.first;
    if ( !GetTransaction(args->txid,tx,hashBlock,false) || tx.vout.size() < 2 )
        return(-2);
    GetOpReturnData(tx.vout[1].scriptPubKey,opret);
    if ( opret.size() >= 32*2+4 )
        args->desttxid = NSPV_opretextract(&args->ntzheight,&args->blockhash,symbol,opret,args->txid);
    return(args->ntzheight);
}

int32_t NSPV_notarized_bracket(struct NSPV_ntzargs *prev,struct NSPV_ntzargs *next,int32_t height)
{
    uint256 bhash; int32_t txidht,ntzht,nextht,i=0;
    memset(prev,0,sizeof(*prev));
    memset(next,0,sizeof(*next));
    if ( (ntzht= NSPV_notarization_find(prev,height,-1)) < 0 || ntzht > height || ntzht == 0 )
        return(-1);
    txidht = height+1;
    while ( (ntzht=  NSPV_notarization_find(next,txidht,1)) < height )
    {
        nextht = next->txidht + 10*i;
//fprintf(stderr,"found forward ntz, but ntzht.%d vs height.%d, txidht.%d -> nextht.%d\n",next->ntzheight,height,txidht,nextht);
        memset(next,0,sizeof(*next));
        txidht = nextht;
        if ( ntzht <= 0 )
            break;
        if ( i++ > 10 )
            break;
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

int32_t NSPV_getntzsresp(struct NSPV_ntzsresp *ptr,int32_t origreqheight)
{
    struct NSPV_ntzargs prev,next; int32_t reqheight = origreqheight;
    if ( reqheight < chainActive.LastTip()->GetHeight() )
        reqheight++;
    if ( NSPV_notarized_bracket(&prev,&next,reqheight) == 0 )
    {
        if ( prev.ntzheight != 0 )
        {
            ptr->reqheight = origreqheight;
            if ( NSPV_ntzextract(&ptr->prevntz,prev.txid,prev.txidht,prev.desttxid,prev.ntzheight) < 0 )
                return(-1);
        }
        if ( next.ntzheight != 0 )
        {
            if ( NSPV_ntzextract(&ptr->nextntz,next.txid,next.txidht,next.desttxid,next.ntzheight) < 0 )
                return(-1);
        }
    }
    return(sizeof(*ptr));
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

int32_t NSPV_getinfo(struct NSPV_inforesp *ptr,int32_t reqheight)
{
    int32_t prevMoMheight,len = 0; CBlockIndex *pindex; struct NSPV_ntzsresp pair;
    if ( (pindex= chainActive.LastTip()) != 0 )
    {
        ptr->height = pindex->GetHeight();
        ptr->blockhash = pindex->GetBlockHash();
        memset(&pair,0,sizeof(pair));
        if ( NSPV_getntzsresp(&pair,ptr->height-1) < 0 )
            return(-1);
        ptr->notarization = pair.prevntz;
        if ( reqheight == 0 )
            reqheight = ptr->height;
        ptr->hdrheight = reqheight;
        if ( NSPV_setequihdr(&ptr->H,reqheight) < 0 )
            return(-1);
        return(sizeof(*ptr));
    } else return(-1);
}

int32_t NSPV_getaddressutxos(struct NSPV_utxosresp *ptr,char *coinaddr,bool isCC,int32_t skipcount) // check mempool
{
    int64_t total = 0,interest=0; uint32_t locktime; int32_t ind=0,tipheight,maxlen,txheight,n = 0,len = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    SetCCunspents(unspentOutputs,coinaddr,isCC);
    maxlen = MAX_BLOCK_SIZE(tipheight) - 512;
    maxlen /= sizeof(*ptr->utxos);
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    ptr->CCflag = isCC;
    if ( skipcount < 0 )
        skipcount = 0;
    if ( (ptr->numutxos= (int32_t)unspentOutputs.size()) >= 0 && ptr->numutxos < maxlen )
    {
        tipheight = chainActive.LastTip()->GetHeight();
        ptr->nodeheight = tipheight;
        if ( skipcount >= ptr->numutxos )
            skipcount = ptr->numutxos-1;
        ptr->skipcount = skipcount;
        ptr->utxos = (struct NSPV_utxoresp *)calloc(ptr->numutxos-skipcount,sizeof(*ptr->utxos));
        for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++)
        {
            // if gettxout is != null to handle mempool
            {
                if ( n >= skipcount )
                {
                    ptr->utxos[ind].txid = it->first.txhash;
                    ptr->utxos[ind].vout = (int32_t)it->first.index;
                    ptr->utxos[ind].satoshis = it->second.satoshis;
                    ptr->utxos[ind].height = it->second.blockHeight;
                    if ( ASSETCHAINS_SYMBOL[0] == 0 && it->second.satoshis >= 10*COIN )
                    {
                        ptr->utxos[n].extradata = komodo_accrued_interest(&txheight,&locktime,ptr->utxos[ind].txid,ptr->utxos[ind].vout,ptr->utxos[ind].height,ptr->utxos[ind].satoshis,tipheight);
                        interest += ptr->utxos[ind].extradata;
                    }
                    ind++;
                    total += it->second.satoshis;
                }
                n++;
            }
        }
        ptr->numutxos = ind;
        if ( len < maxlen )
        {
            len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->utxos)*ptr->numutxos - sizeof(ptr->utxos));
            //fprintf(stderr,"getaddressutxos for %s -> n.%d:%d total %.8f interest %.8f len.%d\n",coinaddr,n,ptr->numutxos,dstr(total),dstr(interest),len);
            ptr->total = total;
            ptr->interest = interest;
            return(len);
        }
    }
    if ( ptr->utxos != 0 )
        free(ptr->utxos);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}

int32_t NSPV_getaddresstxids(struct NSPV_txidsresp *ptr,char *coinaddr,bool isCC,int32_t skipcount)
{
    int32_t maxlen,txheight,ind=0,n = 0,len = 0;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txids;
    SetCCtxids(txids,coinaddr,isCC);
    ptr->nodeheight = chainActive.LastTip()->GetHeight();
    maxlen = MAX_BLOCK_SIZE(ptr->nodeheight) - 512;
    maxlen /= sizeof(*ptr->txids);
    strncpy(ptr->coinaddr,coinaddr,sizeof(ptr->coinaddr)-1);
    ptr->CCflag = isCC;
    if ( skipcount < 0 )
        skipcount = 0;
    if ( (ptr->numtxids= (int32_t)txids.size()) >= 0 && ptr->numtxids < maxlen )
    {
        // scan mempool add to end
        if ( skipcount >= ptr->numtxids )
            skipcount = ptr->numtxids-1;
        ptr->skipcount = skipcount;
        ptr->txids = (struct NSPV_txidresp *)calloc(ptr->numtxids-skipcount,sizeof(*ptr->txids));
        for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it=txids.begin(); it!=txids.end(); it++)
        {
            if ( n >= skipcount )
            {
                ptr->txids[ind].txid = it->first.txhash;
                ptr->txids[ind].vout = (int32_t)it->first.index;
                ptr->txids[ind].satoshis = (int64_t)it->second;
                ptr->txids[ind].height = (int64_t)it->first.blockHeight;
                ind++;
            }
            n++;
        }
        ptr->numtxids = ind;
        len = (int32_t)(sizeof(*ptr) + sizeof(*ptr->txids)*ptr->numtxids - sizeof(ptr->txids));
        return(len);
    }
    if ( ptr->txids != 0 )
        free(ptr->txids);
    memset(ptr,0,sizeof(*ptr));
    return(0);
}

uint8_t *NSPV_getrawtx(CTransaction &tx,uint256 &hashBlock,int32_t *txlenp,uint256 txid)
{
    uint8_t *rawtx = 0;
    *txlenp = 0;
    {
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
    CTransaction tx;
    ptr->retcode = 0;
    if ( NSPV_txextract(tx,data,n) == 0 )
    {
        //LOCK(cs_main);
        ptr->txid = tx.GetHash();
        //fprintf(stderr,"try to addmempool transaction %s\n",ptr->txid.GetHex().c_str());
        if ( myAddtomempool(tx) != 0 )
        {
            ptr->retcode = 1;
            //int32_t i;
            //for (i=0; i<n; i++)
            //    fprintf(stderr,"%02x",data[i]);
            fprintf(stderr," relay transaction %s retcode.%d\n",ptr->txid.GetHex().c_str(),ptr->retcode);
            RelayTransaction(tx);
        } else ptr->retcode = -3;

    } else ptr->retcode = -1;
    return(sizeof(*ptr));
}

int32_t NSPV_gettxproof(struct NSPV_txproof *ptr,int32_t vout,uint256 txid,int32_t height)
{
    int32_t flag = 0,len = 0; CTransaction _tx; uint256 hashBlock; CBlock block; CBlockIndex *pindex;
    if ( (ptr->tx= NSPV_getrawtx(_tx,hashBlock,&ptr->txlen,txid)) == 0 )
        return(-1);
    ptr->txid = txid;
    ptr->vout = vout;
    ptr->height = height;
    if ( height != 0 && (pindex= komodo_chainactive(height)) != 0 && komodo_blockload(block,pindex) == 0 )
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
            //fprintf(stderr,"%s txproof.(%s)\n",txid.GetHex().c_str(),HexStr(proof).c_str());
            if ( ptr->txprooflen > 0 )
            {
                ptr->txproof = (uint8_t *)calloc(1,ptr->txprooflen);
                memcpy(ptr->txproof,&proof[0],ptr->txprooflen);
            }
            //fprintf(stderr,"gettxproof slen.%d\n",(int32_t)(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen));
        }
    }
    ptr->unspentvalue = CCgettxout(txid,vout,1,1);
    return(sizeof(*ptr) - sizeof(ptr->tx) - sizeof(ptr->txproof) + ptr->txlen + ptr->txprooflen);
}

int32_t NSPV_getntzsproofresp(struct NSPV_ntzsproofresp *ptr,uint256 prevntztxid,uint256 nextntztxid)
{
    int32_t i; uint256 hashBlock,bhash0,bhash1,desttxid0,desttxid1; CTransaction tx;
    ptr->prevtxid = prevntztxid;
    ptr->prevntz = NSPV_getrawtx(tx,hashBlock,&ptr->prevtxlen,ptr->prevtxid);
    ptr->prevtxidht = komodo_blockheight(hashBlock);
    if ( NSPV_notarizationextract(0,&ptr->common.prevht,&bhash0,&desttxid0,tx) < 0 )
        return(-2);
    else if ( komodo_blockheight(bhash0) != ptr->common.prevht )
        return(-3);
    
    ptr->nexttxid = nextntztxid;
    ptr->nextntz = NSPV_getrawtx(tx,hashBlock,&ptr->nexttxlen,ptr->nexttxid);
    ptr->nexttxidht = komodo_blockheight(hashBlock);
    if ( NSPV_notarizationextract(0,&ptr->common.nextht,&bhash1,&desttxid1,tx) < 0 )
        return(-5);
    else if ( komodo_blockheight(bhash1) != ptr->common.nextht )
        return(-6);

    else if ( ptr->common.prevht > ptr->common.nextht || (ptr->common.nextht - ptr->common.prevht) > 1440 )
    {
        fprintf(stderr,"illegal prevht.%d nextht.%d\n",ptr->common.prevht,ptr->common.nextht);
        return(-7);
    }
    //fprintf(stderr,"%s -> prevht.%d, %s -> nexht.%d\n",ptr->prevtxid.GetHex().c_str(),ptr->common.prevht,ptr->nexttxid.GetHex().c_str(),ptr->common.nextht);
    ptr->common.numhdrs = (ptr->common.nextht - ptr->common.prevht + 1);
    ptr->common.hdrs = (struct NSPV_equihdr *)calloc(ptr->common.numhdrs,sizeof(*ptr->common.hdrs));
    //fprintf(stderr,"prev.%d next.%d allocate numhdrs.%d\n",prevht,nextht,ptr->common.numhdrs);
    for (i=0; i<ptr->common.numhdrs; i++)
    {
        //hashBlock = NSPV_hdrhash(&ptr->common.hdrs[i]);
        //fprintf(stderr,"hdr[%d] %s\n",prevht+i,hashBlock.GetHex().c_str());
        if ( NSPV_setequihdr(&ptr->common.hdrs[i],ptr->common.prevht+i) < 0 )
        {
            fprintf(stderr,"error setting hdr.%d\n",ptr->common.prevht+i);
            free(ptr->common.hdrs);
            ptr->common.hdrs = 0;
            return(-1);
        }
    }
    //fprintf(stderr,"sizeof ptr %ld, common.%ld lens.%d %d\n",sizeof(*ptr),sizeof(ptr->common),ptr->prevtxlen,ptr->nexttxlen);
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
        if ( NSPV_gettxproof(&ptr->spent,0,ptr->spent.txid,ptr->spent.height) > 0 )
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
    int32_t len,slen,ind,reqheight; std::vector<uint8_t> response; uint32_t timestamp = (uint32_t)time(NULL);
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
                if ( len == 1+sizeof(reqheight) )
                    iguana_rwnum(0,&request[1],sizeof(reqheight),&reqheight);
                else reqheight = 0;
                //fprintf(stderr,"request height.%d\n",reqheight);
                memset(&I,0,sizeof(I));
                if ( (slen= NSPV_getinfo(&I,reqheight)) > 0 )
                {
                    response.resize(1 + slen);
                    response[0] = NSPV_INFORESP;
                    //fprintf(stderr,"slen.%d\n",slen);
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
                if ( len < 64 && (request[1] == len-3 || request[1] == len-7) )
                {
                    int32_t skipcount = 0; uint8_t isCC = 0;
                    memcpy(coinaddr,&request[2],request[1]);
                    coinaddr[request[1]] = 0;
                    if ( request[1] == len-3 )
                        isCC = (request[len-1] != 0);
                    else
                    {
                        isCC = (request[len-5] != 0);
                        iguana_rwnum(0,&request[len-4],sizeof(skipcount),&skipcount);
                    }
                    if ( isCC != 0 )
                        fprintf(stderr,"%s isCC.%d skipcount.%d\n",coinaddr,isCC,skipcount);
                    memset(&U,0,sizeof(U));
                    if ( (slen= NSPV_getaddressutxos(&U,coinaddr,isCC,skipcount)) > 0 )
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
        else if ( request[0] == NSPV_TXIDS )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_txidsresp T; char coinaddr[64];
                if ( len < 64+5 && (request[1] == len-3 || request[1] == len-7) )
                {
                    int32_t skipcount = 0; uint8_t isCC = 0;
                    memcpy(coinaddr,&request[2],request[1]);
                    coinaddr[request[1]] = 0;
                    if ( request[1] == len-3 )
                        isCC = (request[len-1] != 0);
                    else
                    {
                        isCC = (request[len-5] != 0);
                        iguana_rwnum(0,&request[len-4],sizeof(skipcount),&skipcount);
                    }
                    //if ( isCC != 0 )
                        fprintf(stderr,"%s isCC.%d skipcount.%d\n",coinaddr,isCC,skipcount);
                    memset(&T,0,sizeof(T));
                    if ( (slen= NSPV_getaddresstxids(&T,coinaddr,isCC,skipcount)) > 0 )
                    {
fprintf(stderr,"slen.%d\n",slen);
                        response.resize(1 + slen);
                        response[0] = NSPV_TXIDSRESP;
                        if ( NSPV_rwtxidsresp(1,&response[1],&T) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_txidsresp_purge(&T);
                    }
                } else fprintf(stderr,"len.%d req1.%d\n",len,request[1]);
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
                struct NSPV_ntzsproofresp P; uint256 prevntz,nextntz;
                if ( len == 1+sizeof(prevntz)+sizeof(nextntz) )
                {
                    iguana_rwbignum(0,&request[1],sizeof(prevntz),(uint8_t *)&prevntz);
                    iguana_rwbignum(0,&request[1+sizeof(prevntz)],sizeof(nextntz),(uint8_t *)&nextntz);
                    memset(&P,0,sizeof(P));
                    if ( (slen= NSPV_getntzsproofresp(&P,prevntz,nextntz)) > 0 )
                    {
                        // fprintf(stderr,"slen.%d msg prev.%s next.%s\n",slen,prevntz.GetHex().c_str(),nextntz.GetHex().c_str());
                        response.resize(1 + slen);
                        response[0] = NSPV_NTZSPROOFRESP;
                        if ( NSPV_rwntzsproofresp(1,&response[1],&P) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->prevtimes[ind] = timestamp;
                        }
                        NSPV_ntzsproofresp_purge(&P);
                    } else fprintf(stderr,"err.%d\n",slen);
                }
            }
        }
        else if ( request[0] == NSPV_TXPROOF )
        {
            if ( timestamp > pfrom->prevtimes[ind] )
            {
                struct NSPV_txproof P; uint256 txid; int32_t height,vout;
                if ( len == 1+sizeof(txid)+sizeof(height)+sizeof(vout) )
                {
                    iguana_rwnum(0,&request[1],sizeof(height),&height);
                    iguana_rwnum(0,&request[1+sizeof(height)],sizeof(vout),&vout);
                    iguana_rwbignum(0,&request[1+sizeof(height)+sizeof(vout)],sizeof(txid),(uint8_t *)&txid);
                    //fprintf(stderr,"got txid %s/v%d ht.%d\n",txid.GetHex().c_str(),vout,height);
                    memset(&P,0,sizeof(P));
                    if ( (slen= NSPV_gettxproof(&P,vout,txid,height)) > 0 )
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
                struct NSPV_broadcastresp B; uint32_t n,offset; uint256 txid;
                if ( len > 1+sizeof(txid)+sizeof(n) )
                {
                    iguana_rwbignum(0,&request[1],sizeof(txid),(uint8_t *)&txid);
                    iguana_rwnum(0,&request[1+sizeof(txid)],sizeof(n),&n);
                    memset(&B,0,sizeof(B));
                    offset = 1 + sizeof(txid) + sizeof(n);
                    if ( n < MAX_TX_SIZE_AFTER_SAPLING && request.size() == offset+n && (slen= NSPV_sendrawtransaction(&B,&request[offset],n)) > 0 )
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
