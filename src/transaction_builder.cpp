// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transaction_builder.h"

#include "main.h"
#include "pubkey.h"
#include "script/sign.h"

#include <boost/variant.hpp>
#include <librustzcash.h>

SpendDescriptionInfo::SpendDescriptionInfo(
    libzcash::SaplingExpandedSpendingKey expsk,
    libzcash::SaplingNote note,
    uint256 anchor,
    SaplingWitness witness) : expsk(expsk), note(note), anchor(anchor), witness(witness)
{
    librustzcash_sapling_generate_r(alpha.begin());
}

TransactionBuilder::TransactionBuilder(
    const Consensus::Params& consensusParams,
    int nHeight,
    CKeyStore* keystore) : consensusParams(consensusParams), nHeight(nHeight), keystore(keystore)
{
    mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight);
}

bool TransactionBuilder::AddSaplingSpend(
    libzcash::SaplingExpandedSpendingKey expsk,
    libzcash::SaplingNote note,
    uint256 anchor,
    SaplingWitness witness)
{
    // Consistency check: all anchors must equal the first one
    if (!spends.empty()) {
        if (spends[0].anchor != anchor) {
            return false;
        }
    }

    spends.emplace_back(expsk, note, anchor, witness);
    mtx.valueBalance += note.value();
    return true;
}

void TransactionBuilder::AddSaplingOutput(
    uint256 ovk,
    libzcash::SaplingPaymentAddress to,
    CAmount value,
    std::array<unsigned char, ZC_MEMO_SIZE> memo)
{
    auto note = libzcash::SaplingNote(to, value);
    outputs.emplace_back(ovk, note, memo);
    mtx.valueBalance -= value;
}

void TransactionBuilder::AddTransparentInput(COutPoint utxo, CScript scriptPubKey, CAmount value, uint32_t _nSequence)
{
    if (keystore == nullptr) {
        if (!scriptPubKey.IsPayToCryptoCondition())
        {
            throw std::runtime_error("Cannot add transparent inputs to a TransactionBuilder without a keystore, except with crypto conditions");
        }
    }

    mtx.vin.emplace_back(utxo);
    mtx.vin[mtx.vin.size() - 1].nSequence = _nSequence;
    tIns.emplace_back(scriptPubKey, value);
}

bool TransactionBuilder::AddTransparentOutput(CTxDestination& to, CAmount value)
{
    if (!IsValidDestination(to)) {
        return false;
    }

    CScript scriptPubKey = GetScriptForDestination(to);
    CTxOut out(value, scriptPubKey);
    mtx.vout.push_back(out);
    return true;
}

bool TransactionBuilder::AddOpRetLast()
{
    CScript s;
    if (opReturn)
    {
        s = opReturn.value();
        CTxOut out(0, s);
        mtx.vout.push_back(out);
    }
    return true;
}

void TransactionBuilder::AddOpRet(CScript &s)
{
    opReturn.emplace(CScript(s));
}

void TransactionBuilder::SetFee(CAmount fee)
{
    this->fee = fee;
}

void TransactionBuilder::SendChangeTo(libzcash::SaplingPaymentAddress changeAddr, uint256 ovk)
{
    zChangeAddr = std::make_pair(ovk, changeAddr);
    tChangeAddr = boost::none;
}

bool TransactionBuilder::SendChangeTo(CTxDestination& changeAddr)
{
    if (!IsValidDestination(changeAddr)) {
        return false;
    }

    tChangeAddr = changeAddr;
    zChangeAddr = boost::none;

    return true;
}

