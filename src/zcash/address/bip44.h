// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_BIP44_H
#define ZCASH_ZCASH_ADDRESS_BIP44_H

#include "zip32.h"

namespace libzcash {

std::optional<std::pair<CExtKey, HDKeyPath>> DeriveBip44TransparentAccountKey(const HDSeed& seed, uint32_t bip44CoinType, libzcash::AccountId accountId);

class Bip44AccountChains {
private:
    uint256 seedFp;
    libzcash::AccountId accountId;
    uint32_t bip44CoinType;
    CExtKey external;
    CExtKey internal;

    Bip44AccountChains(uint256 seedFpIn, uint32_t bip44CoinTypeIn, libzcash::AccountId accountIdIn, CExtKey externalIn, CExtKey internalIn):
        seedFp(seedFpIn), accountId(accountIdIn), bip44CoinType(bip44CoinTypeIn), external(externalIn), internal(internalIn) {}
public:
    static std::optional<Bip44AccountChains> ForAccount(
            const HDSeed& mnemonic,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    std::optional<std::pair<CExtKey, HDKeyPath>> DeriveExternal(uint32_t addrIndex);
    std::optional<std::pair<CExtKey, HDKeyPath>> DeriveInternal(uint32_t addrIndex);
};

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_BIP44_H
