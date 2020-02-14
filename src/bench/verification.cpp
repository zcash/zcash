// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bench.h"
#include "coins.h"
#include "consensus/upgrades.h"
#include "keystore.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "script/sign.h"
#include "streams.h"
#include "uint256.h"
#include "utilstrencodings.h"
#include "version.h"

#include "librustzcash.h"
#include "sodium.h"

static void ECDSA(benchmark::State& state)
{
    uint32_t consensusBranchId = NetworkUpgradeInfo[Consensus::UPGRADE_OVERWINTER].nBranchId;
    CMutableTransaction mtx;
    mtx.fOverwintered = true;
    mtx.nVersion = SAPLING_TX_VERSION;
    mtx.nVersionGroupId = SAPLING_VERSION_GROUP_ID;

    CKey key;
    key.MakeNewKey(false);
    CBasicKeyStore keystore;
    keystore.AddKeyPubKey(key, key.GetPubKey());
    CKeyID hash = key.GetPubKey().GetID();
    CScript scriptPubKey = GetScriptForDestination(hash);

    // Benchmark a transaction containing a single input and output.
    auto nInputs = 1;
    for(uint32_t ij = 0; ij < nInputs; ij++) {
        uint32_t i = mtx.vin.size();
        uint256 prevId;
        prevId.SetHex("0000000000000000000000000000000000000000000000000000000000000100");
        COutPoint outpoint(prevId, i);

        mtx.vin.resize(mtx.vin.size() + 1);
        mtx.vin[i].prevout = outpoint;
        mtx.vin[i].scriptSig = CScript();

        mtx.vout.resize(mtx.vout.size() + 1);
        mtx.vout[i].nValue = 1000;
        mtx.vout[i].scriptPubKey = CScript() << OP_1;
    }

    // sign all inputs
    for(uint32_t i = 0; i < mtx.vin.size(); i++) {
        bool hashSigned = SignSignature(keystore, scriptPubKey, mtx, i, 1000, SIGHASH_ALL, consensusBranchId);
        assert(hashSigned);
    }

    CTransaction tx;
    CDataStream ssout(SER_NETWORK, PROTOCOL_VERSION);
    ssout << mtx;
    ssout >> tx;

    ScriptError error;

    while (state.KeepRunning()) {
        VerifyScript(
            tx.vin[0].scriptSig,
            scriptPubKey,
            SCRIPT_VERIFY_P2SH,
            TransactionSignatureChecker(&tx, 0, 1000),
            consensusBranchId,
            &error);
    }
}

static void JoinSplitSig(benchmark::State& state)
{
    uint256 joinSplitPubKey;
    unsigned char joinSplitPrivKey[crypto_sign_SECRETKEYBYTES];
    uint256 dataToBeSigned;
    std::array<unsigned char, 64> joinSplitSig;

    crypto_sign_keypair(joinSplitPubKey.begin(), joinSplitPrivKey);
    crypto_sign_detached(&joinSplitSig[0], nullptr, dataToBeSigned.begin(), 32, joinSplitPrivKey);

    while (state.KeepRunning()) {
        // Declared with warn_unused_result.
        auto res = crypto_sign_verify_detached(
            &joinSplitSig[0],
            dataToBeSigned.begin(), 32,
            joinSplitPubKey.begin());
    }
}

static void SaplingSpend(benchmark::State& state)
{
    SpendDescription spend;
    CDataStream ss(
        ParseHex("8c6cf86bbb83bf0d075e5bd9bb4b5cd56141577be69f032880b11e26aa32aa5ef09fd00899e4b469fb11f38e9d09dc0379f0b11c23b5fe541765f76695120a03f0261d32af5d2a2b1e5c9a04200cd87d574dc42349de9790012ce560406a8a876a1e54cfcdc0eb74998abec2a9778330eeb2a0ac0e41d0c9ed5824fbd0dbf7da930ab299966ce333fd7bc1321dada0817aac5444e02c754069e218746bf879d5f2a20a8b028324fb2c73171e63336686aa5ec2e6e9a08eb18b87c14758c572f4531ccf6b55d09f44beb8b47563be4eff7a52598d80959dd9c9fee5ac4783d8370cb7d55d460053d3e067b5f9fe75ff2722623fb1825fcba5e9593d4205b38d1f502ff03035463043bd393a5ee039ce75a5d54f21b395255df6627ef96751566326f7d4a77d828aa21b1827282829fcbc42aad59cdb521e1a3aaa08b99ea8fe7fff0a04da31a52260fc6daeccd79bb877bdd8506614282258e15b3fe74bf71a93f4be3b770119edf99a317b205eea7d5ab800362b97384273888106c77d633600"),
        SER_NETWORK,
        PROTOCOL_VERSION);
    ss >> spend;
    uint256 dataToBeSigned = uint256S("0x2dbf83fe7b88a7cbd80fac0c719483906bb9a0c4fc69071e4780d5f2c76e592c");

    auto ctx = librustzcash_sapling_verification_ctx_init();

    while (state.KeepRunning()) {
        librustzcash_sapling_check_spend(
            ctx,
            spend.cv.begin(),
            spend.anchor.begin(),
            spend.nullifier.begin(),
            spend.rk.begin(),
            spend.zkproof.begin(),
            spend.spendAuthSig.begin(),
            dataToBeSigned.begin());
    }

    librustzcash_sapling_verification_ctx_free(ctx);
}