boost::optional<CTransaction> TransactionBuilder::Build()
{
    //
    // Consistency checks
    //

    // Valid change
    CAmount change = mtx.valueBalance - fee;
    for (auto tIn : tIns) {
        change += tIn.value;
    }
    for (auto tOut : mtx.vout) {
        change -= tOut.nValue;
    }
    if (change < 0) {
        return boost::none;
    }

    //
    // Change output
    //

    if (change > 0) {
        // Send change to the specified change address. If no change address
        // was set, send change to the first Sapling address given as input.
        if (zChangeAddr) {
            AddSaplingOutput(zChangeAddr->first, zChangeAddr->second, change);
        } else if (tChangeAddr) {
            // tChangeAddr has already been validated.
            assert(AddTransparentOutput(tChangeAddr.value(), change));
        } else if (!spends.empty()) {
            auto fvk = spends[0].expsk.full_viewing_key();
            auto note = spends[0].note;
            libzcash::SaplingPaymentAddress changeAddr(note.d, note.pk_d);
            AddSaplingOutput(fvk.ovk, changeAddr, change);
        } else {
            return boost::none;
        }
    }

    //
    // Sapling spends and outputs
    //

    auto ctx = librustzcash_sapling_proving_ctx_init();

    // Create Sapling SpendDescriptions
    for (auto spend : spends) {
        auto cm = spend.note.cm();
        auto nf = spend.note.nullifier(
            spend.expsk.full_viewing_key(), spend.witness.position());
        if (!(cm && nf)) {
            librustzcash_sapling_proving_ctx_free(ctx);
            return boost::none;
        }

        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << spend.witness.path();
        std::vector<unsigned char> witness(ss.begin(), ss.end());

        SpendDescription sdesc;
        if (!librustzcash_sapling_spend_proof(
                ctx,
                spend.expsk.full_viewing_key().ak.begin(),
                spend.expsk.nsk.begin(),
                spend.note.d.data(),
                spend.note.r.begin(),
                spend.alpha.begin(),
                spend.note.value(),
                spend.anchor.begin(),
                witness.data(),
                sdesc.cv.begin(),
                sdesc.rk.begin(),
                sdesc.zkproof.data())) {
            librustzcash_sapling_proving_ctx_free(ctx);
            return boost::none;
        }

        sdesc.anchor = spend.anchor;
        sdesc.nullifier = *nf;
        mtx.vShieldedSpend.push_back(sdesc);
    }

    // Create Sapling OutputDescriptions
    for (auto output : outputs) {
        auto cm = output.note.cm();
        if (!cm) {
            librustzcash_sapling_proving_ctx_free(ctx);
            return boost::none;
        }

        libzcash::SaplingNotePlaintext notePlaintext(output.note, output.memo);

        auto res = notePlaintext.encrypt(output.note.pk_d);
        if (!res) {
            librustzcash_sapling_proving_ctx_free(ctx);
            return boost::none;
        }
        auto enc = res.get();
        auto encryptor = enc.second;

        OutputDescription odesc;
        if (!librustzcash_sapling_output_proof(
                ctx,
                encryptor.get_esk().begin(),
                output.note.d.data(),
                output.note.pk_d.begin(),
                output.note.r.begin(),
                output.note.value(),
                odesc.cv.begin(),
                odesc.zkproof.begin())) {
            librustzcash_sapling_proving_ctx_free(ctx);
            return boost::none;
        }

        odesc.cm = *cm;
        odesc.ephemeralKey = encryptor.get_epk();
        odesc.encCiphertext = enc.first;

        libzcash::SaplingOutgoingPlaintext outPlaintext(output.note.pk_d, encryptor.get_esk());
        odesc.outCiphertext = outPlaintext.encrypt(
            output.ovk,
            odesc.cv,
            odesc.cm,
            encryptor);
        mtx.vShieldedOutput.push_back(odesc);
    }

    // add op_return if there is one to add
    AddOpRetLast();

    //
    // Signatures
    //

    auto consensusBranchId = CurrentEpochBranchId(nHeight, consensusParams);

    // Empty output script.
    uint256 dataToBeSigned;
    CScript scriptCode;
    try {
        dataToBeSigned = SignatureHash(scriptCode, mtx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId);
    } catch (std::logic_error ex) {
        librustzcash_sapling_proving_ctx_free(ctx);
        return boost::none;
    }

    // Create Sapling spendAuth and binding signatures
    for (size_t i = 0; i < spends.size(); i++) {
        librustzcash_sapling_spend_sig(
            spends[i].expsk.ask.begin(),
            spends[i].alpha.begin(),
            dataToBeSigned.begin(),
            mtx.vShieldedSpend[i].spendAuthSig.data());
    }
    librustzcash_sapling_binding_sig(
        ctx,
        mtx.valueBalance,
        dataToBeSigned.begin(),
        mtx.bindingSig.data());

    librustzcash_sapling_proving_ctx_free(ctx);

    // Transparent signatures
    CTransaction txNewConst(mtx);
    for (int nIn = 0; nIn < mtx.vin.size(); nIn++) {
        auto tIn = tIns[nIn];
        SignatureData sigdata;
        bool signSuccess = ProduceSignature(
            TransactionSignatureCreator(
                keystore, &txNewConst, nIn, tIn.value, SIGHASH_ALL),
            tIn.scriptPubKey, sigdata, consensusBranchId);

        if (!signSuccess) {
            return boost::none;
        } else {
            UpdateTransaction(mtx, nIn, sigdata);
        }
    }

    return CTransaction(mtx);
}
