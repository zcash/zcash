// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/paymentdisclosure.h"

#include "key_io.h"
#include "util.h"

#include <rust/ed25519.h>

std::string PaymentDisclosureInfo::ToString() const {
    KeyIO keyIO(Params());
    return strprintf("PaymentDisclosureInfo(version=%d, esk=%s, joinSplitPrivKey=<omitted>, address=%s)",
        version, esk.ToString(), keyIO.EncodePaymentAddress(zaddr));
}

std::string PaymentDisclosure::ToString() const {
    std::string s = HexStr(payloadSig.bytes, payloadSig.bytes + ED25519_SIGNATURE_LEN);
    return strprintf("PaymentDisclosure(payload=%s, payloadSig=%s)", payload.ToString(), s);
}

std::string PaymentDisclosurePayload::ToString() const {
    KeyIO keyIO(Params());
    return strprintf("PaymentDisclosurePayload(version=%d, esk=%s, txid=%s, js=%d, n=%d, address=%s, message=%s)",
        version, esk.ToString(), txid.ToString(), js, n, keyIO.EncodePaymentAddress(zaddr), message);
}

PaymentDisclosure::PaymentDisclosure(
    const Ed25519VerificationKey& joinSplitPubKey,
    const PaymentDisclosureKey& key,
    const PaymentDisclosureInfo& info,
    const std::string& message)
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

    // Compute payload signature member variable
    if (!ed25519_sign(
        &info.joinSplitPrivKey,
        dataToBeSigned.begin(), 32,
        &payloadSig))
    {
        throw std::runtime_error("ed25519_sign failed");
    }

    // Sanity check
    if (!ed25519_verify(
        &joinSplitPubKey,
        &payloadSig,
        dataToBeSigned.begin(), 32))
    {
        throw std::runtime_error("ed25519_verify failed");
    }

    std::string sigString = HexStr(payloadSig.bytes, payloadSig.bytes + ED25519_SIGNATURE_LEN);
    LogPrint("paymentdisclosure", "Payment Disclosure: signature = %s\n", sigString);
}
