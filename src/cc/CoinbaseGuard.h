/********************************************************************
 * (C) 2018 Michael Toutonghi
 * 
 * Distributed under the MIT software license, see the accompanying
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.
 * 
 * This crypto-condition eval solves the problem of nothing-at-stake
 * in a proof of stake consensus system.
 * 
 */

#ifndef CC_COINBASEGUARD_H
#define CC_COINBASEGUARD_H

#include <vector>

#include "CCinclude.h"
#include "streams.h"
#include "script/script.h"

class CStakeParams
{
    public:
        static const uint32_t STAKE_MINPARAMS = 4;
        static const uint32_t STAKE_MAXPARAMS = 5;
        
        uint32_t srcHeight;
        uint32_t blkHeight;
        uint256 prevHash;
        CTxDestination dest;
    
        CStakeParams() : srcHeight(0), blkHeight(0), prevHash(), dest() {}

        CStakeParams(std::vector<std::vector<unsigned char>> vData);

        bool IsValid() { return srcHeight != 0; }
};

bool ValidateStakeTransaction(const CTransaction &stakeTx, CStakeParams &stakeParams);

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout);

bool CoinbaseGuardValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

UniValue CoinbaseGuardInfo();

#endif
