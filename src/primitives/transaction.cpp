// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "primitives/transaction.h"
#include "policy/policy.h"

#include "hash.h"
#include "tinyformat.h"
#include "util/strencodings.h"
#include "zip317.h"

#include <rust/transaction.h>

SaplingBundle::SaplingBundle(
    const std::vector<SpendDescription>& vShieldedSpend,
    const std::vector<OutputDescription>& vShieldedOutput,
    const CAmount& valueBalanceSapling,
    const binding_sig_t& bindingSig)
        : valueBalanceSapling(valueBalanceSapling), bindingSigSapling(bindingSig)
{
    for (auto &spend : vShieldedSpend) {
        vSpendsSapling.emplace_back(spend.cv, spend.nullifier, spend.rk);
        if (anchorSapling.IsNull()) {
            anchorSapling = spend.anchor;
        } else {
            assert(anchorSapling == spend.anchor);
        }
        vSpendProofsSapling.push_back(spend.zkproof);
        vSpendAuthSigSapling.push_back(spend.spendAuthSig);
    }
    for (auto &output : vShieldedOutput) {
        vOutputsSapling.emplace_back(
            output.cv,
            output.cmu,
            output.ephemeralKey,
            output.encCiphertext,
            output.outCiphertext);
        vOutputProofsSapling.push_back(output.zkproof);
    }
}

std::vector<SpendDescription> SaplingBundle::GetV4ShieldedSpend()
{
    std::vector<SpendDescription> vShieldedSpend;
    for (int i = 0; i < vSpendsSapling.size(); i++) {
        auto spend = vSpendsSapling[i];
        vShieldedSpend.emplace_back(
            spend.cv,
            anchorSapling,
            spend.nullifier,
            spend.rk,
            vSpendProofsSapling[i],
            vSpendAuthSigSapling[i]);
    }
    return vShieldedSpend;
}

std::vector<OutputDescription> SaplingBundle::GetV4ShieldedOutput()
{
    std::vector<OutputDescription> vShieldedOutput;
    for (int i = 0; i < vOutputsSapling.size(); i++) {
        auto output = vOutputsSapling[i];
        vShieldedOutput.emplace_back(
            output.cv,
            output.cmu,
            output.ephemeralKey,
            output.encCiphertext,
            output.outCiphertext,
            vOutputProofsSapling[i]);
    }
    return vShieldedOutput;
}

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

std::string SaplingOutPoint::ToString() const
{
    return strprintf("SaplingOutPoint(%s, %u)", hash.ToString().substr(0, 10), n);
}

std::string OrchardOutPoint::ToString() const
{
    return strprintf("OrchardOutPoint(%s, %u)", hash.ToString().substr(0, 10), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
}

CAmount CTxOut::GetDustThreshold() const
{
    // See the comment on ONE_THIRD_DUST_THRESHOLD_RATE in policy.h.
    static const CFeeRate oneThirdDustThresholdRate {ONE_THIRD_DUST_THRESHOLD_RATE};

    if (scriptPubKey.IsUnspendable())
        return 0;

    // A typical spendable txout is 34 bytes, and will need a txin of at
    // least 148 bytes to spend. With ONE_THIRD_DUST_THRESHOLD_RATE == 100,
    // the dust threshold for such a txout would be
    // 3*floor(100*(34 + 148)/1000) zats = 54 zats.
    size_t nSize = GetSerializeSize(*this, SER_DISK, 0) + 148u;
    return 3*oneThirdDustThresholdRate.GetFee(nSize);
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, HexStr(scriptPubKey).substr(0, 30));
}


CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::SPROUT_MIN_CURRENT_VERSION), fOverwintered(false), nVersionGroupId(0), nExpiryHeight(0), nLockTime(0), valueBalanceSapling(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), fOverwintered(tx.fOverwintered), nVersionGroupId(tx.nVersionGroupId), nExpiryHeight(tx.nExpiryHeight),
                                                                   nConsensusBranchId(tx.GetConsensusBranchId()),
                                                                   vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime),
                                                                   valueBalanceSapling(tx.GetValueBalanceSapling()), vShieldedSpend(tx.vShieldedSpend), vShieldedOutput(tx.vShieldedOutput),
                                                                   orchardBundle(tx.GetOrchardBundle()),
                                                                   vJoinSplit(tx.vJoinSplit), joinSplitPubKey(tx.joinSplitPubKey), joinSplitSig(tx.joinSplitSig),
                                                                   bindingSig(tx.bindingSig)
{
}

uint256 CMutableTransaction::GetHash() const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    uint256 hash;
    if (!zcash_transaction_digests(
        reinterpret_cast<const unsigned char*>(ss.data()),
        ss.size(),
        hash.begin(),
        nullptr))
    {
        throw std::ios_base::failure("CMutableTransaction::GetHash: Invalid transaction format");
    }
    return hash;
}

uint256 CMutableTransaction::GetAuthDigest() const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    uint256 authDigest;
    if (!zcash_transaction_digests(
        reinterpret_cast<const unsigned char*>(ss.data()),
        ss.size(),
        nullptr,
        authDigest.begin()))
    {
        throw std::ios_base::failure("CMutableTransaction::GetAuthDigest: Invalid transaction format");
    }
    return authDigest;
}

void CTransaction::UpdateHash() const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    if (!zcash_transaction_digests(
        reinterpret_cast<const unsigned char*>(ss.data()),
        ss.size(),
        const_cast<uint256*>(&wtxid.hash)->begin(),
        const_cast<uint256*>(&wtxid.authDigest)->begin()))
    {
        throw std::ios_base::failure("CTransaction::UpdateHash: Invalid transaction format");
    }
}

CTransaction::CTransaction() : nVersion(CTransaction::SPROUT_MIN_CURRENT_VERSION),
                               fOverwintered(false), nVersionGroupId(0), nExpiryHeight(0),
                               nConsensusBranchId(std::nullopt),
                               vin(), vout(), nLockTime(0),
                               valueBalanceSapling(0), vShieldedSpend(), vShieldedOutput(),
                               orchardBundle(),
                               vJoinSplit(), joinSplitPubKey(), joinSplitSig(),
                               bindingSig() { }

CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), fOverwintered(tx.fOverwintered), nVersionGroupId(tx.nVersionGroupId), nExpiryHeight(tx.nExpiryHeight),
                                                            nConsensusBranchId(tx.nConsensusBranchId),
                                                            vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime),
                                                            valueBalanceSapling(tx.valueBalanceSapling), vShieldedSpend(tx.vShieldedSpend), vShieldedOutput(tx.vShieldedOutput),
                                                            orchardBundle(tx.orchardBundle),
                                                            vJoinSplit(tx.vJoinSplit), joinSplitPubKey(tx.joinSplitPubKey), joinSplitSig(tx.joinSplitSig),
                                                            bindingSig(tx.bindingSig)
{
    UpdateHash();
}

// Protected constructor which only derived classes can call.
// For developer testing only.
CTransaction::CTransaction(
    const CMutableTransaction &tx,
    bool evilDeveloperFlag) : nVersion(tx.nVersion), fOverwintered(tx.fOverwintered), nVersionGroupId(tx.nVersionGroupId), nExpiryHeight(tx.nExpiryHeight),
                              nConsensusBranchId(tx.nConsensusBranchId),
                              vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime),
                              valueBalanceSapling(tx.valueBalanceSapling), vShieldedSpend(tx.vShieldedSpend), vShieldedOutput(tx.vShieldedOutput),
                              orchardBundle(tx.orchardBundle),
                              vJoinSplit(tx.vJoinSplit), joinSplitPubKey(tx.joinSplitPubKey), joinSplitSig(tx.joinSplitSig),
                              bindingSig(tx.bindingSig)
{
    assert(evilDeveloperFlag);
}

