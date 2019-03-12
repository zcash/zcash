// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet/paymentdisclosure.h"

#include "key_io.h"
#include "util.h"

std::string PaymentDisclosureInfo::ToString() const {
    return strprintf("PaymentDisclosureInfo(version=%d, esk=%s, joinSplitPrivKey=<omitted>, address=%s)",
        version, esk.ToString(), EncodePaymentAddress(zaddr));
}

std::string PaymentDisclosure::ToString() const {
    std::string s = HexStr(payloadSig.begin(), payloadSig.end());
    return strprintf("PaymentDisclosure(payload=%s, payloadSig=%s)", payload.ToString(), s);
}

std::string PaymentDisclosurePayload::ToString() const {
    return strprintf("PaymentDisclosurePayload(version=%d, esk=%s, txid=%s, js=%d, n=%d, address=%s, message=%s)",
        version, esk.ToString(), txid.ToString(), js, n, EncodePaymentAddress(zaddr), message);
}

PaymentDisclosure::PaymentDisclosure(const uint256 &joinSplitPubKey, const PaymentDisclosureKey &key, const PaymentDisclosureInfo &info, const std::string &message)
{
    // Populate payload member variable
    payload.version = info.version; // experimental = 0, production = 1 etc.
    payload.esk = info.esk;
    payload.txid = key.hash;
    payload.js = key.js;
    payload.n = key.n;
    payload.zaddr = info.zaddr;
    payload.message = message;

    // Serialize and hash the payload to generate a signature
    uint256 dataToBeSigned = SerializeHash(payload, SER_GETHASH, 0);

    LogPrint("paymentdisclosure", "Payment Disclosure: signing raw payload = %s\n", dataToBeSigned.ToString());

    // Prepare buffer to store ed25519 key pair in libsodium-compatible format
    unsigned char bufferKeyPair[64];
    memcpy(&bufferKeyPair[0], info.joinSplitPrivKey.begin(), 32);
    memcpy(&bufferKeyPair[32], joinSplitPubKey.begin(), 32);

    // Compute payload signature member variable
    if (!(crypto_sign_detached(payloadSig.data(), NULL,
                               dataToBeSigned.begin(), 32,
                               &bufferKeyPair[0]
                               ) == 0))
    {
        throw std::runtime_error("crypto_sign_detached failed");
    }

    // Sanity check
    if (!(crypto_sign_verify_detached(payloadSig.data(),
                                      dataToBeSigned.begin(), 32,
                                      joinSplitPubKey.begin()) == 0))
    {
        throw std::runtime_error("crypto_sign_verify_detached failed");
    }

    std::string sigString = HexStr(payloadSig.data(), payloadSig.data() + payloadSig.size());
    LogPrint("paymentdisclosure", "Payment Disclosure: signature = %s\n", sigString);
}
