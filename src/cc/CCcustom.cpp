/******************************************************************************
 * Copyright © 2014-2018 The SuperNET Developers.                             *
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

#include "CCinclude.h"
#include "CCassets.h"
#include "CCfaucet.h"
#include "CCrewards.h"
#include "CCdice.h"

/*
 CCcustom has most of the functions that need to be extended to create a new CC contract.
 
 A CC scriptPubKey can only be spent if it is properly signed and validated. By constraining the vins and vouts, it is possible to implement a variety of functionality. CC vouts have an otherwise non-standard form, but it is properly supported by the enhanced bitcoin protocol code as a "cryptoconditions" output and the same pubkey will create a different address.
 
 This allows creation of a special address(es) for each contract type, which has the privkey public. That allows anybody to properly sign and spend it, but with the constraints on what is allowed in the validation code, the contract functionality can be implemented.
 
 what needs to be done to add a new contract:
 1. add EVAL_CODE to eval.h
 2. initialize the variables in the CCinit function below
 3. write a Validate function to reject any unsanctioned usage of vin/vout
 4. make helper functions to create rawtx for RPC functions
 5. add rpc calls to rpcserver.cpp and rpcserver.h and in one of the rpc.cpp files
 6. add the new .cpp files to src/Makefile.am
 */

//BTCD Address: RAssetsAtGnvwgK9gVHBbAU4sVTah1hAm5
//BTCD Privkey: UvtvQVgVScXEYm4J3r4nE4nbFuGXSVM5pKec8VWXwgG9dmpWBuDh
//BTCD Address: RSavingsEYcivt2DFsxsKeCjqArV6oVtVZ
//BTCD Privkey: Ux6XQekTxokko6gZHz24B7PUsmUQtWFzG2W9nUA8jba7UoVbPBF4

// Assets, aka Tokens
#define FUNCNAME IsAssetsInput
#define EVALCODE EVAL_ASSETS
const char *AssetsCCaddr = "RGKRjeTBw4LYFotSDLT6RWzMHbhXri6BG6";  //"RFYE2yL3KknWdHK6uNhvWacYsCUtwzjY3u";
char AssetsCChexstr[67] = { "02adf84e0e075cf90868bd4e3d34a03420e034719649c41f371fc70d8e33aa2702" };
uint8_t AssetsCCpriv[32] = { 0x9b, 0x17, 0x66, 0xe5, 0x82, 0x66, 0xac, 0xb6, 0xba, 0x43, 0x83, 0x74, 0xf7, 0x63, 0x11, 0x3b, 0xf0, 0xf3, 0x50, 0x6f, 0xd9, 0x6b, 0x67, 0x85, 0xf9, 0x7a, 0xf0, 0x54, 0x4d, 0xb1, 0x30, 0x77 };

#include "CCcustom.inc"
#undef FUNCNAME
#undef EVALCODE

// Faucet
#define FUNCNAME IsFaucetInput
#define EVALCODE EVAL_FAUCET
const char *FaucetCCaddr = "R9zHrofhRbub7ER77B7NrVch3A63R39GuC";  //"RKQV4oYs4rvxAWx1J43VnT73rSTVtUeckk";
char FaucetCChexstr[67] = { "03682b255c40d0cde8faee381a1a50bbb89980ff24539cb8518e294d3a63cefe12" };
uint8_t FaucetCCpriv[32] = { 0xd4, 0x4f, 0xf2, 0x31, 0x71, 0x7d, 0x28, 0x02, 0x4b, 0xc7, 0xdd, 0x71, 0xa0, 0x39, 0xc4, 0xbe, 0x1a, 0xfe, 0xeb, 0xc2, 0x46, 0xda, 0x76, 0xf8, 0x07, 0x53, 0x3d, 0x96, 0xb4, 0xca, 0xa0, 0xe9 };

#include "CCcustom.inc"
#undef FUNCNAME
#undef EVALCODE

// Rewards
#define FUNCNAME IsRewardsInput
#define EVALCODE EVAL_REWARDS
const char *RewardsCCaddr = "RJCqA4jQTFEZ841dZgxko8aYgUU3FRNGNm";  //"RYRJGMAYEfLCZ6ZddbpxPiUZ1sens8vPYK";
char RewardsCChexstr[67] = { "026f00fdc2f1ed0006d66e2ca1787633590581c2fc90e7cb7b01a6c1131b40e94d" };
uint8_t RewardsCCpriv[32] = { 0x9f, 0x0c, 0x57, 0xdc, 0x6f, 0x78, 0xae, 0xb0, 0xc7, 0x62, 0x9e, 0x7d, 0x2b, 0x90, 0x6b, 0xbd, 0x40, 0x78, 0x19, 0x5b, 0x3c, 0xb8, 0x82, 0x2d, 0x29, 0x84, 0x72, 0x7a, 0x59, 0x5a, 0x4b, 0x69 };
#include "CCcustom.inc"
#undef FUNCNAME
#undef EVALCODE

// Dice
#define FUNCNAME IsDiceInput
#define EVALCODE EVAL_DICE
const char *DiceCCaddr = "REabWB7KjFN5C3LFMZ5odExHPenYzHLtVw";  //"RLEe8f7Eg3TDuXii9BmNiiiaVGraHUt25c";
char DiceCChexstr[67] = { "039d966927cfdadab3ee6c56da63c21f17ea753dde4b3dfd41487103e24b27e94e" };
uint8_t DiceCCpriv[32] = { 0x0e, 0xe8, 0xf5, 0xb4, 0x3d, 0x25, 0xcc, 0x35, 0xd1, 0xf1, 0x2f, 0x04, 0x5f, 0x01, 0x26, 0xb8, 0xd1, 0xac, 0x3a, 0x5a, 0xea, 0xe0, 0x25, 0xa2, 0x8f, 0x2a, 0x8e, 0x0e, 0xf9, 0x34, 0xfa, 0x77 };
#include "CCcustom.inc"
#undef FUNCNAME
#undef EVALCODE

struct CCcontract_info *CCinit(struct CCcontract_info *cp,uint8_t evalcode)
{
    cp->evalcode = evalcode;
    switch ( evalcode )
    {
        case EVAL_ASSETS:
            strcpy(cp->unspendableCCaddr,AssetsCCaddr);
            strcpy(cp->CChexstr,AssetsCChexstr);
            memcpy(cp->CCpriv,AssetsCCpriv,32);
            cp->validate = AssetsValidate;
            cp->ismyvin = IsAssetsInput;
            break;
        case EVAL_FAUCET:
            strcpy(cp->unspendableCCaddr,FaucetCCaddr);
            strcpy(cp->CChexstr,FaucetCChexstr);
            memcpy(cp->CCpriv,FaucetCCpriv,32);
            cp->validate = FaucetValidate;
            cp->ismyvin = IsFaucetInput;
            break;
        case EVAL_REWARDS:
            strcpy(cp->unspendableCCaddr,RewardsCCaddr);
            strcpy(cp->CChexstr,RewardsCChexstr);
            memcpy(cp->CCpriv,RewardsCCpriv,32);
            cp->validate = RewardsValidate;
            cp->ismyvin = IsRewardsInput;
            break;
        case EVAL_DICE:
            strcpy(cp->unspendableCCaddr,DiceCCaddr);
            strcpy(cp->CChexstr,DiceCChexstr);
            memcpy(cp->CCpriv,DiceCCpriv,32);
            cp->validate = DiceValidate;
            cp->ismyvin = IsDiceInput;
            break;
    }
    return(cp);
}