static void SaplingOutput(benchmark::State& state)
{
    OutputDescription output;
    CDataStream ss(
        ParseHex("edd742af18857e5ec2d71d346a7fe2ac97c137339bd5268eea86d32e0ff4f38f76213fa8cfed3347ac4e8572dd88aff395c0c10a59f8b3f49d2bc539ed6c726667e29d4763f914ddd0abf1cdfa84e44de87c233434c7e69b8b5b8f4623c8aa444163425bae5cef842972fed66046c1c6ce65c866ad894d02e6e6dcaae7a962d9f2ef95757a09c486928e61f0f7aed90ad0a542b0d3dc5fe140dfa7626b9315c77e03b055f19cbacd21a866e46f06c00e0c7792b2a590a611439b510a9aaffcf1073bad23e712a9268b36888e3727033eee2ab4d869f54a843f93b36ef489fb177bf74b41a9644e5d2a0a417c6ac1c8869bc9b83273d453f878ed6fd96b82a5939903f7b64ecaf68ea16e255a7fb7cc0b6d8b5608a1c6b0ed3024cc62c2f0f9c5cfc7b431ae6e9d40815557aa1d010523f9e1960de77b2274cb6710d229d475c87ae900183206ba90cb5bbc8ec0df98341b82726c705e0308ca5dc08db4db609993a1046dfb43dfd8c760be506c0bed799bb2205fc29dc2e654dce731034a23b0aaf6da0199248702ee0523c159f41f4cbfff6c35ace4dd9ae834e44e09c76a0cbdda1d3f6a2c75ad71212daf9575ab5f09ca148718e667f29ddf18c8a330a86ace18a86e89454653902aa393c84c6b694f27d0d42e24e7ac9fe34733de5ec15f5066081ce912c62c1a804a2bb4dedcef7cc80274f6bb9e89e2fce91dc50d6a73c8aefb9872f1cf3524a92626a0b8f39bbf7bf7d96ca2f770fc04d7f457021c536a506a187a93b2245471ddbfb254a71bc4a0d72c8d639a31c7b1920087ffca05c24214157e2e7b28184e91989ef0b14f9b34c3dc3cc0ac64226b9e337095870cb0885737992e120346e630a416a9b217679ce5a778fb15779c136bcecca5efe79012013d77d90b4e99dd22c8f35bc77121716e160d05bd30d288ee8886390ee436f85bdc9029df888a3a3326d9d4ddba5cb5318b3274928829d662e96fea1d601f7a306251ed8c6cc4e5a3a7a98c35a3650482a0eee08f3b4c2da9b22947c96138f1505c2f081f8972d429f3871f32bef4aaa51aa6945df8e9c9760531ac6f627d17c1518202818a91ca304fb4037875c666060597976144fcbbc48a776a2c61beb9515fa8f3ae6d3a041d320a38a8ac75cb47bb9c866ee497fc3cd13299970c4b369c1c2ceb4220af082fbecdd8114492a8e4d713b5a73396fd224b36c1185bd5e20d683e6c8db35346c47ae7401988255da7cfffdced5801067d4d296688ee8fe424b4a8a69309ce257eefb9345ebfda3f6de46bb11ec94133e1f72cd7ac54934d6cf17b3440800e70b80ebc7c7bfc6fb0fc2c"),
        SER_NETWORK,
        PROTOCOL_VERSION);
    ss >> output;

    auto ctx = librustzcash_sapling_verification_ctx_init();

    while (state.KeepRunning()) {
        librustzcash_sapling_check_output(
            ctx,
            output.cv.begin(),
            output.cm.begin(),
            output.ephemeralKey.begin(),
            output.zkproof.begin());
    }

    librustzcash_sapling_verification_ctx_free(ctx);
}

BENCHMARK(ECDSA);
BENCHMARK(JoinSplitSig);
BENCHMARK(SaplingSpend);
BENCHMARK(SaplingOutput);
