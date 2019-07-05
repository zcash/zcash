
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
// make sure no files are updated (this is to allow nSPV=1 and later nSPV=0 without affecting database)
// validate proofs
// make sure to sanity check all vector lengths on receipt
// determine if it makes sense to be scanning mempool for the utxo/spentinfo requests

#ifndef KOMODO_NSPV_H
#define KOMODO_NSPV_H

#define NSPV_INFO 0x00
#define NSPV_INFORESP 0x01
#define NSPV_UTXOS 0x02
#define NSPV_UTXOSRESP 0x03
#define NSPV_NTZS 0x04
#define NSPV_NTZSRESP 0x05
#define NSPV_NTZSPROOF 0x06
#define NSPV_NTZSPROOFRESP 0x07
#define NSPV_TXPROOF 0x08
#define NSPV_TXPROOFRESP 0x09
#define NSPV_SPENTINFO 0x0a
#define NSPV_SPENTINFORESP 0x0b

int32_t iguana_rwbuf(int32_t rwflag,uint8_t *serialized,uint16_t len,uint8_t *buf)
{
    if ( rwflag != 0 )
        memcpy(serialized,buf,len);
    else memcpy(buf,serialized,len);
    return(len);
}

struct NSPV_equihdr
{
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint256 hashFinalSaplingRoot;
    uint32_t nTime;
    uint32_t nBits;
    uint256 nNonce;
    uint8_t nSolution[1344];
};

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

int32_t iguana_rwuint8vec(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,uint8_t **ptrp)
{
    int32_t vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        if ( *ptrp == 0 )
            *ptrp = (uint8_t *)calloc(1,vsize); // relies on uint16_t being "small" to prevent mem exhaustion
        len += iguana_rwbuf(rwflag,&serialized[len],vsize,*ptrp);
    }
    return(len);
}

struct NSPV_utxoresp
{
    uint256 txid;
    int64_t satoshis,extradata;
    int32_t vout,height;
};

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

struct NSPV_utxosresp
{
    struct NSPV_utxoresp *utxos;
    int64_t total,interest;
    int32_t nodeheight;
    uint16_t numutxos,pad16;
};

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
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad16),&ptr->pad16);
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

struct NSPV_ntz
{
    uint256 blockhash,txid,othertxid;
    int32_t height,txidheight;
};

int32_t NSPV_rwntz(int32_t rwflag,uint8_t *serialized,struct NSPV_ntz *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->othertxid),(uint8_t *)&ptr->othertxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->txidheight),&ptr->txidheight);
    return(len);
}

struct NSPV_ntzsresp
{
    struct NSPV_ntz prevntz,nextntz;
};

int32_t NSPV_rwntzsresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->prevntz);
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->nextntz);
    return(len);
}

void NSPV_ntzsresp_purge(struct NSPV_ntzsresp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

struct NSPV_inforesp
{
    struct NSPV_ntz notarization;
    uint256 blockhash;
    int32_t height,pad32;
};

int32_t NSPV_rwinforesp(int32_t rwflag,uint8_t *serialized,struct NSPV_inforesp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntz(rwflag,&serialized[len],&ptr->notarization);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->blockhash),(uint8_t *)&ptr->blockhash);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    return(len);
}

void NSPV_inforesp_purge(struct NSPV_inforesp *ptr)
{
    if ( ptr != 0 )
        memset(ptr,0,sizeof(*ptr));
}

struct NSPV_txproof
{
    uint256 txid;
    int32_t height;
    uint16_t txlen,txprooflen;
    uint8_t *tx,*txproof;
};

int32_t NSPV_rwtxproof(int32_t rwflag,uint8_t *serialized,struct NSPV_txproof *ptr)
{
    int32_t len = 0;
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txlen,&ptr->tx);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->txprooflen,&ptr->txproof);
    return(len);
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

struct NSPV_utxo
{
    struct NSPV_txproof T;
    int64_t satoshis,extradata;
    int32_t vout,prevht,nextht,pad32;
};

int32_t NSPV_rwutxo(int32_t rwflag,uint8_t *serialized,struct NSPV_utxo *ptr)
{
    int32_t len = 0;
    len += NSPV_rwtxproof(rwflag,&serialized[len],&ptr->T);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->satoshis),&ptr->satoshis);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->extradata),&ptr->extradata);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->vout),&ptr->vout);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevht),&ptr->prevht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nextht),&ptr->nextht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    return(len);
}

