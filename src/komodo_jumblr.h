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
#pragma once
#include "uthash.h" // UT_hash_handle
#include "komodo_cJSON.h"
#include "komodo_defs.h"

#ifdef _WIN32
#include <wincrypt.h>
#endif

#define JUMBLR_ADDR "RGhxXpXSSBTBm9EvNsXnTQczthMCxHX91t"
#define JUMBLR_BTCADDR "18RmTJe9qMech8siuhYfMtHo8RtcN1obC6"
#define JUMBLR_MAXSECRETADDRS 777
#define JUMBLR_SYNCHRONIZED_BLOCKS 10
#define JUMBLR_INCR 9.965
#define JUMBLR_FEE 0.001
#define JUMBLR_TXFEE 0.01
#define SMALLVAL 0.000000000000001

#define JUMBLR_ERROR_DUPLICATEDEPOSIT -1
#define JUMBLR_ERROR_SECRETCANTBEDEPOSIT -2
#define JUMBLR_ERROR_TOOMANYSECRETS -3
#define JUMBLR_ERROR_NOTINWALLET -4

struct jumblr_item
{
    UT_hash_handle hh;
    int64_t amount,fee,txfee; // fee and txfee not really used (yet)
    uint32_t spent,pad;
    char opid[66],src[128],dest[128],status;
};

char *jumblr_issuemethod(char *userpass,char *method,char *params,uint16_t port);

char *jumblr_importaddress(char *address);

char *jumblr_validateaddress(char *addr);

int32_t Jumblr_secretaddrfind(char *searchaddr);

int32_t Jumblr_secretaddradd(char *secretaddr); // external

int32_t Jumblr_depositaddradd(char *depositaddr); // external

int32_t Jumblr_secretaddr(char *secretaddr);

int32_t jumblr_addresstype(char *addr);

struct jumblr_item *jumblr_opidfind(char *opid);

struct jumblr_item *jumblr_opidadd(char *opid);

char *jumblr_zgetnewaddress();

char *jumblr_zlistoperationids();

char *jumblr_zgetoperationresult(char *opid);

char *jumblr_zgetoperationstatus(char *opid);

char *jumblr_sendt_to_z(char *taddr,char *zaddr,double amount);

char *jumblr_sendz_to_z(char *zaddrS,char *zaddrD,double amount);

char *jumblr_sendz_to_t(char *zaddr,char *taddr,double amount);

char *jumblr_zlistaddresses();

char *jumblr_zlistreceivedbyaddress(char *addr);

char *jumblr_getreceivedbyaddress(char *addr);

char *jumblr_importprivkey(char *wifstr);

char *jumblr_zgetbalance(char *addr);

char *jumblr_listunspent(char *coinaddr);

char *jumblr_gettransaction(char *txidstr);

int32_t jumblr_numvins(bits256 txid);

int64_t jumblr_receivedby(char *addr);

int64_t jumblr_balance(char *addr);

int32_t jumblr_itemset(struct jumblr_item *ptr,cJSON *item,char *status);

void jumblr_opidupdate(struct jumblr_item *ptr);

void jumblr_prune(struct jumblr_item *ptr);

void jumblr_zaddrinit(char *zaddr);

void jumblr_opidsupdate();

uint64_t jumblr_increment(uint8_t r,int32_t height,uint64_t total,uint64_t biggest,uint64_t medium, uint64_t smallest);

void jumblr_iteration();
