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

#ifndef STAKEGUARD_H
#define STAKEGUARD_H

#include <vector>

#include "CCinclude.h"
#include "streams.h"
#include "script/script.h"

#define DEFAULT_STAKE_TXFEE 0

bool UnpackStakeOpRet(const CTransaction &stakeTx, std::vector<std::vector<unsigned char>> &vData);

bool GetStakeParams(const CTransaction &stakeTx, CStakeParams &stakeParams);

bool ValidateStakeTransaction(const CTransaction &stakeTx, CStakeParams &stakeParams, bool validateSig = true);

bool ValidateMatchingStake(const CTransaction &ccTx, uint32_t voutNum, const CTransaction &stakeTx, bool &cheating);

bool MakeGuardedOutput(CAmount value, CPubKey &dest, CTransaction &stakeTx, CTxOut &vout);

bool MakeCheatEvidence(CMutableTransaction &mtx, const CTransaction &ccTx, uint32_t voutNum, const CTransaction &cheatTx);

bool StakeGuardValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn);

bool IsStakeGuardInput(const CScript &scriptSig);

UniValue StakeGuardInfo();

#endif
