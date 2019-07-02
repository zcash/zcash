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

struct NSPV_ntz
{
    uint256 blockhash,txid,othertxid;
    int32_t height,txidheight;
};

struct NSPV_info
{
    struct NSPV_ntz notarization;
    uint256 blockhash;
    int32_t height;
};

struct NSPV_utxo
{
    uint256 txid;
    int64_t satoshis,extradata;
    int32_t vout,height,before,after;
    std::vector<uint8_t> tx,txproof;
};

struct NSPV_ntzs
{
    struct NSPV_ntz before,after;
};

struct NSPV_equiheader
{
    int32_t nVersion;
    uint256 hashPrevBlock;
    uint256 hashMerkleRoot;
    uint256 hashFinalSaplingRoot;
    uint32_t nTime;
    uint32_t nBits;
    CPOSNonce nNonce;
    uint8_t nSolution[1344];
};

struct NSPV_ntzproofhdr
{
    std::vector<NSPV_equiheader> headers;
    int32_t beforeheight,afterheight;
};

struct NSPV_ntzproof
{
    struct NSPV_ntzproofhdr hdr;
    std::vector<uint8_t> beforentz,afterntz;
};

struct NSPV_MMRproof
{
    struct NSPV_ntzproofhdr hdr;
    std::vector<uint8_t> mmrproof;
};

struct NSPV_txproof
{
    uint256 txid;
    std::vector<uint8_t> tx,txproof;
    int32_t height;
};

uint32_t NSPV_lastinfo,NSPV_lastutxos;
std::vector<struct NSPV_utxo> NSPV_utxos;

// on fullnode:
void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> request) // received a request
{
    int32_t len; std::vector<uint8_t> response; uint32_t timestamp = time(NULL);
    if ( (len= request.size()) > 0 )
    {
        response.resize(1);
        if ( len == 1 && request[0] == NSPV_INFO ) // info
        {
            if ( timestamp > pfrom->lastinfo + ASSETCHAINS_BLOCKTIME/2 )
            {
                response[0] = NSPV_INFORESP;
                pfrom->lastinfo = timestamp;
                pfrom->PushMessage("nSPV",response);
            }
        }
        else if ( request[0] == NSPV_UTXOS )
        {
            if ( timestamp > pfrom->lastutxos + ASSETCHAINS_BLOCKTIME/2 )
            {
                response[0] = NSPV_UTXOSRESP;
                pfrom->lastutxos = timestamp;
                pfrom->PushMessage("nSPV",response);
            }
        }
        else if ( request[0] == NSPV_NTZS )
        {
            if ( timestamp > pfrom->lastntzs )
            {
                response[0] = NSPV_NTZSRESP;
                pfrom->lastntzs = timestamp;
                pfrom->PushMessage("nSPV",response);
            }
        }
        else if ( request[0] == NSPV_NTZPROOF )
        {
            if ( timestamp > pfrom->lastproof )
            {
                response[0] = NSPV_NTZPROOFRESP;
                pfrom->lastproof = timestamp;
                pfrom->PushMessage("nSPV",response);
            }
        }
        else if ( request[0] == NSPV_TXPROOF )
        {
            if ( timestamp > pfrom->lastproof )
            {
                response[0] = NSPV_TXPROOFRESP;
                pfrom->lastproof = timestamp;
                pfrom->PushMessage("nSPV",response);
            }
        }
  }
}

// on nSPV client
void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> response) // received a response
{
    int32_t len; uint32_t timestamp = (uint32_t)time(NULL);
    if ( (len= response.size()) > 0 )
    {
        switch ( response[0] )
        {
        case NSPV_INFORESP:
            fprintf(stderr,"got info response %u\n",timestamp); // update current height and ntrz status
            break;
        case NSPV_UTXOSRESP:
            fprintf(stderr,"got utxos response %u\n",timestamp); // update utxos list
            break;
        case NSPV_NTZSRESP:
            fprintf(stderr,"got ntzs response %u\n",timestamp); // update utxos[i]
            break;
        case NSPV_NTZPROOFRESP:
            fprintf(stderr,"got ntzproof response %u\n",timestamp); // update utxos[i]
            break;
        case NSPV_TXPROOFRESP:
            fprintf(stderr,"got txproof response %u\n",timestamp); // update utxos[i]
            break;
        default: fprintf(stderr,"unexpected response %02x size.%d at %u\n",response[0],(int32_t)response.size(),timestamp);
                break;
        }
    }
}

void komodo_nSPV(CNode *pto)
{
    std::vector<uint8_t> request; int32_t i; uint32_t timestamp = time(NULL);
    if ( timestamp > pto->lastntzs || timestamp > pto->lastproof )
    {
        for (i=0; i<NSPV_utxos.size(); i++)
        {
            if ( NSPV_utxos[i].before == 0 || NSPV_utxos[i].tx.size() == 0 || NSPV_utxos[i].txproof.size() == 0 )
            {
                request.resize(1);
                if ( NSPV_utxos[i].before == 0 && timestamp > pto->lastntzs )
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
            request.resize(1);
            request[0] = NSPV_UTXOS;
            NSPV_lastutxos = pto->lastutxos = timestamp;
            pto->PushMessage("getnSPV",request);
        }
    }
    if ( timestamp > NSPV_lastinfo + ASSETCHAINS_BLOCKTIME/2 && timestamp > pto->lastinfo + ASSETCHAINS_BLOCKTIME )
    {
        // query current height, blockhash, notarization info
        request.resize(1);
        request[0] = NSPV_INFO;
        NSPV_lastinfo = pto->lastinfo = timestamp;
        pto->PushMessage("getnSPV",request);
    }
}

#endif // KOMODO_NSPV_H
