
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

// todo:
// spentinfo via CC

// headers "sync" make sure it connects to prior blocks to notarization. use getinfo hdrht to get missing hdrs

// make sure to sanity check all vector lengths on receipt
// make sure no files are updated (this is to allow nSPV=1 and later nSPV=0 without affecting database)
// bug: under load, fullnode was returning all 0 nServices

#ifndef KOMODO_NSPV_H
#define KOMODO_NSPV_H

int32_t iguana_rwbuf(int32_t rwflag,uint8_t *serialized,int32_t len,uint8_t *buf)
{
    if ( rwflag != 0 )
        memcpy(serialized,buf,len);
    else memcpy(buf,serialized,len);
    return(len);
}

int32_t NSPV_rwequihdr(int32_t rwflag,uint8_t *serialized,struct NSPV_equihdr *ptr)
{
    int32_t len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nVersion),&ptr->nVersion);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashPrevBlock),(uint8_t *)&ptr->hashPrevBlock);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashMerkleRoot),(uint8_t *)&ptr->hashMerkleRoot);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashFinalSaplingRoot),(uint8_t *)&ptr->hashFinalSaplingRoot);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nTime),&ptr->nTime);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nBits),&ptr->nBits);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->nNonce),(uint8_t *)&ptr->nNonce);
    len += iguana_rwbuf(rwflag,&serialized[len],sizeof(ptr->nSolution),ptr->nSolution);
    return(len);
}

int32_t iguana_rwequihdrvec(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,struct NSPV_equihdr **ptrp)
{
    int32_t i,vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        //fprintf(stderr,"vsize.%d ptrp.%p alloc %ld\n",vsize,*ptrp,sizeof(struct NSPV_equihdr)*vsize);
        if ( *ptrp == 0 )
            *ptrp = (struct NSPV_equihdr *)calloc(sizeof(struct NSPV_equihdr),vsize); // relies on uint16_t being "small" to prevent mem exhaustion
        for (i=0; i<vsize; i++)
            len += NSPV_rwequihdr(rwflag,&serialized[len],&(*ptrp)[i]);
    }
    return(len);
}

int32_t iguana_rwuint8vec(int32_t rwflag,uint8_t *serialized,int32_t *biglenp,uint8_t **ptrp)
{
    int32_t vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*biglenp),biglenp);
    if ( (vsize= *biglenp) > 0 && vsize < MAX_TX_SIZE_AFTER_SAPLING )
    {
        if ( *ptrp == 0 )
            *ptrp = (uint8_t *)calloc(1,vsize);
        len += iguana_rwbuf(rwflag,&serialized[len],vsize,*ptrp);
    }
    return(len);
}

int32_t NSPV_rwutxoresp(int32_t rwflag,uint8_t *serialized,struct NSPV_utxoresp *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->satoshis),&ptr->satoshis);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->extradata),&ptr->extradata);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    return(len);
}

int32_t NSPV_rwutxosresp(int32_t rwflag,uint8_t *serialized,struct NSPV_utxosresp *ptr) // check mempool
{
    int32_t i,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->numutxos),&ptr->numutxos);
    if ( ptr->numutxos != 0 )
    {
        if ( ptr->utxos == 0 )
            ptr->utxos = (struct NSPV_utxoresp *)calloc(sizeof(*ptr->utxos),ptr->numutxos); // relies on uint16_t being "small" to prevent mem exhaustion
        for (i=0; i<ptr->numutxos; i++)
            len += NSPV_rwutxoresp(rwflag,&serialized[len],&ptr->utxos[i]);
    }
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->total),&ptr->total);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->interest),&ptr->interest);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nodeheight),&ptr->nodeheight);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->filter),&ptr->filter);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->CCflag),&ptr->CCflag);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->skipcount),&ptr->skipcount);
    if ( rwflag != 0 )
    {
        memcpy(&serialized[len],ptr->coinaddr,sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    else
    {
        memcpy(ptr->coinaddr,&serialized[len],sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    return(len);
}

void NSPV_utxosresp_purge(struct NSPV_utxosresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->utxos != 0 )
            free(ptr->utxos);
        memset(ptr,0,sizeof(*ptr));
    }
}

void NSPV_utxosresp_copy(struct NSPV_utxosresp *dest,struct NSPV_utxosresp *ptr)
{
    *dest = *ptr;
    if ( ptr->utxos != 0 )
    {
        dest->utxos = (struct NSPV_utxoresp *)malloc(ptr->numutxos * sizeof(*ptr->utxos));
        memcpy(dest->utxos,ptr->utxos,ptr->numutxos * sizeof(*ptr->utxos));
    }
}

int32_t NSPV_rwtxidresp(int32_t rwflag,uint8_t *serialized,struct NSPV_txidresp *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->satoshis),&ptr->satoshis);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    return(len);
}

