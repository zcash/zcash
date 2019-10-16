
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

#ifndef KOMODO_NSPV_DEFSH
#define KOMODO_NSPV_DEFSH

#define NSPV_PROTOCOL_VERSION 0x00000004
#define NSPV_POLLITERS 200
#define NSPV_POLLMICROS 50000
#define NSPV_MAXVINS 64
#define NSPV_AUTOLOGOUT 777
#define NSPV_BRANCHID 0x76b809bb

// nSPV defines and struct definitions with serialization and purge functions

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
#define NSPV_BROADCAST 0x0c
#define NSPV_BROADCASTRESP 0x0d
#define NSPV_TXIDS 0x0e
#define NSPV_TXIDSRESP 0x0f
#define NSPV_MEMPOOL 0x10
#define NSPV_MEMPOOLRESP 0x11
#define NSPV_CCMODULEUTXOS 0x12
#define NSPV_CCMODULEUTXOSRESP 0x13
#define NSPV_MEMPOOL_ALL 0
#define NSPV_MEMPOOL_ADDRESS 1
#define NSPV_MEMPOOL_ISSPENT 2
#define NSPV_MEMPOOL_INMEMPOOL 3
#define NSPV_MEMPOOL_CCEVALCODE 4
#define NSPV_CC_TXIDS 16
#define NSPV_REMOTERPC 0x14
#define NSPV_REMOTERPCRESP 0x15

int32_t NSPV_gettransaction(int32_t skipvalidation,int32_t vout,uint256 txid,int32_t height,CTransaction &tx,uint256 &hashblock,int32_t &txheight,int32_t &currentheight,int64_t extradata,uint32_t tiptime,int64_t &rewardsum);
UniValue NSPV_spend(char *srcaddr,char *destaddr,int64_t satoshis);
extern uint256 SIG_TXHASH;
uint32_t NSPV_blocktime(int32_t hdrheight);

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

struct NSPV_utxoresp
{
    uint256 txid;
    int64_t satoshis,extradata;
    int32_t vout,height;
};

struct NSPV_utxosresp
{
    struct NSPV_utxoresp *utxos;
    char coinaddr[64];
    int64_t total,interest;
    int32_t nodeheight,skipcount,filter;
    uint16_t numutxos,CCflag;
};

struct NSPV_txidresp
{
    uint256 txid;
    int64_t satoshis;
    int32_t vout,height;
};

struct NSPV_txidsresp
{
    struct NSPV_txidresp *txids;
    char coinaddr[64];
    int32_t nodeheight,skipcount,filter;
    uint16_t numtxids,CCflag;
};

struct NSPV_mempoolresp
{
    uint256 *txids;
    char coinaddr[64];
    uint256 txid;
    int32_t nodeheight,vout,vindex;
    uint16_t numtxids; uint8_t CCflag,funcid;
};

struct NSPV_ntz
{
    uint256 blockhash,txid,othertxid;
    int32_t height,txidheight;
    uint32_t timestamp;
};

struct NSPV_ntzsresp
{
    struct NSPV_ntz prevntz,nextntz;
    int32_t reqheight;
};

struct NSPV_inforesp
{
    struct NSPV_ntz notarization;
    uint256 blockhash;
    int32_t height,hdrheight;
    struct NSPV_equihdr H;
    uint32_t version;
};

struct NSPV_txproof
{
    uint256 txid;
    int64_t unspentvalue;
    int32_t height,vout,txlen,txprooflen;
    uint8_t *tx,*txproof;
    uint256 hashblock;
};

struct NSPV_ntzproofshared
{
    struct NSPV_equihdr *hdrs;
    int32_t prevht,nextht,pad32;
    uint16_t numhdrs,pad16;
};

struct NSPV_ntzsproofresp
{
    struct NSPV_ntzproofshared common;
    uint256 prevtxid,nexttxid;
    int32_t prevtxidht,nexttxidht,prevtxlen,nexttxlen;
    uint8_t *prevntz,*nextntz;
};

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

struct NSPV_broadcastresp
{
    uint256 txid;
    int32_t retcode;
};

struct NSPV_CCmtxinfo
{
    struct NSPV_utxosresp U;
    struct NSPV_utxoresp used[NSPV_MAXVINS];
};

struct NSPV_remoterpcresp
{
    char method[64];
    char json[11000];
};

#endif // KOMODO_NSPV_DEFSH
