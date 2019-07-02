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


void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> request) // received a request
{
    int32_t len; std::vector<uint8_t> response;
    if ( (len= request.size()) > 0 )
    {
        response.resize(1);
        if ( len == 1 && request[0] == NSPV_INFO ) // info
        {
            response[0] = NSPV_INFORESP;
            pfrom->PushMessage("nSPV",response);
        }
        else if ( request[0] == NSPV_UTXOS )
        {
            response[0] = NSPV_UTXOSRESP;
            pfrom->PushMessage("nSPV",response);
        }
    }
}

void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> response) // received a response
{
    int32_t len;
    if ( (len= response.size()) > 0 )
    {
        switch ( response[0] )
        {
        case NSPV_INFORESP:
                fprintf(stderr,"got info response\n");
                break;
        case NSPV_UTXOSRESP:
                fprintf(stderr,"got utxos response\n");
                break;
        default: fprintf(stderr,"unexpected response %02x size.%d\n",response[0],(int32_t)response.size());
                break;
        }
    }
}

void komodo_nSPV(CNode *pto)
{
    std::vector<uint8_t> request;
    // limit frequency!
    if ( (pto->nServices & NODE_ADDRINDEX) != 0 )
    {
        // get utxo since lastheight
        request.resize(1);
        request[0] = NSPV_UTXOS;
        pnode->PushMessage("getnSPV",request);
    }
    else
    {
        // query current height, blockhash, notarization info
        request.resize(1);
        request[0] = NSPV_INFO;
        pnode->PushMessage("getnSPV",request);
    }
}

#endif // KOMODO_NSPV_H