int32_t NSPV_rwtxidsresp(int32_t rwflag,uint8_t *serialized,struct NSPV_txidsresp *ptr)
{
    int32_t i,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->numtxids),&ptr->numtxids);
    if ( ptr->numtxids != 0 )
    {
        if ( ptr->txids == 0 )
            ptr->txids = (struct NSPV_txidresp *)calloc(sizeof(*ptr->txids),ptr->numtxids);
        for (i=0; i<ptr->numtxids; i++)
            len += NSPV_rwtxidresp(rwflag,&serialized[len],&ptr->txids[i]);
    }
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nodeheight),&ptr->nodeheight);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->filter),&ptr->filter);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->CCflag),&ptr->CCflag);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->skipcount),&ptr->skipcount);
    if ( rwflag != 0 )
    {
        memcpy(&serialized[len],ptr->coinaddr,sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    else
    {
        memcpy(ptr->coinaddr,&serialized[len],sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
//fprintf(stderr,"rwlen.%d\n",len);
    return(len);
}

void NSPV_txidsresp_purge(struct NSPV_txidsresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->txids != 0 )
            free(ptr->txids);
        memset(ptr,0,sizeof(*ptr));
    }
}

void NSPV_txidsresp_copy(struct NSPV_txidsresp *dest,struct NSPV_txidsresp *ptr)
{
    *dest = *ptr;
    if ( ptr->txids != 0 )
    {
        dest->txids = (struct NSPV_txidresp *)malloc(ptr->numtxids * sizeof(*ptr->txids));
        memcpy(dest->txids,ptr->txids,ptr->numtxids * sizeof(*ptr->txids));
    }
}

int32_t NSPV_rwmempoolresp(int32_t rwflag,uint8_t *serialized,struct NSPV_mempoolresp *ptr)
{
    int32_t i,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->numtxids),&ptr->numtxids);
    if ( ptr->numtxids != 0 )
    {
        if ( ptr->txids == 0 )
            ptr->txids = (uint256 *)calloc(sizeof(*ptr->txids),ptr->numtxids);
        for (i=0; i<ptr->numtxids; i++)
            len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txids[i]),(uint8_t *)&ptr->txids[i]);
    }
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nodeheight),&ptr->nodeheight);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vindex),&ptr->vindex);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->CCflag),&ptr->CCflag);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->funcid),&ptr->funcid);
    if ( rwflag != 0 )
    {
        memcpy(&serialized[len],ptr->coinaddr,sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    else
    {
        memcpy(ptr->coinaddr,&serialized[len],sizeof(ptr->coinaddr));
        len += sizeof(ptr->coinaddr);
    }
    //fprintf(stderr,"NSPV_rwmempoolresp rwlen.%d\n",len);
    return(len);
}

void NSPV_mempoolresp_purge(struct NSPV_mempoolresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->txids != 0 )
            free(ptr->txids);
        memset(ptr,0,sizeof(*ptr));
    }
}

void NSPV_mempoolresp_copy(struct NSPV_mempoolresp *dest,struct NSPV_mempoolresp *ptr)
{
    *dest = *ptr;
    if ( ptr->txids != 0 )
    {
        dest->txids = (uint256 *)malloc(ptr->numtxids * sizeof(*ptr->txids));
        memcpy(dest->txids,ptr->txids,ptr->numtxids * sizeof(*ptr->txids));
    }
}

int32_t NSPV_rwntz(int32_t rwflag,uint8_t *serialized,struct NSPV_ntz *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->othertxid),(uint8_t *)&ptr->othertxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->txidheight),&ptr->txidheight);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->timestamp),&ptr->timestamp);
    return(len);
}

int32_t NSPV_rwntzsresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->prevntz);
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->nextntz);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->reqheight),&ptr->reqheight);
    return(len);
}

void NSPV_ntzsresp_copy(struct NSPV_ntzsresp *dest,struct NSPV_ntzsresp *ptr)
{
    *dest = *ptr;
}

void NSPV_ntzsresp_purge(struct NSPV_ntzsresp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

int32_t NSPV_rwinforesp(int32_t rwflag,uint8_t *serialized,struct NSPV_inforesp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->notarization);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->hdrheight),&ptr->hdrheight);
    len += NSPV_rwequihdr(rwflag,&serialized[len],&ptr->H);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->version),&ptr->version);