struct NSPV_ntzproofshared
{
    struct NSPV_equihdr *hdrs;
    int32_t prevht,nextht,pad32;
    uint16_t numhdrs,pad16;
};

int32_t NSPV_rwntzproofshared(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzproofshared *ptr)
{
    int32_t len = 0;
    len += iguana_rwequihdrvec(rwflag,&serialized[len],&ptr->numhdrs,&ptr->hdrs);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevht),&ptr->prevht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nextht),&ptr->nextht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad16),&ptr->pad16);
    return(len);
}

struct NSPV_ntzsproofresp
{
    struct NSPV_ntzproofshared common;
    uint256 prevtxid,nexttxid;
    int32_t pad32,prevtxidht,nexttxidht;
    uint16_t prevtxlen,nexttxlen;
    uint8_t *prevntz,*nextntz;
};

int32_t NSPV_rwntzsproofresp(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsproofresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntzproofshared(rwflag,&serialized[len],&ptr->common);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->prevtxid),(uint8_t *)&ptr->prevtxid);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->nexttxid),(uint8_t *)&ptr->nexttxid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->prevtxidht),&ptr->prevtxidht);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->nexttxidht),&ptr->nexttxidht);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->prevtxlen,&ptr->prevntz);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->nexttxlen,&ptr->nextntz);
    return(len);
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

struct NSPV_MMRproof
{
    struct NSPV_ntzproofshared common;
    // tbd
};

struct NSPV_spentinfo
{
    struct NSPV_txproof spent;
    uint256 txid;
    int32_t vout,spentvini;
};

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

// on fullnode:
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
    if ( height != 2668 )
        fprintf(stderr,"scan.%d -> %s txidht.%d ntzht.%d\n",height,desttxids[0].GetHex().c_str(),txidht,ntzheights[0]);
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
    }
}

// nSPV client. VERY simplistic "single threaded" networking model. for production GUI best to multithread, etc.
#define NSPV_POLLITERS 15
#define NSPV_POLLMICROS 100000

CAmount AmountFromValue(const UniValue& value);
int32_t bitcoin_base58decode(uint8_t *data,char *coinaddr);

uint32_t NSPV_lastinfo,NSPV_logintime;
char NSPV_wifstr[64];
std::string NSPV_address;
struct NSPV_inforesp NSPV_inforesult;
struct NSPV_utxosresp NSPV_utxosresult;
struct NSPV_spentinfo NSPV_spentresult;
struct NSPV_ntzsresp NSPV_ntzsresult;
struct NSPV_ntzsproofresp NSPV_ntzsproofresult;
struct NSPV_txproof NSPV_txproofresult;

struct NSPV_utxo *NSPV_utxos;

