// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

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

// todo: make sure no files are updated
// finalize structs, de/serialization
// rpc calls
// validate proofs

// make sure to sanity check all vector lengths on receipt

#ifndef KOMODO_NSPV_H
#define KOMODO_NSPV_H

#define NSPV_INFO 0x00
#define NSPV_INFORESP 0x01
#define NSPV_UTXOS 0x02
#define NSPV_UTXOSRESP 0x03
#define NSPV_NTZS 0x04
#define NSPV_NTZSRESP 0x05
#define NSPV_NTZPROOF 0x06
#define NSPV_NTZPROOFRESP 0x07
#define NSPV_TXPROOF 0x08
#define NSPV_TXPROOFRESP 0x09
#define NSPV_SPENTINFO 0x0a
#define NSPV_SPENTINFORESP 0x0b

int32_t iguana_rwbuf(int32_t rwflag,uint8_t *serialized,uint16_t len,uint8_t *buf)
{
    if ( rwflag != 0 )
        memcpy(serialized,buf,len);
    else memcpy(buf,serialized,len);
    return(len)
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

int32_t iguana_rwequihdrvec(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,uint8_t struct NSPV_equihdr **ptrp)
{
    int32_t i,vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        if ( *ptrp == 0 )
            *ptrp = calloc(sizeof(**ptrp),vsize); // relies on uint16_t being "small" to prevent mem exhaustion
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
            *ptrp = calloc(1,vsize); // relies on uint16_t being "small" to prevent mem exhaustion
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
    int32_t pad32;
    uint16_t numutxos,pad16;
};

int32_t NSPV_rwutxosresp(int32_t rwflag,uint8_t *serialized,uint16_t *vecsizep,uint8_t struct NSPV_utxosresp **ptrp) // check mempool
{
    int32_t i,vsize,len = 0;
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(*vecsizep),vecsizep);
    if ( (vsize= *vecsizep) != 0 )
    {
        if ( *ptrp == 0 )
            *ptrp = calloc(sizeof(**ptrp),vsize); // relies on uint16_t being "small" to prevent mem exhaustion
        for (i=0; i<vsize; i++)
            len += NSPV_rwutxoresp(rwflag,&serialized[len],&(*ptrp)[i]);
    }
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->total),&ptr->total);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->interest),&ptr->interest);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
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
    len += NSPV_rwntz(rwflag,&serialized[len],sizeof(ptr->prevntz),&ptr->prevntz);
    len += NSPV_rwntz(rwflag,&serialized[len],sizeof(ptr->nextntz),&ptr->nextntz);
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
    len += NSPV_rwntz(rwflag,&serialized[len],sizeof(ptr->notarization),&ptr->notarization);
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
    uint32_t pad32;
    uint16_t prevlen,nextlen;
    uint8_t *prevntz,*nextntz;
};

int32_t NSPV_rwntzproof(int32_t rwflag,uint8_t *serialized,struct NSPV_ntzsproofresp *ptr)
{
    int32_t len = 0;
    len += NSPV_rwntzproofshared(rwflag,&serialized[len],&ptr->common);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->pad32),&ptr->pad32);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->prevlen,&ptr->prevntz);
    len += iguana_rwuint8vec(rwflag,&serialized[len],&ptr->nextlen,&ptr->nextntz);
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
    struct NSPV_ntzproofhdr hdr;
    // tbd
};

struct NSPV_spentinfo
{
    struct NSPV_txproof spent;
    uint256 txid;
    int32_t height,spentvini;
};