CTransaction::CTransaction(CMutableTransaction &&tx) : nVersion(tx.nVersion),
                                                       fOverwintered(tx.fOverwintered), nVersionGroupId(tx.nVersionGroupId),
                                                       nConsensusBranchId(tx.nConsensusBranchId),
                                                       vin(std::move(tx.vin)), vout(std::move(tx.vout)),
                                                       nLockTime(tx.nLockTime), nExpiryHeight(tx.nExpiryHeight),
                                                       valueBalanceSapling(tx.valueBalanceSapling),
                                                       vShieldedSpend(std::move(tx.vShieldedSpend)), vShieldedOutput(std::move(tx.vShieldedOutput)),
                                                       orchardBundle(std::move(tx.orchardBundle)),
                                                       vJoinSplit(std::move(tx.vJoinSplit)),
                                                       joinSplitPubKey(std::move(tx.joinSplitPubKey)), joinSplitSig(std::move(tx.joinSplitSig)),
                                                       bindingSig(std::move(tx.bindingSig))
{
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<bool*>(&fOverwintered) = tx.fOverwintered;
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<uint32_t*>(&nVersionGroupId) = tx.nVersionGroupId;
    nConsensusBranchId = tx.nConsensusBranchId;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<uint32_t*>(&nExpiryHeight) = tx.nExpiryHeight;
    valueBalanceSapling = tx.valueBalanceSapling;
    *const_cast<std::vector<SpendDescription>*>(&vShieldedSpend) = tx.vShieldedSpend;
    *const_cast<std::vector<OutputDescription>*>(&vShieldedOutput) = tx.vShieldedOutput;
    orchardBundle = tx.orchardBundle;
    *const_cast<std::vector<JSDescription>*>(&vJoinSplit) = tx.vJoinSplit;
    *const_cast<ed25519::VerificationKey*>(&joinSplitPubKey) = tx.joinSplitPubKey;
    *const_cast<ed25519::Signature*>(&joinSplitSig) = tx.joinSplitSig;
    *const_cast<binding_sig_t*>(&bindingSig) = tx.bindingSig;
    *const_cast<uint256*>(&wtxid.hash) = tx.wtxid.hash;
    *const_cast<uint256*>(&wtxid.authDigest) = tx.wtxid.authDigest;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (const auto& out : vout) {
        if (!MoneyRange(out.nValue)) {
            throw std::runtime_error("CTransaction::GetValueOut(): nValue out of range");
        }
        nValueOut += out.nValue;
        if (!MoneyRange(nValueOut)) {
            throw std::runtime_error("CTransaction::GetValueOut(): nValueOut out of range");
        }
    }

    if (valueBalanceSapling <= 0) {
        // NB: negative valueBalanceSapling "takes" money from the transparent value pool just as outputs do
        if (valueBalanceSapling < -MAX_MONEY) {
            throw std::runtime_error("CTransaction::GetValueOut(): valueBalanceSapling out of range");
        }
        nValueOut += -valueBalanceSapling;

        if (!MoneyRange(nValueOut)) {
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
        }
    }

    auto valueBalanceOrchard = orchardBundle.GetValueBalance();
    if (valueBalanceOrchard <= 0) {
        // NB: negative valueBalanceOrchard "takes" money from the transparent value pool just as outputs do
        if (valueBalanceOrchard < -MAX_MONEY) {
            throw std::runtime_error("CTransaction::GetValueOut(): valueBalanceOrchard out of range");
        }
        nValueOut += -valueBalanceOrchard;

        if (!MoneyRange(nValueOut)) {
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
        }
    }

    for (const auto& jsDescription : vJoinSplit) {
        // NB: vpub_old "takes" money from the transparent value pool just as outputs do
        if (!MoneyRange(jsDescription.vpub_old)) {
            throw std::runtime_error("CTransaction::GetValueOut(): vpub_old out of range");
        }
        nValueOut += jsDescription.vpub_old;
        if (!MoneyRange(nValueOut)) {
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
        }
    }
    return nValueOut;
}

