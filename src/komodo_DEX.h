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

// included from komodo_nSPV_superlite.h

int32_t komodo_DEXprocess(CNode *pfrom,std::vector<uint8_t> &response,uint8_t *msg,int32_t len) // incoming message
{
    // update DEX state and set response and return nonzero if there is a response
    return(0);
}

void komodo_DEXpoll(CNode *pto) // from SendMessages polling
{
    // determine what message to send to the other node
}

cJSON *komodo_DEXrpc(cJSON *jsonargs) // from cli
{
    // update DEX state
}