int32_t NSPV_rwspentinfo(int32_t rwflag,uint8_t *serialized,struct NSPV_spentinfo *ptr) // check mempool
{
    int32_t len = 0;
    len += NSPV_rwtxproof(rwflag,&serialized[len],&ptr->spent);
    len += iguana_rwbignum(rwflag,&serialized[len],sizeof(ptr->txid),(uint8_t *)&ptr->txid);
    len += iguana_rwnum(rwflag,&serialized[len],sizeof(ptr->height),&ptr->height);
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

int32_t NSPV_getinfo(struct NSPV_inforesp *ptr)
{
    int32_t prevMoMheight,len = 0; CBlockIndex *pindex;
    if ( (pindex= chainActive.LastTip()) != 0 )
    {
        ptr->height = pindex->GetHeight();
        ptr->blockhash = pindex->GetBlockHash();
        ptr->notarization.height = komodo_notarized_height(&prevMoMheight,&ptr->notarization.blockhash,&ptr->notarization.othertxid);
        //ptr->notarization.txidheight = komodo_findnotarization(&ptr->notarization.txid,ptr->notarization.height,ptr->notarization.blockhash);
        return(sizeof(*ptr));
    } else return(-1);
}

int32_t NSPV_getaddressutxos(struct NSPV_utxosresp *ptr,char *coinaddr) // check mempool
{
    int64_t total = 0,interest=0; uint32_t locktime; int32_t tipheight,txheight,n = 0,len = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;
    fprintf(stderr,"getaddressutxos for %s\n",coinaddr);
    SetCCunspents(unspentOutputs,coinaddr,false);
    if ( (ptr->numutxos= (int32_t)unspentOutputs.size()) > 0 )
    {
        tipheight = chainActive.LastTip()->GetHeight();
        ptr->utxos = calloc(ptr->numutxos,sizeof(*ptr->utxos));
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
        fprintf(stderr,"getaddressutxos for %s -> n.%d total %.8f interest %.8f\n",coinaddr,dstr(total),dstr(interest));
        if ( n == ptr->numutxos )
        {
            ptr->total = total;
            ptr->interest = interest;
            return((int32_t)(sizeof(*ptr) + sizeof(*ptr->utxos)*ptr->numutxos));
        }
    }
    if ( ptr->utxos != 0 )
        free(ptr->utxos);
    memst(ptr,0,sizeof(*ptr));
    return(0);
}

int32_t NSPV_getntzsresp(struct NSPV_ntzsresp *ptr,int32_t height)
{
    int32_t len = 0;
    return(sizeof(*ptr));
}

int32_t NSPV_getntzsproofresp(struct NSPV_ntzsproofresp *ptr,int32_t prevht,int32_t nextht)
{
    int32_t len = 0;
    return(len);
}

int32_t NSPV_gettxproof(struct NSPV_txproof *ptr,uint256 txid,int32_t height)
{
    int32_t len = 0;
    return(len);
}

int32_t NSPV_getspentinfo(struct NSPV_spentinfo *ptr,uint256 txid,int32_t vout)
{
    int32_t len = 0;
    return(len);
}

void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> request) // received a request
{
    int32_t len,slen; std::vector<uint8_t> response; uint32_t timestamp;
    timestamp = (uint32_t)time(NULL);
    if ( (len= request.size()) > 0 )
    {
        if ( request[0] == NSPV_INFO ) // info
        {
            if ( timestamp > pfrom->lastinfo + ASSETCHAINS_BLOCKTIME/2 )
            {
                struct NSPV_inforesp I;
                memset(&I,0,sizeof(I));
                if ( (slen= NSPV_getinfo(&I)) > 0 )
                {
                    response.resize(1 + slen);
                    response[0] = NSPV_INFORESP;
                    NSPV_rwinforesp(1,&response[1],&I);
                    pfrom->PushMessage("nSPV",response);
                    pfrom->lastinfo = timestamp;
                    NSPV_inforesp_purge(&I);
                }
            }
        }
        else if ( request[0] == NSPV_UTXOS )
        {
            if ( timestamp > pfrom->lastutxos + ASSETCHAINS_BLOCKTIME/2 )
            {
                struct NSPV_utxosresp U; char coinaddr[64];
                if ( len < 64 && request[1] == len-2 )
                {
                    memcpy(coinaddr,&request[2],request[1]);
                    memset(&U,0,sizeof(U));
                    slen = NSPV_getaddressutxos(&U,coinaddr);
                    response.resize(1 + slen);
                    response[0] = NSPV_UTXOSRESP;
                    if ( NSPV_rwutxosresp(1,&response[1],&U.numutxos,&U.utxos) == slen )
                    {
                        pfrom->PushMessage("nSPV",response);
                        pfrom->lastutxos = timestamp;
                    }
                    NSPV_utxosresp_purge(&U);
                }
            }
        }
        else if ( request[0] == NSPV_NTZS )
        {
            if ( timestamp > pfrom->lastntzs )
            {
                struct NSPV_ntzsresp N; int32_t height;
                if ( len == 1+sizeof(height) )
                {
                    iguana_rwnum(rwflag,&request[1],sizeof(height),&height);
                    memset(&N,0,sizeof(N));
                    slen = NSPV_getntzsresp(&N,height);
                    response.resize(1 + slen);
                    response[0] = NSPV_NTZSRESP;
                    if ( NSPV_rwntzsresp(1,&response[1],&N) == slen )
                    {
                        pfrom->PushMessage("nSPV",response);
                        pfrom->lastntzs = timestamp;
                    }
                    NSPV_ntzsresp_purge(&N);
                }
            }
        }
        else if ( request[0] == NSPV_NTZPROOF )
        {
            if ( timestamp > pfrom->lastproof )
            {
                struct NSPV_ntzsproofresp P; int32_t prevht,nextht;
                if ( len == 1+sizeof(prevht)+sizeof(nextht) )
                {
                    iguana_rwnum(rwflag,&request[1],sizeof(prevht),&prevht);
                    iguana_rwnum(rwflag,&request[1+sizeof(prevht)],sizeof(nextht),&nextht);
                    if ( prevht != 0 && nextht != 0 && nextht >= prevht )
                    {
                        memset(&N,0,sizeof(N));
                        slen = NSPV_getntzsproofresp(&P,prevht,nextht);
                        response.resize(1 + slen);
                        response[0] = NSPV_NTZPROOFRESP;
                        if ( NSPV_rwntzsresp(1,&response[1],&P) == slen )
                        {
                            pfrom->PushMessage("nSPV",response);
                            pfrom->lastproof = timestamp;
                        }
                        NSPV_ntzsproofresp_purge(&P);
                    }
                }
            }
        }
        else if ( request[0] == NSPV_TXPROOF )
        {
            if ( timestamp > pfrom->lastproof )
            {
                struct NSPV_spentinfo P; uint256 txid; int32_t height;
                if ( len == 1+sizeof(txid)+sizeof(height) )
                {
                    iguana_rwnum(rwflag,&request[1],sizeof(height),&height);
                    iguana_rwbignum(rwflag,&request[1+sizeof(height)],sizeof(txid),(uint8_t *)&txid);
                    memset(&P,0,sizeof(P));
                    slen = NSPV_gettxproof(&P,txid,height);
                    response.resize(1 + slen);
                    response[0] = NSPV_TXPROOFRESP;
                    if ( NSPV_rwtxproof(1,&response[1],&P) == slen )
                    {
                        pfrom->PushMessage("nSPV",response);
                        pfrom->lastproof = timestamp;
                    }
                    NSPV_txproof_purge(&P);
                }
            }
        }
        else if ( request[0] == NSPV_SPENTINFO )
        {
            if ( timestamp > pfrom->lastspent )
            {
                struct NSPV_spentinfo S; int32_t vout; uint256 txid;
                if ( len == 1+sizeof(txid)+sizeof(vout) )
                {
                    iguana_rwnum(rwflag,&request[1],sizeof(vout),&vout);
                    iguana_rwbignum(rwflag,&request[1+sizeof(vout)],sizeof(txid),(uint8_t *)&txid);
                    memset(&S,0,sizeof(S));
                    slen = NSPV_getspentinfo(&S,txid,vout);
                    response.resize(1 + slen);
                    response[0] = NSPV_SPENTINFORESP;
                    if ( NSPV_rwspentinfo(1,&response[1],&S) == slen )
                    {
                        pfrom->PushMessage("nSPV",response);
                        pfrom->lastspent = timestamp;
                    }
                    NSPV_spentinfo_purge(&S);
                }
            }
        }
    }
}