CAmount CTransaction::GetShieldedValueIn() const
{
    CAmount nValue = 0;

    if (valueBalanceSapling >= 0) {
        // NB: positive valueBalanceSapling "gives" money to the transparent value pool just as inputs do
        if (valueBalanceSapling > MAX_MONEY) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): valueBalanceSapling out of range");
        }
        nValue += valueBalanceSapling;

        if (!MoneyRange(nValue)) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): value out of range");
        }
    }

    auto valueBalanceOrchard = orchardBundle.GetValueBalance();
    if (valueBalanceOrchard >= 0) {
        // NB: positive valueBalanceOrchard "gives" money to the transparent value pool just as inputs do
        if (valueBalanceOrchard > MAX_MONEY) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): valueBalanceOrchard out of range");
        }
        nValue += valueBalanceOrchard;
        if (!MoneyRange(nValue)) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): nValue out of range");
        }
    }

    for (const auto& jsDescription : vJoinSplit) {
        // NB: vpub_new "gives" money to the transparent value pool just as inputs do
        if (!MoneyRange(jsDescription.vpub_new)) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): vpub_new out of range");
        }
        nValue += jsDescription.vpub_new;
        if (!MoneyRange(nValue)) {
            throw std::runtime_error("CTransaction::GetShieldedValueIn(): value out of range");
        }
    }

    if (IsCoinBase() && nValue != 0) {
        throw std::runtime_error("CTransaction::GetShieldedValueIn(): shielded value of inputs must be zero in coinbase transactions.");
    }

    return nValue;
}

CAmount CTransaction::GetConventionalFee() const {
    return CalculateConventionalFee(GetLogicalActionCount());
}

size_t CTransaction::GetLogicalActionCount() const {
    return CalculateLogicalActionCount(
            vin,
            vout,
            vJoinSplit.size(),
            vShieldedSpend.size(),
            vShieldedOutput.size(),
            orchardBundle.GetNumActions());
}

std::string CTransaction::ToString() const
{
    std::string str;
    if (!fOverwintered) {
        str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
            GetHash().ToString().substr(0,10),
            nVersion,
            vin.size(),
            vout.size(),
            nLockTime);
    } else if (nVersion >= SAPLING_MIN_TX_VERSION) {
        str += strprintf("CTransaction(hash=%s, ver=%d, fOverwintered=%d, nVersionGroupId=%08x, vin.size=%u, vout.size=%u, nLockTime=%u, nExpiryHeight=%u, valueBalanceSapling=%u, vSaplingSpend.size=%u, vSaplingOutput.size=%u",
            GetHash().ToString().substr(0,10),
            nVersion,
            fOverwintered,
            nVersionGroupId,
            vin.size(),
            vout.size(),
            nLockTime,
            nExpiryHeight,
            valueBalanceSapling,
            vShieldedSpend.size(),
            vShieldedOutput.size());
        if (nVersion >= ZIP225_MIN_TX_VERSION) {
            str += strprintf(", nConsensusBranchId=%08x, valueBalanceOrchard=%u, vOrchardAction.size=%u",
                nConsensusBranchId.value_or(0),
                orchardBundle.GetValueBalance(),
                orchardBundle.GetNumActions());
        }
        str += ")\n";
    } else if (nVersion >= 3) {
        str += strprintf("CTransaction(hash=%s, ver=%d, fOverwintered=%d, nVersionGroupId=%08x, vin.size=%u, vout.size=%u, nLockTime=%u, nExpiryHeight=%u)\n",
            GetHash().ToString().substr(0,10),
            nVersion,
            fOverwintered,
            nVersionGroupId,
            vin.size(),
            vout.size(),
            nLockTime,
            nExpiryHeight);
    }
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}