//fprintf(stderr,"getinfo rwlen.%d\n",len);
    return(len);
}

void NSPV_inforesp_purge(struct NSPV_inforesp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

int32_t NSPV_rwtxproof(int32_t rwflag,uint8_t *serialized,struct NSPV_txproof *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->unspentvalue),&ptr->unspentvalue);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txlen,&ptr->tx);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txprooflen,&ptr->txproof);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->hashblock),(uint8_t *)&ptr->hashblock);
    return(len);
}

void NSPV_txproof_copy(struct NSPV_txproof *dest,struct NSPV_txproof *ptr)
{
    *dest = *ptr;
    if ( ptr->tx != 0 && ptr->txlen < MAX_TX_SIZE_AFTER_SAPLING )
    {
        dest->tx = (uint8_t *)malloc(ptr->txlen);
        memcpy(dest->tx,ptr->tx,ptr->txlen);
    }
    if ( ptr->txproof != 0 )
    {
        dest->txproof = (uint8_t *)malloc(ptr->txprooflen);
        memcpy(dest->txproof,ptr->txproof,ptr->txprooflen);
    }
}

void NSPV_txproof_purge(struct NSPV_txproof *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->tx != 0 )
            free(ptr->tx);
        if ( ptr->txproof != 0 )
            free(ptr->txproof);
        memset(ptr,0,sizeof(*ptr));
    }
}

int32_t NSPV_rwntzproofshared(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzproofshared *ptr)
{
    int32_t len = 0;
    len += iguana_rwequihdrvec(rwflag,&serialized[len],&ptr->numhdrs,&ptr->hdrs);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevht),&ptr->prevht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nextht),&ptr->nextht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad16),&ptr->pad16);
    //fprintf(stderr,"rwcommon prev.%d next.%d\n",ptr->prevht,ptr->nextht);
    return(len);
}

int32_t NSPV_rwntzsproofresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsproofresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntzproofshared(rwflag,&serialized[len],&ptr->common);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->prevtxid),(uint8_t *)&ptr->prevtxid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->nexttxid),(uint8_t *)&ptr->nexttxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevtxidht),&ptr->prevtxidht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nexttxidht),&ptr->nexttxidht);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->prevtxlen,&ptr->prevntz);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->nexttxlen,&ptr->nextntz);
    //fprintf(stderr,"retlen.%d\n",len);
    return(len);
}

void NSPV_ntzsproofresp_copy(struct NSPV_ntzsproofresp *dest,struct NSPV_ntzsproofresp *ptr)
{
    *dest = *ptr;
    if ( ptr->common.hdrs != 0 )
    {
        dest->common.hdrs = (struct NSPV_equihdr *)malloc(ptr->common.numhdrs * sizeof(*ptr->common.hdrs));
        memcpy(dest->common.hdrs,ptr->common.hdrs,ptr->common.numhdrs * sizeof(*ptr->common.hdrs));
    }
    if ( ptr->prevntz != 0 )
    {
        dest->prevntz = (uint8_t *)malloc(ptr->prevtxlen);
        memcpy(dest->prevntz,ptr->prevntz,ptr->prevtxlen);
    }
    if ( ptr->nextntz != 0 )
    {
        dest->nextntz = (uint8_t *)malloc(ptr->nexttxlen);
        memcpy(dest->nextntz,ptr->nextntz,ptr->nexttxlen);
    }
}

void NSPV_ntzsproofresp_purge(struct NSPV_ntzsproofresp *ptr)
{
    if ( ptr != 0 )
    {
        if ( ptr->common.hdrs != 0 )
            free(ptr->common.hdrs);
        if ( ptr->prevntz != 0 )
            free(ptr->prevntz);
        if ( ptr->nextntz != 0 )
            free(ptr->nextntz);
        memset(ptr,0,sizeof(*ptr));
    }
}

int32_t NSPV_rwspentinfo(int32_t rwflag,uint8_t *serialized,struct NSPV_spentinfo *ptr) // check mempool
{
    int32_t len = 0;
    len += NSPV_rwtxproof(rwflag,&serialized[len],&ptr->spent);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->spentvini),&ptr->spentvini);
    return(len);
}

void NSPV_spentinfo_purge(struct NSPV_spentinfo *ptr)
{
    if ( ptr != 0 )
    {
        NSPV_txproof_purge(&ptr->spent);
        memset(ptr,0,sizeof(*ptr));
    }
}

