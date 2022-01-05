// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_BIP44_H
#define ZCASH_ZCASH_ADDRESS_BIP44_H

#include "zip32.h"

namespace libzcash {

HDKeyPath Bip44TransparentAccountKeyPath(uint32_t bip44CoinType, libzcash::AccountId accountId);

/**
 * Derive a transparent extended key in compressed format for the specified
 * seed, bip44 coin type, and account ID.
 */
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

    /**
     * Generate the key corresponding to the specified index at the "external child"
     * level of the BIP44 path for the account.
     */
    std::optional<std::pair<CKey, HDKeyPath>> DeriveExternal(uint32_t addrIndex);

    /**
     * Generate the key corresponding to the specified index at the "internal child"
     * level of the BIP44 path for the account. This should probably only usually be
     * used at address index 0, but ordinarily it won't need to be used at all since
     * all change should be shielded by default.
     */
    std::optional<std::pair<CKey, HDKeyPath>> DeriveInternal(uint32_t addrIndex = 0);
};

} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_BIP44_H
