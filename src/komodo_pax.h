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
#include "komodo_defs.h"

#define USD 0
#define EUR 1
#define JPY 2
#define GBP 3
#define AUD 4
#define CAD 5
#define CHF 6
#define NZD 7

#define CNY 8
#define RUB 9
#define MXN 10
#define BRL 11
#define INR 12
#define HKD 13
#define TRY 14
#define ZAR 15

#define PLN 16
#define NOK 17
#define SEK 18
#define DKK 19
#define CZK 20
#define HUF 21
#define ILS 22
#define KRW 23

#define MYR 24
#define PHP 25
#define RON 26
#define SGD 27
#define THB 28
#define BGN 29
#define IDR 30
#define HRK 31

#define MAX_CURRENCIES 32
extern char CURRENCIES[][8];

uint64_t komodo_maxallowed(int32_t baseid);

uint64_t komodo_paxvol(uint64_t volume,uint64_t price);

void pax_rank(uint64_t *ranked,uint32_t *pvals);

double PAX_BTCUSD(int32_t height,uint32_t btcusd);

int32_t dpow_readprices(int32_t height,uint8_t *data,uint32_t *timestampp,double *KMDBTCp,double *BTCUSDp,double *CNYUSDp,uint32_t *pvals);

int32_t komodo_pax_opreturn(int32_t height,uint8_t *opret,int32_t maxsize);

int32_t PAX_pubkey(int32_t rwflag,uint8_t *pubkey33,uint8_t *addrtypep,uint8_t rmd160[20],char fiat[4],uint8_t *shortflagp,int64_t *fiatoshisp);

double PAX_val(uint32_t pval,int32_t baseid);

void komodo_pvals(int32_t height,uint32_t *pvals,uint8_t numpvals);

uint64_t komodo_paxcorrelation(uint64_t *votes,int32_t numvotes,uint64_t seed);

uint64_t komodo_paxcalc(int32_t height,uint32_t *pvals,int32_t baseid,int32_t relid,uint64_t basevolume,uint64_t refkmdbtc,uint64_t refbtcusd);

uint64_t _komodo_paxprice(uint64_t *kmdbtcp,uint64_t *btcusdp,int32_t height,char *base,char *rel,uint64_t basevolume,uint64_t kmdbtc,uint64_t btcusd);

int32_t komodo_kmdbtcusd(int32_t rwflag,uint64_t *kmdbtcp,uint64_t *btcusdp,int32_t height);

uint64_t _komodo_paxpriceB(uint64_t seed,int32_t height,char *base,char *rel,uint64_t basevolume);

uint64_t komodo_paxpriceB(uint64_t seed,int32_t height,char *base,char *rel,uint64_t basevolume);

uint64_t komodo_paxprice(uint64_t *seedp,int32_t height,char *base,char *rel,uint64_t basevolume);

int32_t komodo_paxprices(int32_t *heights,uint64_t *prices,int32_t max,char *base,char *rel);

void komodo_paxpricefeed(int32_t height,uint8_t *pricefeed,int32_t opretlen);

uint64_t PAX_fiatdest(uint64_t *seedp,int32_t tokomodo,char *destaddr,uint8_t pubkey33[33],char *coinaddr,int32_t height,char *origbase,int64_t fiatoshis);
