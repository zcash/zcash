// Copyright (c) 2018 Michael Toutonghi
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "hash.h"
#include "nonce.h"
#include <cstring>

extern char ASSETCHAINS_SYMBOL[65];

arith_uint256 CPOSNonce::entropyMask = UintToArith256(uint256S("00000000000000000000000000000000ffffffffffffffffffffffff00000000"));
arith_uint256 CPOSNonce::posDiffMask = UintToArith256(uint256S("00000000000000000000000000000000000000000000000000000000ffffffff"));

bool CPOSNonce::NewPOSActive(int32_t height)
{
    if ((strcmp(ASSETCHAINS_SYMBOL, "VRSC") == 0) && (height < (96480 + 100)))
        return false;
    else
        return true;
}

bool CPOSNonce::NewNonceActive(int32_t height)
{
    if ((strcmp(ASSETCHAINS_SYMBOL, "VRSC") == 0) && (height < 96480))
        return false;
    else
        return true;
}

void CPOSNonce::SetPOSEntropy(const uint256 &pastHash, uint256 txid, int32_t voutNum)
{
    // get low 96 bits of past hash and put it in top 96 bits of low 128 bits of nonce
    CVerusHashWriter hashWriter = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

    // first hash the pastHash, txid, and voutNum, to create a combined 96 bits, which will be used in the nonce
    hashWriter << pastHash;
    hashWriter << txid;
    hashWriter << voutNum;

    arith_uint256 arNonce = (UintToArith256(*this) & posDiffMask) |
        (UintToArith256(hashWriter.GetHash()) & entropyMask);

    // printf("before %s\n", ArithToUint256(arNonce).GetHex().c_str());

    hashWriter.Reset();
    hashWriter << ArithToUint256(arNonce);

    *this = CPOSNonce(ArithToUint256((UintToArith256(hashWriter.GetHash()) << 128) | arNonce));

    // printf("after  %s\n", this->GetHex().c_str());
}

bool CPOSNonce::CheckPOSEntropy(const uint256 &pastHash, uint256 txid, int32_t voutNum)
{
    // get low 96 bits of past hash and put it in top 96 bits of low 128 bits of nonce
    CVerusHashWriter hashWriter = CVerusHashWriter(SER_GETHASH, PROTOCOL_VERSION);

    // first hash the pastHash, txid, and voutNum, to create a combined 96 bits, which will be used in the nonce
    hashWriter << pastHash;
    hashWriter << txid;
    hashWriter << voutNum;

    arith_uint256 arNonce = (UintToArith256(*this) & posDiffMask) |
        (UintToArith256(hashWriter.GetHash()) & entropyMask);

    hashWriter.Reset();
    hashWriter << ArithToUint256(arNonce);

    return UintToArith256(*this) == (UintToArith256(hashWriter.GetHash()) << 128 | arNonce);
}