CNode *NSPV_req(CNode *pnode,uint8_t *msg,int32_t len,uint64_t mask,int32_t ind)
{
    int32_t flag = 0; uint32_t timestamp = (uint32_t)time(NULL);
    if ( pnode == 0 )
    {
        LOCK(cs_vNodes);
        BOOST_FOREACH(CNode *ptr,vNodes)
        {
            if ( ptr->hSocket == INVALID_SOCKET )
                continue;
            if ( (ptr->nServices & mask) == mask && timestamp > ptr->prevtimes[ind] )
            {
                flag = 1;
                pnode = ptr;
                break;
            } // else fprintf(stderr,"nServices %llx vs mask %llx, t%u vs %u, ind.%d\n",(long long)ptr->nServices,(long long)mask,timestamp,ptr->prevtimes[ind],ind);
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
    }
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
    CKey key = DecodeSecret(wifstr);
    CPubKey pubkey = key.GetPubKey();
    //assert(key.VerifyPubKey(pubkey));
    CKeyID vchAddress = pubkey.GetID();
    NSPV_address = EncodeDestination(vchAddress);
    result.push_back(Pair("address",NSPV_address));
    result.push_back(Pair("pubkey",HexStr(pubkey)));
    result.push_back(Pair("wifprefix",(int64_t)data[0]));
    result.push_back(Pair("compressed",(int64_t)(data[len-5] == 1)));
    memset(data,0,sizeof(data));
    return(result);
}

UniValue NSPV_getinfo_json()
{
    uint8_t msg[64]; int32_t i,len = 0; struct NSPV_inforesp I;
    NSPV_inforesp_purge(&NSPV_inforesult);
    msg[len++] = NSPV_INFO;
    for (iters=0; iters<3; iters++)
    {
        fprintf(stderr,"issue getinfo\n");
        if ( NSPV_req(0,msg,len,NODE_NSPV,msg[0]>>1) != 0 )
        {
            for (i=0; i<NSPV_POLLITERS; i++)
            {
                usleep(NSPV_POLLMICROS);
                if ( NSPV_inforesult.height != 0 )
                    return(_NSPV_getinfo_json(&NSPV_inforesult));
            }
        }
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
            fprintf(stderr,"got info response %u size.%d height.%d\n",timestamp,(int32_t)response.size(),NSPV_inforesult.height); // update current height and ntrz status
            break;
        case NSPV_UTXOSRESP:
            NSPV_utxosresp_purge(&NSPV_utxosresult);
            NSPV_rwutxosresp(0,&response[1],&NSPV_utxosresult);
            fprintf(stderr,"got utxos response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos list
            break;
        case NSPV_NTZSRESP:
            NSPV_ntzsresp_purge(&NSPV_ntzsresult);
            NSPV_rwntzsresp(0,&response[1],&NSPV_ntzsresult);
            fprintf(stderr,"got ntzs response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        case NSPV_NTZSPROOFRESP:
            NSPV_ntzsproofresp_purge(&NSPV_ntzsproofresult);
            NSPV_rwntzsproofresp(0,&response[1],&NSPV_ntzsproofresult);
            fprintf(stderr,"got ntzproof response %u size.%d prev.%d next.%d\n",timestamp,(int32_t)response.size(),NSPV_ntzsproofresult.common.prevht,NSPV_ntzsproofresult.common.nextht); // update utxos[i]
            break;
        case NSPV_TXPROOFRESP:
            NSPV_txproof_purge(&NSPV_txproofresult);
            NSPV_rwtxproof(0,&response[1],&NSPV_txproofresult);
            fprintf(stderr,"got txproof response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        case NSPV_SPENTINFORESP:
                
            NSPV_spentinfo_purge(&NSPV_spentresult);
            NSPV_rwspentinfo(0,&response[1],&NSPV_spentresult);
            fprintf(stderr,"got spentinfo response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        default: fprintf(stderr,"unexpected response %02x size.%d at %u\n",response[0],(int32_t)response.size(),timestamp);
                break;
        }
    }
}

void komodo_nSPV(CNode *pto) // polling loop from SendMessages
{
    uint8_t msg[256]; int32_t i,len=0; uint32_t timestamp = (uint32_t)time(NULL);
    if ( NSPV_logintime != 0 && timestamp > NSPV_logintime+60 )
    {
        fprintf(stderr,"scrub wif from NSPV memory\n");
        memset(NSPV_wifstr,0,sizeof(NSPV_wifstr));
        NSPV_logintime = 0;
    }
    if ( (pto->nServices & NODE_NSPV) == 0 )
        return;
    /*if ( timestamp > pto->lastntzs || timestamp > pto->lastproof )
    {
        for (i=0; i<NSPV_numutxos; i++)
        {
            if ( NSPV_utxos[i].prevht == 0 || NSPV_utxos[i].T.txlen == 0 || NSPV_utxos[i].T.txprooflen == 0 )
            {
                request.resize(1);
                if ( NSPV_utxos[i].prevht == 0 && timestamp > pto->lastntzs )
                {
                    request[0] = NSPV_NTZS;
                    pto->lastntzs = timestamp;
                    pto->PushMessage("getnSPV",request);
                    return;
                }
                else if ( timestamp > pto->lastproof )
                {
                    if ( NSPV_utxos[i].T.txlen == 0 )
                    {
                        request[0] = NSPV_TXPROOF;
                        pto->lastproof = timestamp;
                        pto->PushMessage("getnSPV",request);
                    }
                    else // need space for the headers...
                    {
                        request[0] = NSPV_NTZPROOF;
                        pto->lastproof = timestamp;
                        pto->PushMessage("getnSPV",request);
                    }
                    return;
                }
            }
        }
    }*/
    if ( timestamp > NSPV_lastinfo + ASSETCHAINS_BLOCKTIME/2 && timestamp > pto->prevtimes[NSPV_INFO>>1] + 2*ASSETCHAINS_BLOCKTIME/3 )
    {
        len = 0;
        msg[len++] = NSPV_INFO;
        NSPV_req(pto,msg,len,NODE_NSPV,NSPV_INFO>>1);
    }
}

#endif // KOMODO_NSPV_H
