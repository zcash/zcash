// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "zcash_script.h"

#include "consensus/upgrades.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "script/interpreter.h"
#include "version.h"

namespace {

/** A class that deserializes a single CTransaction one time. */
class TxInputStream
{
public:
    TxInputStream(int nTypeIn, int nVersionIn, const unsigned char *txTo, size_t txToLen) :
    m_type(nTypeIn),
    m_version(nVersionIn),
    m_data(txTo),
    m_remaining(txToLen)
    {}

    void read(char* pch, size_t nSize)
    {
        if (nSize > m_remaining)
            throw std::ios_base::failure(std::string(__func__) + ": end of data");

        if (pch == NULL)
            throw std::ios_base::failure(std::string(__func__) + ": bad destination buffer");

        if (m_data == NULL)
            throw std::ios_base::failure(std::string(__func__) + ": bad source buffer");

        memcpy(pch, m_data, nSize);
        m_remaining -= nSize;
        m_data += nSize;
    }

    template<typename T>
    TxInputStream& operator>>(T& obj)
    {
        ::Unserialize(*this, obj);
        return *this;
    }

    int GetVersion() const { return m_version; }
    int GetType() const { return m_type; }
private:
    const int m_type;
    const int m_version;
    const unsigned char* m_data;
    size_t m_remaining;
};

inline int set_error(zcash_script_error* ret, zcash_script_error serror)
{
    if (ret)
        *ret = serror;
    return 0;
}

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;

/// Copy of GetLegacySigOpCount from main.cpp commit c4b2ef7c4
unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    for (const CTxIn& txin : tx.vin)
    {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    for (const CTxOut& txout : tx.vout)
    {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}
}

struct PrecomputedTransaction {
    const CTransaction tx;
    const PrecomputedTransactionData txdata;

    PrecomputedTransaction(CTransaction txIn) : tx(txIn), txdata(txIn) {}
};

void* zcash_script_new_precomputed_tx(
    const unsigned char* txTo,
    unsigned int txToLen,
    zcash_script_error* err)
{
    try {
        TxInputStream stream(SER_NETWORK, PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx;
        stream >> tx;
        if (GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) != txToLen) {
            set_error(err, zcash_script_ERR_TX_SIZE_MISMATCH);
            return nullptr;
        }

        // Deserializing the tx did not error.
        set_error(err, zcash_script_ERR_OK);
        auto preTx = new PrecomputedTransaction(tx);
        return preTx;
    } catch (const std::exception&) {
        set_error(err, zcash_script_ERR_TX_DESERIALIZE); // Error deserializing
        return nullptr;
    }
}

void zcash_script_free_precomputed_tx(void* pre_preTx)
{
    PrecomputedTransaction* preTx = static_cast<PrecomputedTransaction*>(pre_preTx);
    delete preTx;
    preTx = nullptr;
}

int zcash_script_verify_precomputed(
    const void* pre_preTx,
    unsigned int nIn,
    const unsigned char* scriptPubKey,
    unsigned int scriptPubKeyLen,
    int64_t amount,
    unsigned int flags,
    uint32_t consensusBranchId,
    zcash_script_error* err)
{
    const PrecomputedTransaction* preTx = static_cast<const PrecomputedTransaction*>(pre_preTx);
    if (nIn >= preTx->tx.vin.size())
        return set_error(err, zcash_script_ERR_TX_INDEX);

    // Regardless of the verification result, the tx did not error.
    set_error(err, zcash_script_ERR_OK);
    return VerifyScript(
        preTx->tx.vin[nIn].scriptSig,
        CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen),
        flags,
        TransactionSignatureChecker(&preTx->tx, nIn, amount, preTx->txdata),
        consensusBranchId,
        NULL);
}

int zcash_script_verify(
    const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
    int64_t amount,
    const unsigned char *txTo, unsigned int txToLen,
    unsigned int nIn, unsigned int flags,
    uint32_t consensusBranchId,
    zcash_script_error* err)
{
    try {
        TxInputStream stream(SER_NETWORK, PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx;
        stream >> tx;
        if (nIn >= tx.vin.size())
            return set_error(err, zcash_script_ERR_TX_INDEX);
        if (GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) != txToLen)
            return set_error(err, zcash_script_ERR_TX_SIZE_MISMATCH);

         // Regardless of the verification result, the tx did not error.
        set_error(err, zcash_script_ERR_OK);
        PrecomputedTransactionData txdata(tx);
        return VerifyScript(
            tx.vin[nIn].scriptSig,
            CScript(scriptPubKey, scriptPubKey + scriptPubKeyLen),
            flags,
            TransactionSignatureChecker(&tx, nIn, amount, txdata),
            consensusBranchId,
            NULL);
    } catch (const std::exception&) {
        return set_error(err, zcash_script_ERR_TX_DESERIALIZE); // Error deserializing
    }
}

unsigned int zcash_script_legacy_sigop_count_precomputed(
    const void* pre_preTx,
    zcash_script_error* err)
{
    const PrecomputedTransaction* preTx = static_cast<const PrecomputedTransaction*>(pre_preTx);

    // The current implementation of this method never errors.
    set_error(err, zcash_script_ERR_OK);

    return GetLegacySigOpCount(preTx->tx);
}

unsigned int zcash_script_legacy_sigop_count(
    const unsigned char *txTo,
    unsigned int txToLen,
    zcash_script_error* err)
{
    try {
        TxInputStream stream(SER_NETWORK, PROTOCOL_VERSION, txTo, txToLen);
        CTransaction tx;
        stream >> tx;
        if (GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) != txToLen) {
            set_error(err, zcash_script_ERR_TX_SIZE_MISMATCH);
            return UINT_MAX;
        }

        // Deserializing the tx did not error.
        set_error(err, zcash_script_ERR_OK);

        return GetLegacySigOpCount(tx);
    } catch (const std::exception&) {
        set_error(err, zcash_script_ERR_TX_DESERIALIZE); // Error deserializing
        return UINT_MAX;
    }
}

unsigned int zcash_script_version()
{
    // Just use the API version for now
    return ZCASH_SCRIPT_API_VER;
}
