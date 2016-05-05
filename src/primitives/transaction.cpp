// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "hash.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

#include "zerocash/PourProver.h"
#include "zerocash/PourTransaction.h"

template<std::size_t N>
boost::array<std::vector<unsigned char>, N> uint256_to_array(const boost::array<uint256, N>& in) {
    boost::array<std::vector<unsigned char>, N> result;
    for (size_t i = 0; i < N; i++) {
        result[i] = std::vector<unsigned char>(in[i].begin(), in[i].end());
    }

    return result;
}

template<std::size_t N>
boost::array<uint256, N> unsigned_char_vector_array_to_uint256_array(const boost::array<std::vector<unsigned char>, N>& in) {
    boost::array<uint256, N> result;
    for (size_t i = 0; i < N; i++) {
        result[i] = uint256(in[i]);
    }

    return result;
}

CPourTx::CPourTx(ZerocashParams& params,
            const CScript& scriptPubKey,
            const uint256& anchor,
            const boost::array<PourInput, ZC_NUM_JS_INPUTS>& inputs,
            const boost::array<PourOutput, ZC_NUM_JS_OUTPUTS>& outputs,
            CAmount vpub_old,
            CAmount vpub_new) : scriptSig(), scriptPubKey(scriptPubKey), vpub_old(vpub_old), vpub_new(vpub_new), anchor(anchor)
{
    uint256 scriptPubKeyHash;
    {
        CHashWriter ss(SER_GETHASH, 0);
        ss << scriptPubKey;
        scriptPubKeyHash = ss.GetHash();
    }

    PourTransaction pourtx(params,
                           std::vector<unsigned char>(scriptPubKeyHash.begin(), scriptPubKeyHash.end()),
                           std::vector<unsigned char>(anchor.begin(), anchor.end()),
                           std::vector<PourInput>(inputs.begin(), inputs.end()),
                           std::vector<PourOutput>(outputs.begin(), outputs.end()),
                           vpub_old,
                           vpub_new);

    boost::array<std::vector<unsigned char>, ZC_NUM_JS_INPUTS> serials_bv;
    boost::array<std::vector<unsigned char>, ZC_NUM_JS_OUTPUTS> commitments_bv;
    boost::array<std::vector<unsigned char>, ZC_NUM_JS_INPUTS> macs_bv;

    proof = pourtx.unpack(serials_bv, commitments_bv, macs_bv, ciphertexts, ephemeralKey);
    serials = unsigned_char_vector_array_to_uint256_array(serials_bv);
    commitments = unsigned_char_vector_array_to_uint256_array(commitments_bv);
    macs = unsigned_char_vector_array_to_uint256_array(macs_bv);
}

bool CPourTx::Verify(ZerocashParams& params) const {
    // Compute the hash of the scriptPubKey.
    uint256 scriptPubKeyHash;
    {
        CHashWriter ss(SER_GETHASH, 0);
        ss << scriptPubKey;
        scriptPubKeyHash = ss.GetHash();
    }

    return PourProver::VerifyProof(
        params,
        std::vector<unsigned char>(scriptPubKeyHash.begin(), scriptPubKeyHash.end()),
        std::vector<unsigned char>(anchor.begin(), anchor.end()),
        vpub_old,
        vpub_new,
        uint256_to_array<ZC_NUM_JS_INPUTS>(serials),
        uint256_to_array<ZC_NUM_JS_OUTPUTS>(commitments),
        uint256_to_array<ZC_NUM_JS_INPUTS>(macs),
        proof
    );
}

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
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
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
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

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), vpour(tx.vpour) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

void CTransaction::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

CTransaction::CTransaction() : nVersion(CTransaction::CURRENT_VERSION), vin(), vout(), nLockTime(0), vpour() { }

CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), vpour(tx.vpour) {
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<std::vector<CPourTx>*>(&vpour) = tx.vpour;
    *const_cast<uint256*>(&hash) = tx.hash;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        nValueOut += it->nValue;
        if (!MoneyRange(it->nValue) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
    }

    for (std::vector<CPourTx>::const_iterator it(vpour.begin()); it != vpour.end(); ++it)
    {
        // NB: vpub_old "takes" money from the value pool just as outputs do
        nValueOut += it->vpub_old;

        if (!MoneyRange(it->vpub_old) || !MoneyRange(nValueOut))
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
    }
    return nValueOut;
}

CAmount CTransaction::GetPourValueIn() const
{
    CAmount nValue = 0;
    for (std::vector<CPourTx>::const_iterator it(vpour.begin()); it != vpour.end(); ++it)
    {
        // NB: vpub_new "gives" money to the value pool just as inputs do
        nValue += it->vpub_new;

        if (!MoneyRange(it->vpub_new) || !MoneyRange(nValue))
            throw std::runtime_error("CTransaction::GetPourValueIn(): value out of range");
    }

    return nValue;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        unsigned int offset = 41U + std::min(110U, (unsigned int)it->scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    return nTxSize;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}
