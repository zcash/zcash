#include <gtest/gtest.h>

#include "base58.h"
#include "chainparams.h"
#include "wallet/wallet.h"

CZCSpendingKey testSpendingKey("TKWTVvdbYaXSa1Red2X7PgpoomQnDZMPECKDqn5QhTjDvCabvbr8");
// Raw tx taking transparent input and spending it to two addresses:
// 0: TKWREumuAmKX3xJJ5MadYWqwtD76msJgYyEBp6vLLX5W1HzgcEQX
// 1: TKWTVvdbYaXSa1Red2X7PgpoomQnDZMPECKDqn5QhTjDvCabvbr8
const auto testRawJS = ParseHex("02000000012dc18a2c0993c343d892deb9e81ffb5ce016079c4ecdca4f80299579242c99030000000000ffffffff000000000001a0f95600000000000000000000000000a54715500c63cd37d6a1a6e8fb9ccac887aa8c8b3fe5672a08335719c942aa2b2e5163cb8da7c25e887debbc9d357dc6a0f5484547f2fcc78b1f44cb54e19d274e6371b8325888bc01b1b951098f407346b3f9ac6393b6e6b50b6d419a41f05a8534a3d91df4c708fb0830cc240e93ee42012aaf6c5f1d600a2104630c618c9624f9c80eef3ece7b7a59b10e905903eac9ed66bb1619a53bff4afe72153d51597be499ecff8a8730f4c0d20e4694635069e6ffb1212f5009f9243a4bc065ea6da6752bbbe0e0b2a148c71cdc02aaa55618ea7ad7e5105be30b8c2c7d24b6ed436e55885d91c36b497d80b21397da740fb3f8dbc09c3387a1085331d9e860ad741fd5300131e74c42df6ebef1f5acce28c89b8c3b48e7f802887f4a3b53f6148038be98c56a72dd39995c3932209ea60dd198ccd2308bff4a77a2851fe8ff68df7ebe64f6512e14a8ed874229bd4c66a746f7b3be62a9d97574bc53773508216d1281e5234f46792c8ee200f38542264b480352c4b24f08ea60b2fa11dc21af1eff5058fc53a9d23bac98548597bbaf15797b9d8f5c413919988d6bc8d0c709d6f98d12a863b0c2a8dcd2ce2f4aef285b0561e37de6b8c338ffb6a8e505eb570a3418c1c5050ef6aaf2c68f77b07b97370f7a22783a93eeebcdb37278c40e8367bf20dcd13b1ad39bbc15dca1725147395d0a393c7efe9fef15961044761febc359d575cd1d80abdf554a0fee0e61483dc1a4a8de1b378dbd32bc386b13fd738688d5989f1e4dcdad701a4933f40aeba9d04299ea8f1b73c8b6e4486c0a3cc669d31acb06d49cf42e8fa2aaca0f67d9aa3ec650fc6689de7a48f4b8eeaf70e4539b9b11f68c448a9dd2423e2b37fec85b1cdb138b7438148a0fc8e4301a32ccceaae486d26675e4605d93b05ed28880ce4e4f36dfab80909ebe0e7c779ce6adda8525d164612c6d0cba43c27ca0e37e6fa4dbdb2c465e561e9e5c696afd4a0ebbdfe978503667f7629925e99c706380980f8730ce3c67976945922ef4769e58a44fb3a7f1459c6321867566180d1e335b3d4f2d3d966bb1090c3cdd4d3a7a4d9ca2ff58d9137c312aec8acbb3e029b691a9f60f30b8304e1a2f1f9641adf6997ebf85b7e3bdfd73e1e3c24568b05e6e6a66d4df1e287f3e6bc124cc04dc011d09d145c2a823cf6ff6ac942df939250f8d9a088f2c301a98b3e878313e30631a6cd07999e7852a877e4930db2a5cd9708172f023e322ff7aa5192fe336a5b9881a67e0d63102f99b33288056db2d8b03a358a26b2e18fb906f60cbdc476bda2c837d04f489d7ac5862058eddf97b10ef39258ed8af0167609b94097b9eeb313bbe468f2e627da740d4741c6512dfbdc84e5a31a795013082d94faff20e4483d9334e13909c5d54e3bbb1d26447e0b3eccc5499b98119289c8613ffca3f4053c69778b3cfbcc3d9ffc53741081d328bedf6dc83ee56461130f31fbf14acdfb43d796ce4ea86a015ab3647760f292958ce4c9ae9c6fc0f3120fece3bae378b51e7813484a893db74797e9327e3eeab1e7b7856e9cc6124460330299fd4484dd8b6b73073679a859461ec2f5ded61e9de9ed367c6c4e95d68390e78690851b8b39e19ed73798ee93a78cddab362059726228d9fb418f85c149c09306b73ef9f9b1cc8c1c9a5be9a96a21137a1bdc86ac8972666495f28ccc0adae17c874b79c26a9144573213bb44b8412ef1a0202ee653df65777f7522f30da660630786efb24a28d8e4a79dc0e4378276ecec16b0462aff7295c025d84d4c51f8b2c3ff5377b9657cf0160112d1586aae6c1ce8f3c8fcd599342b73dd282a43a1024fa94ecb630df5f8017d19f3c8edfecdd2ab5886db8403ca4a1df6fd2a5194810c2ea00cfed6fdeb080c8c7b2dff67c40755cb7959be8d1d7120ff519145c5da33b516c34ef216d657ebba30ff9a9e06f2eab06724ee26e06f5f0813eb105620e");

TEST(wallet_tests, set_note_addrs_in_cwallettx) {
    SelectParams(CBaseChainParams::TESTNET);

    CDataStream ss(testRawJS, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;
    CWalletTx wtx {NULL, tx};
    ASSERT_EQ(0, wtx.mapNoteAddrs.size());

    mapNoteAddrs_t noteAddrs;
    libzcash::SpendingKey sk = testSpendingKey.Get();
    noteAddrs[std::make_pair(0, 1)] = sk.address();

    wtx.SetNoteAddresses(noteAddrs);
    ASSERT_EQ(noteAddrs, wtx.mapNoteAddrs);
}

TEST(wallet_tests, set_invalid_note_addrs_in_cwallettx) {
    CWalletTx wtx;
    ASSERT_EQ(0, wtx.mapNoteAddrs.size());

    mapNoteAddrs_t noteAddrs;
    auto sk = libzcash::SpendingKey::random();
    noteAddrs[std::make_pair(0, 1)] = sk.address();

    wtx.SetNoteAddresses(noteAddrs);
    ASSERT_EQ(0, wtx.mapNoteAddrs.size());
}

TEST(wallet_tests, find_note_in_tx) {
    SelectParams(CBaseChainParams::TESTNET);
    CWallet wallet;

    libzcash::SpendingKey sk = testSpendingKey.Get();
    wallet.AddSpendingKey(sk);

    CDataStream ss(testRawJS, SER_NETWORK, PROTOCOL_VERSION);
    CTransaction tx;
    ss >> tx;

    auto noteMap = wallet.FindMyNotes(tx);
    ASSERT_EQ(1, noteMap.size());

    auto noteIndex = std::make_pair(0, 1);
    ASSERT_EQ(1, noteMap.count(noteIndex));
    ASSERT_EQ(sk.address(), noteMap[noteIndex]);
}
