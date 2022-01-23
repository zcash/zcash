// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "utiltest.h"

#include "consensus/upgrades.h"
#include "transaction_builder.h"

#include <array>

#include <rust/ed25519.h>

// Sprout
CMutableTransaction GetValidSproutReceiveTransaction(
                                const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                uint32_t versionGroupId, /* = SAPLING_VERSION_GROUP_ID */
                                int32_t version /* = SAPLING_TX_VERSION */) {
    // We removed the ability to create pre-Sapling Sprout transactions
    assert(version >= SAPLING_TX_VERSION);

    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = versionGroupId;
    mtx.nVersion = version;
    mtx.vin.resize(2);
    if (randomInputs) {
        mtx.vin[0].prevout.hash = GetRandHash();
        mtx.vin[1].prevout.hash = GetRandHash();
    } else {
        mtx.vin[0].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000001");
        mtx.vin[1].prevout.hash = uint256S("0000000000000000000000000000000000000000000000000000000000000002");
    }
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.n = 0;
    std::vector<CTxOut> allPrevOutputs;
    allPrevOutputs.resize(mtx.vin.size());

    // Generate an ephemeral keypair.
    Ed25519SigningKey joinSplitPrivKey;
    ed25519_generate_keypair(&joinSplitPrivKey, &mtx.joinSplitPubKey);

    std::array<libzcash::JSInput, 2> inputs = {
        libzcash::JSInput(), // dummy input
        libzcash::JSInput() // dummy input
    };

    std::array<libzcash::JSOutput, 2> outputs = {
        libzcash::JSOutput(sk.address(), value),
        libzcash::JSOutput(sk.address(), value)
    };

    // Prepare JoinSplits
    uint256 rt;
    auto jsdesc = JSDescriptionInfo(mtx.joinSplitPubKey, rt,
                          inputs, outputs, 2*value, 0).BuildDeterministic(false);
    mtx.vJoinSplit.push_back(jsdesc);

    // Consider: The following is a bit misleading (given the name of this function)
    // and should perhaps be changed, but currently a few tests in test_wallet.cpp
    // depend on this happening.
    if (version >= 4) {
        // Shielded Output
        OutputDescription od;
        mtx.vShieldedOutput.push_back(od);
    }

    // Empty output script.
    uint32_t consensusBranchId = SPROUT_BRANCH_ID;
    CScript scriptCode;
    CTransaction signTx(mtx);
    const PrecomputedTransactionData txdata(signTx, allPrevOutputs);
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId, txdata);

    // Add the signature
    assert(ed25519_sign(
        &joinSplitPrivKey,
        dataToBeSigned.begin(), 32,
        &mtx.joinSplitSig));

    return mtx;
}

CWalletTx GetValidSproutReceive(const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                uint32_t versionGroupId, /* = SAPLING_VERSION_GROUP_ID */
                                int32_t version /* = SAPLING_TX_VERSION */)
{
    CMutableTransaction mtx = GetValidSproutReceiveTransaction(
        sk, value, randomInputs, versionGroupId, version
    );
    CTransaction tx {mtx};
    CWalletTx wtx {NULL, tx};
    return wtx;
}

CWalletTx GetInvalidCommitmentSproutReceive(
                                const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                uint32_t versionGroupId, /* = SAPLING_VERSION_GROUP_ID */
                                int32_t version /* = SAPLING_TX_VERSION */)
{
    CMutableTransaction mtx = GetValidSproutReceiveTransaction(
        sk, value, randomInputs, versionGroupId, version
    );
    mtx.vJoinSplit[0].commitments[0] = uint256();
    mtx.vJoinSplit[0].commitments[1] = uint256();
    CTransaction tx {mtx};
    CWalletTx wtx {NULL, tx};
    return wtx;
}

libzcash::SproutNote GetSproutNote(const libzcash::SproutSpendingKey& sk,
                                   const CTransaction& tx, size_t js, size_t n) {
    ZCNoteDecryption decryptor {sk.receiving_key()};
    auto hSig = ZCJoinSplit::h_sig(
        tx.vJoinSplit[js].randomSeed,
        tx.vJoinSplit[js].nullifiers,
        tx.joinSplitPubKey);
    auto note_pt = libzcash::SproutNotePlaintext::decrypt(
        decryptor,
        tx.vJoinSplit[js].ciphertexts[n],
        tx.vJoinSplit[js].ephemeralKey,
        hSig,
        (unsigned char) n);
    return note_pt.note(sk.address());
}

