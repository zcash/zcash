// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_KEY_CONSTANTS_H
#define ZCASH_KEY_CONSTANTS_H

#include <string>

class KeyConstants
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        ZCPAYMENT_ADDRESS,
        ZCSPENDING_KEY,
        ZCVIEWING_KEY,

        MAX_BASE58_TYPES
    };

    enum Bech32Type {
        SAPLING_PAYMENT_ADDRESS,
        SAPLING_FULL_VIEWING_KEY,
        SAPLING_INCOMING_VIEWING_KEY,
        SAPLING_EXTENDED_SPEND_KEY,
        SAPLING_EXTENDED_FVK,

        MAX_BECH32_TYPES
    };

    virtual std::string NetworkIDString() const =0;
    virtual const std::vector<unsigned char>& Base58Prefix(Base58Type type) const =0;
    virtual const std::string& Bech32HRP(Bech32Type type) const =0;
};

#endif // ZCASH_KEY_CONSTANTS_H
