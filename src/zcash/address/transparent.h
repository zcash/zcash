// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_ADDRESS_TRANSPARENT_H
#define ZCASH_ZCASH_ADDRESS_TRANSPARENT_H

#include "zip32.h"

namespace libzcash {
namespace transparent {

class AccountPubKey {
private:
    CChainablePubKey pubkey;
public:
    AccountPubKey(CChainablePubKey pubkey): pubkey(pubkey) {};

    const CChainablePubKey& GetPubKey() const {
        return pubkey;
    }

    std::optional<CPubKey> DeriveExternal(uint32_t addrIndex) const;

    std::optional<CPubKey> DeriveInternal(uint32_t addrIndex) const;

    /**
     * Return the transparent change address for this key a the given diversifier
     * index, if that index is valid for a transparent address and if key
     * derivation is successful.
     */
    std::optional<CKeyID> GetChangeAddress(const diversifier_index_t& j) const;

    /**
     * Search the space of valid child indices starting at the given index, and
     * return the change address corresponding to the first index at which a
     * valid address is derived, along with that index. This will always return
     * a valid address unless the space of valid transparent child indices is
     * exhausted or the provided diversifier index exceeds the maximum allowed
     * non-hardened transparent child index.
     */
    std::optional<std::pair<CKeyID, diversifier_index_t>> FindChangeAddress(diversifier_index_t j) const;

    friend bool operator==(const AccountPubKey& a, const AccountPubKey& b)
    {
        return a.pubkey == b.pubkey;
    }
};

class AccountKey {
private:
    CExtKey accountKey;
    CExtKey external;
    CExtKey internal;

    AccountKey(CExtKey accountKeyIn, CExtKey externalIn, CExtKey internalIn):
        accountKey(accountKeyIn), external(externalIn), internal(internalIn) {}
public:
    static std::optional<AccountKey> ForAccount(
            const HDSeed& mnemonic,
            uint32_t bip44CoinType,
            libzcash::AccountId accountId);

    static HDKeyPath KeyPath(uint32_t bip44CoinType, libzcash::AccountId accountId) {
        return
            "m/44'/" +
            std::to_string(bip44CoinType) + "'/" +
            std::to_string(accountId) + "'";
    }

    static HDKeyPath KeyPath(uint32_t bip44CoinType, libzcash::AccountId accountId, bool external, uint32_t childIndex) {
        return
            AccountKey::KeyPath(bip44CoinType, accountId) + "/" +
            (external ? "0/" : "1/") +
            std::to_string(childIndex);
    }

    /**
     * Generate the key corresponding to the specified index at the "external child"
     * level of the TRANSPARENT path for the account.
     */
    std::optional<CKey> DeriveExternalSpendingKey(uint32_t addrIndex) const;

    /**
     * Generate the key corresponding to the specified index at the "internal child"
     * level of the TRANSPARENT path for the account. This should probably only usually be
     * used at address index 0, but ordinarily it won't need to be used at all since
     * all change should be shielded by default.
     */
    std::optional<CKey> DeriveInternalSpendingKey(uint32_t addrIndex = 0) const;

    /**
     * Return the public key associated with this spending key.
     */
    AccountPubKey ToAccountPubKey() const;

    friend bool operator==(const AccountKey& a, const AccountKey& b)
    {
        return a.accountKey == b.accountKey;
    }
};

} //namespace transparent
} //namespace libzcash

#endif // ZCASH_ZCASH_ADDRESS_TRANSPARENT_H