CWalletTx GetValidSproutSpend(const libzcash::SproutSpendingKey& sk,
                              const libzcash::SproutNote& note,
                              CAmount value) {
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.vout.resize(2);
    mtx.vout[0].nValue = value;
    mtx.vout[1].nValue = 0;

    // Generate an ephemeral keypair.
    Ed25519SigningKey joinSplitPrivKey;
    ed25519_generate_keypair(&joinSplitPrivKey, &mtx.joinSplitPubKey);

    // Fake tree for the unused witness
    SproutMerkleTree tree;

    libzcash::JSOutput dummyout;
    libzcash::JSInput dummyin;

    {
        if (note.value() > value) {
            libzcash::SproutSpendingKey dummykey = libzcash::SproutSpendingKey::random();
            libzcash::SproutPaymentAddress dummyaddr = dummykey.address();
            dummyout = libzcash::JSOutput(dummyaddr, note.value() - value);
        } else if (note.value() < value) {
            libzcash::SproutSpendingKey dummykey = libzcash::SproutSpendingKey::random();
            libzcash::SproutPaymentAddress dummyaddr = dummykey.address();
            libzcash::SproutNote dummynote(dummyaddr.a_pk, (value - note.value()), uint256(), uint256());
            tree.append(dummynote.cm());
            dummyin = libzcash::JSInput(tree.witness(), dummynote, dummykey);
        }
    }

    tree.append(note.cm());

    std::array<libzcash::JSInput, 2> inputs = {
        libzcash::JSInput(tree.witness(), note, sk),
        dummyin
    };

    std::array<libzcash::JSOutput, 2> outputs = {
        dummyout, // dummy output
        libzcash::JSOutput() // dummy output
    };

    // Prepare JoinSplits
    uint256 rt = tree.root();
    auto jsdesc = JSDescriptionInfo(mtx.joinSplitPubKey, rt,
                          inputs, outputs, 0, value).BuildDeterministic(false);
    mtx.vJoinSplit.push_back(jsdesc);

    // Empty output script.
    uint32_t consensusBranchId = SPROUT_BRANCH_ID;
    CScript scriptCode;
    CTransaction signTx(mtx);
    assert(signTx.vin.size() == 0);
    const PrecomputedTransactionData txdata(signTx, {});
    uint256 dataToBeSigned = SignatureHash(scriptCode, signTx, NOT_AN_INPUT, SIGHASH_ALL, 0, consensusBranchId, txdata);

    // Add the signature
    assert(ed25519_sign(
        &joinSplitPrivKey,
        dataToBeSigned.begin(), 32,
        &mtx.joinSplitSig));
    CTransaction tx {mtx};
    CWalletTx wtx {NULL, tx};
    return wtx;
}

const CChainParams& RegtestActivateOverwinter() {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    return Params();
}

void RegtestDeactivateOverwinter() {
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

// Sapling
const Consensus::Params& RegtestActivateSapling() {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    return Params().GetConsensus();
}

void RegtestDeactivateSapling() {
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
}

const CChainParams& RegtestActivateBlossom(bool updatePow, int blossomActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, blossomActivationHeight);
    if (updatePow) {
        UpdateRegtestPow(32, 16, uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), false);
    }
    return Params();
}

void RegtestDeactivateBlossom() {
    UpdateRegtestPow(0, 0, uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"), true);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    SelectParams(CBaseChainParams::MAIN);
}

const Consensus::Params& RegtestActivateHeartwood(bool updatePow, int heartwoodActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, heartwoodActivationHeight);
    if (updatePow) {
        UpdateRegtestPow(32, 16, uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), false);
    }
    return Params().GetConsensus();
}

void RegtestDeactivateHeartwood() {
    UpdateRegtestPow(0, 0, uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"), true);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    SelectParams(CBaseChainParams::MAIN);
}

const Consensus::Params& RegtestActivateCanopy(bool updatePow, int canopyActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_CANOPY, canopyActivationHeight);
    if (updatePow) {
        UpdateRegtestPow(32, 16, uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), false);
    }
    return Params().GetConsensus();
}

const Consensus::Params& RegtestActivateCanopy() {
    return RegtestActivateCanopy(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
}

void RegtestDeactivateCanopy() {
    UpdateRegtestPow(0, 0, uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"), true);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_CANOPY, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    SelectParams(CBaseChainParams::MAIN);
}

const Consensus::Params& RegtestActivateNU5(bool updatePow, int nu5ActivationHeight) {
    SelectParams(CBaseChainParams::REGTEST);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_CANOPY, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_NU5, nu5ActivationHeight);
    if (updatePow) {
        UpdateRegtestPow(32, 16, uint256S("0007ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"), false);
    }
    return Params().GetConsensus();
}

const Consensus::Params& RegtestActivateNU5() {
    return RegtestActivateNU5(false, Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
}

void RegtestDeactivateNU5() {
    UpdateRegtestPow(0, 0, uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f"), true);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_NU5, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_CANOPY, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_HEARTWOOD, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_BLOSSOM, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_SAPLING, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    UpdateNetworkUpgradeParameters(Consensus::UPGRADE_OVERWINTER, Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT);
    SelectParams(CBaseChainParams::MAIN);
}

libzcash::SaplingExtendedSpendingKey GetTestMasterSaplingSpendingKey() {
    SecureString mnemonic("abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon art");
    auto seed{MnemonicSeed::ForPhrase(English, mnemonic).value()};
    return libzcash::SaplingExtendedSpendingKey::Master(seed);
}

CKey AddTestCKeyToKeyStore(CBasicKeyStore& keyStore) {
    KeyIO keyIO(Params());
    CKey tsk = keyIO.DecodeSecret(T_SECRET_REGTEST);
    keyStore.AddKey(tsk);
    return tsk;
}

TestSaplingNote GetTestSaplingNote(const libzcash::SaplingPaymentAddress& pa, CAmount value) {
    // Generate dummy Sapling note
    libzcash::SaplingNote note(pa, value, libzcash::Zip212Enabled::BeforeZip212);
    uint256 cm = note.cmu().value();
    SaplingMerkleTree tree;
    tree.append(cm);
    return { note, tree };
}

CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStore,
                                 const libzcash::SaplingExtendedSpendingKey &sk,
                                 CAmount value) {
    // From taddr
    CKey tsk = AddTestCKeyToKeyStore(keyStore);
    auto scriptPubKey = GetScriptForDestination(tsk.GetPubKey().GetID());
    // To zaddr
    auto fvk = sk.expsk.full_viewing_key();
    auto pa = sk.ToXFVK().DefaultAddress();

    auto builder = TransactionBuilder(consensusParams, 1, &keyStore);
    builder.SetFee(0);
    builder.AddTransparentInput(COutPoint(), scriptPubKey, value);
    builder.AddSaplingOutput(fvk.ovk, pa, value, {});

    CTransaction tx = builder.Build().GetTxOrThrow();
    CWalletTx wtx {NULL, tx};
    return wtx;
}
