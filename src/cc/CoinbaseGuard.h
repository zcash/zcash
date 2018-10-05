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

#define DEFAULT_STAKE_TXFEE 0

class CStakeParams
{
    public:
        static const uint32_t STAKE_MINPARAMS = 4;
        static const uint32_t STAKE_MAXPARAMS = 5;
        
        uint32_t srcHeight;
        uint32_t blkHeight;
        uint256 prevHash;
        CPubKey pk;
    
        CStakeParams() : srcHeight(0), blkHeight(0), prevHash(), pk() {}

        CStakeParams(const std::vector<std::vector<unsigned char>> &vData);

        CStakeParams(uint32_t _srcHeight, uint32_t _blkHeight, const uint256 &_prevHash, const CPubKey &_pk) :
            srcHeight(_srcHeight), blkHeight(_blkHeight), prevHash(_prevHash), pk(_pk) {}

        std::vector<unsigned char> AsVector()
        {
            std::vector<unsigned char> ret;
            CScript scr = CScript();
            scr << OPRETTYPE_STAKEPARAMS;
            scr << srcHeight;
            scr << blkHeight;
            scr << std::vector<unsigned char>(prevHash.begin(), prevHash.end());
            
            if (pk.IsValid())
            {
                scr << std::vector<unsigned char>(pk.begin(), pk.end());
            }
                                    
            ret = std::vector<unsigned char>(scr.begin(), scr.end());
            return ret;
        }

        bool IsValid() { return srcHeight != 0; }
};

bool UnpackStakeOpRet(const CTransaction &stakeTx, std::vector<std::vector<unsigned char>> &vData);

bool GetStakeParams(const CTransaction &stakeTx, CStakeParams &stakeParams);

bool ValidateStakeTransaction(const CTransaction &stakeTx, CStakeParams &stakeParams);

bool ValidateCheatingStake(const CTransaction &ccTx, uint32_t voutNum, const CTransaction &cheatTx);

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout);

bool MakeCheatEvidence(CMutableTransaction &mtx, const CTransaction &ccTx, uint32_t voutNum, const CTransaction &cheatTx);

bool CoinbaseGuardValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

UniValue CoinbaseGuardInfo();

#endif
