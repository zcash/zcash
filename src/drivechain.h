// Copyright (c) 2009-2025 Satoshi Nakamoto
// Copyright (c) 2009-2025 The Bitcoin Core developers
// Copyright (c) 2015-2025 The Zcash developers
// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef L2L_DRIVECHAIN_H
#define L2L_DRIVECHAIN_H

#include <amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <string>

class CBlock;

struct DrivechainDeposit {
    std::string strDest;
    CAmount amount;
    CMutableTransaction mtx; // Mainchain deposit transaction
    uint32_t nBurnIndex; // Deposit burn output index
    uint32_t nTx; // Deposit transaction number in mainchain block
    uint256 hashMainchainBlock;
};

// Bitcoin-patched RPC client interface

bool DrivechainRPCGetBTCBlockCount(int& nBlocks);


// BMM validation & cache

// DAT files to save BMM info
bool ReadBMMCache();
bool WriteBMMCache();

// Check if we already verified BMM for this sidechain block
bool HaveVerifiedBMM(const uint256& hashBlock);

// Cache that we verified BMM for this sidechain block
void CacheVerifiedBMM(const uint256& hashBlock);

bool VerifyBMM(const CBlock& block);


// Deposit validation & DB

bool GetUnpaidDrivechainDeposits(std::vector<DrivechainDeposit>& vDeposit);

std::string GenerateDepositAddress(const std::string& strDestIn);

bool ParseDepositAddress(const std::string& strAddressIn, std::string& strAddressOut, unsigned int& nSidechainOut);

bool IsDrivechainDepositScript(const CScript& script);

bool VerifyDrivechainDeposit(const CTxOut& out);

#endif // L2L_DRIVECHAIN_H
