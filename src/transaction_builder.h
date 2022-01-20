// Copyright (c) 2018 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_TRANSACTION_BUILDER_H
#define ZCASH_TRANSACTION_BUILDER_H

#include "coins.h"
#include "consensus/params.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include "random.h"
#include "script/script.h"
#include "script/standard.h"
#include "uint256.h"
#include "util/match.h"
#include "zcash/Address.hpp"
#include "zcash/IncrementalMerkleTree.hpp"
#include "zcash/JoinSplit.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

#include <optional>

#define NO_MEMO {{0xF6}}

struct SpendDescriptionInfo {
    libzcash::SaplingExpandedSpendingKey expsk;
    libzcash::SaplingNote note;
    uint256 alpha;
    uint256 anchor;
    SaplingWitness witness;

    SpendDescriptionInfo(
        libzcash::SaplingExpandedSpendingKey expsk,
        libzcash::SaplingNote note,
        uint256 anchor,
        SaplingWitness witness);
};

struct OutputDescriptionInfo {
    uint256 ovk;
    libzcash::SaplingNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;

    OutputDescriptionInfo(
        uint256 ovk,
        libzcash::SaplingNote note,
        std::array<unsigned char, ZC_MEMO_SIZE> memo) : ovk(ovk), note(note), memo(memo) {}

    std::optional<OutputDescription> Build(void* ctx);
};

struct JSDescriptionInfo {
    Ed25519VerificationKey joinSplitPubKey;
    uint256 anchor;
    // We store references to these so they are correctly randomised for the caller.
    std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS>& inputs;
    std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS>& outputs;
    CAmount vpub_old;
    CAmount vpub_new;

    JSDescriptionInfo(
        Ed25519VerificationKey joinSplitPubKey,
        uint256 anchor,
        std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS>& inputs,
        std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS>& outputs,
        CAmount vpub_old,
        CAmount vpub_new) : joinSplitPubKey(joinSplitPubKey), anchor(anchor), inputs(inputs), outputs(outputs), vpub_old(vpub_old), vpub_new(vpub_new) {}

    JSDescription BuildDeterministic(
        bool computeProof = true, // Set to false in some tests
        uint256* esk = nullptr    // payment disclosure
    );

    JSDescription BuildRandomized(
        std::array<size_t, ZC_NUM_JS_INPUTS>& inputMap,
        std::array<size_t, ZC_NUM_JS_OUTPUTS>& outputMap,
        bool computeProof = true, // Set to false in some tests
        uint256* esk = nullptr,   // payment disclosure
        std::function<int(int)> gen = GetRandInt
    );
};

struct TransparentInputInfo {
    CScript scriptPubKey;
    CAmount value;

    TransparentInputInfo(
        CScript scriptPubKey,
        CAmount value) : scriptPubKey(scriptPubKey), value(value) {}
};

class TransactionBuilderResult {
private:
    std::optional<CTransaction> maybeTx;
    std::optional<std::string> maybeError;
public:
    TransactionBuilderResult() = delete;
    TransactionBuilderResult(const CTransaction& tx);
    TransactionBuilderResult(const std::string& error);
    bool IsTx();
    bool IsError();
    CTransaction GetTxOrThrow();
    std::string GetError();
};

class TransactionBuilder
{
private:
    std::optional<bool> usingSprout;
    Consensus::Params consensusParams;
    int nHeight;
    const CKeyStore* keystore;
    const CCoinsViewCache* coinsView;
    CCriticalSection* cs_coinsView;
    CMutableTransaction mtx;
    CAmount fee = 10000;

    std::vector<SpendDescriptionInfo> spends;
    std::vector<OutputDescriptionInfo> outputs;
    std::vector<libzcash::JSInput> jsInputs;
    std::vector<libzcash::JSOutput> jsOutputs;
    std::vector<TransparentInputInfo> tIns;

    std::optional<std::pair<uint256, libzcash::SaplingPaymentAddress>> saplingChangeAddr;
    std::optional<libzcash::SproutPaymentAddress> sproutChangeAddr;
    std::optional<CTxDestination> tChangeAddr;

public:
    TransactionBuilder() {}
    TransactionBuilder(
        const Consensus::Params& consensusParams,
        int nHeight,
        CKeyStore* keyStore = nullptr,
        CCoinsViewCache* coinsView = nullptr,
        CCriticalSection* cs_coinsView = nullptr);

    void SetExpiryHeight(uint32_t nExpiryHeight);

    void SetFee(CAmount fee);

    // Throws if the anchor does not match the anchor used by
    // previously-added Sapling spends.
    void AddSaplingSpend(
        libzcash::SaplingExpandedSpendingKey expsk,
        libzcash::SaplingNote note,
        uint256 anchor,
        SaplingWitness witness);

    void AddSaplingOutput(
        uint256 ovk,
        libzcash::SaplingPaymentAddress to,
        CAmount value,
        std::array<unsigned char, ZC_MEMO_SIZE> memo = NO_MEMO);

    // Throws if the anchor does not match the anchor used by
    // previously-added Sprout inputs.
    void AddSproutInput(
        libzcash::SproutSpendingKey sk,
        libzcash::SproutNote note,
        SproutWitness witness);

    void AddSproutOutput(
        libzcash::SproutPaymentAddress to,
        CAmount value,
        std::array<unsigned char, ZC_MEMO_SIZE> memo = NO_MEMO);

    // Assumes that the value correctly corresponds to the provided UTXO.
    void AddTransparentInput(COutPoint utxo, CScript scriptPubKey, CAmount value);

    void AddTransparentOutput(const CTxDestination& to, CAmount value);

    void SendChangeTo(const libzcash::RecipientAddress& changeAddr, const uint256& ovk);
    void SendChangeToSprout(const libzcash::SproutPaymentAddress& changeAddr);

    TransactionBuilderResult Build();

private:
    void CheckOrSetUsingSprout();

    void CreateJSDescriptions();

    void CreateJSDescription(
        uint64_t vpub_old,
        uint64_t vpub_new,
        std::array<libzcash::JSInput, ZC_NUM_JS_INPUTS> vjsin,
        std::array<libzcash::JSOutput, ZC_NUM_JS_OUTPUTS> vjsout,
        std::array<size_t, ZC_NUM_JS_INPUTS>& inputMap,
        std::array<size_t, ZC_NUM_JS_OUTPUTS>& outputMap);
};

#endif // ZCASH_TRANSACTION_BUILDER_H
