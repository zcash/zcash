// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transaction_builder.h"

#include "main.h"
#include "script/script.h"

#include <boost/variant.hpp>
#include <librustzcash.h>

SpendDescriptionInfo::SpendDescriptionInfo(
    libzcash::SaplingExpandedSpendingKey xsk,
    libzcash::SaplingNote note,
    uint256 anchor,
    ZCSaplingIncrementalWitness witness) : xsk(xsk), note(note), anchor(anchor), witness(witness)
{
    librustzcash_sapling_generate_r(alpha.begin());
}

TransactionBuilder::TransactionBuilder(
    const Consensus::Params& consensusParams,
    int nHeight) : consensusParams(consensusParams), nHeight(nHeight)
{
    mtx = CreateNewContextualCMutableTransaction(consensusParams, nHeight);
}

bool TransactionBuilder::AddSaplingSpend(
    libzcash::SaplingExpandedSpendingKey xsk,
    libzcash::SaplingNote note,
    uint256 anchor,
    ZCSaplingIncrementalWitness witness)
{
    // Consistency check: all anchors must equal the first one
    if (!spends.empty()) {
        if (spends[0].anchor != anchor) {
            return false;
        }
    }

    spends.emplace_back(xsk, note, anchor, witness);
    mtx.valueBalance += note.value();
    return true;
}

void TransactionBuilder::AddSaplingOutput(
    libzcash::SaplingFullViewingKey from,
    libzcash::SaplingPaymentAddress to,
    CAmount value,
    std::array<unsigned char, ZC_MEMO_SIZE> memo)
{
    auto note = libzcash::SaplingNote(to, value);
    outputs.emplace_back(from.ovk, note, memo);
    mtx.valueBalance -= value;
}

boost::optional<CTransaction> TransactionBuilder::Build()
{
    auto ctx = librustzcash_sapling_proving_ctx_init();

    // Create Sapling SpendDescriptions
    for (auto spend : spends) {
        auto cm = spend.note.cm();
        auto nf = spend.note.nullifier(
            spend.xsk.full_viewing_key(), spend.witness.position());
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
                spend.xsk.full_viewing_key().ak.begin(),
                spend.xsk.nsk.begin(),
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

    // Calculate SignatureHash
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
            spends[i].xsk.ask.begin(),
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
    return CTransaction(mtx);
}
