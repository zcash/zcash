/******************************************************************************
 * Copyright © 2014-2016 The SuperNET Developers.                             *
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

int COINBASE_MATURITY = 100;

int32_t IS_KOMODO_NOTARY,USE_EXTERNAL_PUBKEY;
std::string NOTARY_PUBKEY;
uint8_t NOTARY_PUBKEY33[33];

int32_t ASSETCHAINS_SHORTFLAG;
char ASSETCHAINS_SYMBOL[16];
uint16_t ASSETCHAINS_PORT;
uint32_t ASSETCHAIN_INIT;
uint32_t ASSETCHAINS_MAGIC = 2387029918;
uint64_t ASSETCHAINS_SUPPLY = 10;

int32_t NOTARIZED_HEIGHT,Num_nutxos,KMDHEIGHT = 43000;
uint256 NOTARIZED_HASH,NOTARIZED_DESTTXID;
pthread_mutex_t komodo_mutex;
uint32_t KOMODO_INITDONE,KOMODO_REALTIME;
char KMDUSERPASS[1024]; uint16_t BITCOIND_PORT = 7771;
uint64_t PENDING_KOMODO_TX;
