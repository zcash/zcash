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

void komodo_nSPVreq(CNode *pfrom,std::vector<uint8_t> payload) // received a request
{
    
}

void komodo_nSPVresp(CNode *pfrom,std::vector<uint8_t> payload) // received a response
{
    
}

/*void komodo_sendnSPV(int32_t minpeers,int32_t maxpeers,const char *message,std::vector<uint8_t> payload)
{
    int32_t numsent = 0;
    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes)
    {
        if ( pnode->hSocket == INVALID_SOCKET )
            continue;
        if ( numsent < minpeers || (rand() % 10) == 0 )
        {
            //fprintf(stderr,"pushmessage\n");
            pnode->PushMessage(message,payload);
            if ( numsent++ > maxpeers )
                break;
        }
    }
}*/

void komodo_nSPV(CNode *pto) // issue nSPV requests if has nServices
{
    
}

#endif // KOMODO_NSPV_H