int32_t NSPV_rwbroadcastresp(int32_t rwflag,uint8_t *serialized,struct NSPV_broadcastresp *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->retcode),&ptr->retcode);
    return(len);
}

void NSPV_broadcast_purge(struct NSPV_broadcastresp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

int32_t NSPV_rwremoterpcresp(int32_t rwflag,uint8_t *serialized,struct NSPV_remoterpcresp *ptr, int32_t slen)
{
    int32_t len = 0;
    len+=iguana_rwbuf(rwflag,&serialized[len],sizeof(ptr->method),(uint8_t*)ptr->method);
    len+=iguana_rwbuf(rwflag,&serialized[len],slen-len,(uint8_t*)ptr->json);
    return(len);
}

void NSPV_remoterpc_purge(struct NSPV_remoterpcresp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

// useful utility functions

uint256 NSPV_doublesha256(uint8_t *data,int32_t datalen)
{
    bits256 _hash; uint256 hash; int32_t i;
    _hash = bits256_doublesha256(0,data,datalen);
    for (i=0; i<32; i++)
        ((uint8_t *)&hash)[i] = _hash.bytes[31 - i];
    return(hash);
}

uint256 NSPV_hdrhash(struct NSPV_equihdr *hdr)
{
    CBlockHeader block;
    block.nVersion = hdr->nVersion;
    block.hashPrevBlock = hdr->hashPrevBlock;
    block.hashMerkleRoot = hdr->hashMerkleRoot;
    block.hashFinalSaplingRoot = hdr->hashFinalSaplingRoot;
    block.nTime = hdr->nTime;
    block.nBits = hdr->nBits;
    block.nNonce = hdr->nNonce;
    block.nSolution.resize(sizeof(hdr->nSolution));
    memcpy(&block.nSolution[0],hdr->nSolution,sizeof(hdr->nSolution));
    return(block.GetHash());
}

int32_t NSPV_txextract(CTransaction &tx,uint8_t *data,int32_t datalen)
{
    std::vector<uint8_t> rawdata;
    if ( datalen < MAX_TX_SIZE_AFTER_SAPLING )
    {
        rawdata.resize(datalen);
        memcpy(&rawdata[0],data,datalen);
        if ( DecodeHexTx(tx,HexStr(rawdata)) != 0 )
            return(0);
    }
    return(-1);
}

bool NSPV_SignTx(CMutableTransaction &mtx,int32_t vini,int64_t utxovalue,const CScript scriptPubKey,uint32_t nTime);

int32_t NSPV_fastnotariescount(CTransaction tx,uint8_t elected[64][33],uint32_t nTime)
{
    CPubKey pubkeys[64]; uint8_t sig[512]; CScript scriptPubKeys[64]; CMutableTransaction mtx(tx); int32_t vini,j,siglen,retval; uint64_t mask = 0; char *str; std::vector<std::vector<unsigned char>> vData;
    for (j=0; j<64; j++)
    {
        pubkeys[j] = buf2pk(elected[j]);
        scriptPubKeys[j] = (CScript() << ParseHex(HexStr(pubkeys[j])) << OP_CHECKSIG);
        //fprintf(stderr,"%d %s\n",j,HexStr(pubkeys[j]).c_str());
    }
    fprintf(stderr,"txid %s\n",tx.GetHash().GetHex().c_str());
    //for (vini=0; vini<tx.vin.size(); vini++)
    //    mtx.vin[vini].scriptSig.resize(0);
    for (vini=0; vini<tx.vin.size(); vini++)
    {
        CScript::const_iterator pc = tx.vin[vini].scriptSig.begin();
        if ( tx.vin[vini].scriptSig.GetPushedData(pc,vData) != 0 )
        {
            vData[0].pop_back();
            for (j=0; j<64; j++)
            {
                if ( ((1LL << j) & mask) != 0 )
                    continue;
                char coinaddr[64]; Getscriptaddress(coinaddr,scriptPubKeys[j]);
                NSPV_SignTx(mtx,vini,10000,scriptPubKeys[j],nTime); // sets SIG_TXHASH
                //fprintf(stderr,"%s ",SIG_TXHASH.GetHex().c_str());
                if ( (retval= pubkeys[j].Verify(SIG_TXHASH,vData[0])) != 0 )
                {
                    //fprintf(stderr,"(vini.%d %s.%d) ",vini,coinaddr,retval);
                    mask |= (1LL << j);
                    break;
                }
            }
            //fprintf(stderr," vini.%d verified %llx\n",vini,(long long)mask);
        }
    }
    return(bitweight(mask));
}

/*
 NSPV_notariescount is the slowest process during full validation as it requires looking up 13 transactions.
 one way that would be 10000x faster would be to bruteforce validate the signatures in each vin, against all 64 pubkeys! for a valid tx, that is on average 13*32 secp256k1/sapling verify operations, which is much faster than even a single network request.
 Unfortunately, due to the complexity of calculating the hash to sign for a tx, this bruteforcing would require determining what type of signature method and having sapling vs legacy methods of calculating the txhash.
 It could be that the fullnode side could calculate this and send it back to the superlite side as any hash that would validate 13 different ways has to be the valid txhash.
 However, since the vouts being spent by the notaries are highly constrained p2pk vouts, the txhash can be deduced if a specific notary pubkey is indeed the signer
 */
int32_t NSPV_notariescount(CTransaction tx,uint8_t elected[64][33])
{
    uint8_t *script; CTransaction vintx; int64_t rewardsum = 0; int32_t i,j,utxovout,scriptlen,numsigs = 0,txheight,currentheight; uint256 hashBlock;
    for (i=0; i<tx.vin.size(); i++)
    {
        utxovout = tx.vin[i].prevout.n;
        if ( NSPV_gettransaction(1,utxovout,tx.vin[i].prevout.hash,0,vintx,hashBlock,txheight,currentheight,-1,0,rewardsum) != 0 )
        {
            fprintf(stderr,"error getting %s/v%d\n",tx.vin[i].prevout.hash.GetHex().c_str(),utxovout);
            return(numsigs);
        }
        if ( utxovout < vintx.vout.size() )
        {
            script = (uint8_t *)&vintx.vout[utxovout].scriptPubKey[0];
            if ( (scriptlen= vintx.vout[utxovout].scriptPubKey.size()) == 35 )
            {
                for (j=0; j<64; j++)
                    if ( memcmp(&script[1],elected[j],33) == 0 )
                    {
                        numsigs++;
                        break;
                    }
            } else fprintf(stderr,"invalid scriptlen.%d\n",scriptlen);
        } else fprintf(stderr,"invalid utxovout.%d vs %d\n",utxovout,(int32_t)vintx.vout.size());
    }
    return(numsigs);
}

uint256 NSPV_opretextract(int32_t *heightp,uint256 *blockhashp,char *symbol,std::vector<uint8_t> opret,uint256 txid)
{
    uint256 desttxid; int32_t i;
    iguana_rwnum(0,&opret[32],sizeof(*heightp),heightp);
    for (i=0; i<32; i++)
        ((uint8_t *)blockhashp)[i] = opret[i];
    for (i=0; i<32; i++)
        ((uint8_t *)&desttxid)[i] = opret[4 + 32 + i];
    if ( 0 && *heightp != 2690 )
        fprintf(stderr," ntzht.%d %s <- txid.%s size.%d\n",*heightp,(*blockhashp).GetHex().c_str(),(txid).GetHex().c_str(),(int32_t)opret.size());
    return(desttxid);
}

int32_t NSPV_notarizationextract(int32_t verifyntz,int32_t *ntzheightp,uint256 *blockhashp,uint256 *desttxidp,CTransaction tx)
{
    int32_t numsigs=0; uint8_t elected[64][33]; char *symbol; std::vector<uint8_t> opret; uint32_t nTime;
    if ( tx.vout.size() >= 2 )
    {
        symbol = (ASSETCHAINS_SYMBOL[0] == 0) ? (char *)"KMD" : ASSETCHAINS_SYMBOL;
        GetOpReturnData(tx.vout[1].scriptPubKey,opret);
        if ( opret.size() >= 32*2+4 )
        {
            //sleep(1); // needed to avoid no pnodes error
            *desttxidp = NSPV_opretextract(ntzheightp,blockhashp,symbol,opret,tx.GetHash());
            nTime = NSPV_blocktime(*ntzheightp);
            komodo_notaries(elected,*ntzheightp,nTime);
            if ( verifyntz != 0 && (numsigs= NSPV_fastnotariescount(tx,elected,nTime)) < 12 )
            {
                fprintf(stderr,"numsigs.%d error\n",numsigs);
                return(-3);
            } 
            return(0);
        }
        else
        {
            fprintf(stderr,"opretsize.%d error\n",(int32_t)opret.size());
            return(-2);
        }
    } else return(-1);
}
#endif // KOMODO_NSPV_H