// on nSPV client
uint32_t NSPV_lastinfo,NSPV_lastutxos;
int32_t NSPV_numutxos,NSPV_numspends;
struct NSPV_utxo *NSPV_utxos;
struct NSPV_spentinfo *NSPV_spends;

void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> response) // received a response
{
    int32_t len; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= response.size()) > 0 )
    {
        switch ( response[0] )
        {
        case NSPV_INFORESP:
            fprintf(stderr,"got info response %u size.%d\n",timestamp,(int32_t)response.size()); // update current height and ntrz status
            break;
        case NSPV_UTXOSRESP:
            fprintf(stderr,"got utxos response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos list
            break;
        case NSPV_NTZSRESP:
            fprintf(stderr,"got ntzs response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        case NSPV_NTZPROOFRESP:
            fprintf(stderr,"got ntzproof response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        case NSPV_TXPROOFRESP:
            fprintf(stderr,"got txproof response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        case NSPV_SPENTINFORESP:
            fprintf(stderr,"got spentinfo response %u size.%d\n",timestamp,(int32_t)response.size()); // update utxos[i]
            break;
        default: fprintf(stderr,"unexpected response %02x size.%d at %u\n",response[0],(int32_t)response.size(),timestamp);
                break;
        }
    }
}

void komodo_NSPV_spentinfoclear()
{
    if ( NSPV_spends != 0 )
        free(NSPV_spends);
    NSPV_spends = 0;
    NSPV_numspends = 0;
}

struct NSPV_spentinfo komodo_NSPV_spentinfo(uint256 txid,int32_t vout) // just a primitive example of how to add new rpc to p2p msg
{
    std::vector<uint8_t> request; struct NSPV_spentinfo I; int32_t i,numsent = 0; uint32_t timestamp;
    timestamp = (uint32_t)time(NULL);
    for (i=0; i<NSPV_numspends; i++)
    {
        I = NSPV_spends[i];
        if ( I.txid == txid && I.vout == vout )
            return(I);
    }
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode *pnode, vNodes)
    {
        if ( pnode->hSocket == INVALID_SOCKET )
            continue;
        if ( (pnode->nServices & NODE_SPENTINDEX) != 0 && timestamp > pnode->lastspent )
        {
            request.resize(1);
            request[0] = NSPV_SPENTINFO;
            pnode->lastspent = timestamp;
            pnode->PushMessage("getnSPV",request);
            if ( ++numsent >= 3 )
                break;
        }
    }
}

void komodo_nSPV(CNode *pto)
{
    std::vector<uint8_t> request; int32_t i; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (pto->nServices & NODE_NSPV) == 0 )
        return;
    if ( timestamp > pto->lastntzs || timestamp > pto->lastproof )
    {
        for (i=0; i<NSPV_numutxos; i++)
        {
            if ( NSPV_utxos[i].prevlen == 0 || NSPV_utxos[i].tx.size() == 0 || NSPV_utxos[i].txproof.size() == 0 )
            {
                request.resize(1);
                if ( NSPV_utxos[i].prevlen == 0 && timestamp > pto->lastntzs )
                {
                    request[0] = NSPV_NTZS;
                    pto->lastntzs = timestamp;
                    pto->PushMessage("getnSPV",request);
                    return;
                }
                else if ( timestamp > pto->lastproof )
                {
                    if ( NSPV_utxos[i].tx.size() == 0 )
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
    }
    if ( timestamp > NSPV_lastutxos + ASSETCHAINS_BLOCKTIME/2 )
    {
        if ( (pto->nServices & NODE_ADDRINDEX) != 0 && timestamp > pto->lastutxos + ASSETCHAINS_BLOCKTIME )
        {
            // get utxo since lastheight
            if ( (rand() % 100) < 10 )
            {
                char coinaddr[64]; int32_t slen;
                Getscriptaddress(coinaddr,CScript() << Mypubkey() << OP_CHECKSIG);
                slen = (int32_t)strlen(coinaddr);
                if ( slen < 64 )
                {
                    request.resize(1 + 1 + slen);
                    request[0] = NSPV_UTXOS;
                    request[1] = slen;
                    memcpy(&request[2],coinaddr,slen);
                    NSPV_lastutxos = pto->lastutxos = timestamp;
                    pto->PushMessage("getnSPV",request);
                }
            }
        }
    }
    if ( timestamp > NSPV_lastinfo + ASSETCHAINS_BLOCKTIME/2 && timestamp > pto->lastinfo + ASSETCHAINS_BLOCKTIME )
    {
        if ( (rand() % 100) < 10 )
        {
            // query current height, blockhash, notarization info
            request.resize(1);
            request[0] = NSPV_INFO;
            NSPV_lastinfo = pto->lastinfo = timestamp;
            pto->PushMessage("getnSPV",request);
        }
    }
}

#endif // KOMODO_NSPV_H
