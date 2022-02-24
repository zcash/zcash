#include <gtest/gtest.h>
#include <iostream>

#include "chainparams.h"
#include "clientversion.h"
#include "version.h"
#include "utilstrencodings.h"

#include "chain.h"
#include "main.h"

#include "komodo_utils.h"
#include "komodo_extern_globals.h"

// ./komodo-test --gtest_filter=TestHarakaRemoval.*
namespace TestHarakaRemoval {

    BlockMap mapBlockIndex;
    CChain chainActive;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);

    TEST(TestHarakaRemoval, test_block_ser)
    {
        // this test should cover genesis block (de)serialization before
        // and after applying haraka removal PR
        SelectParams(CBaseChainParams::MAIN);
        const Consensus::Params& params = Params().GetConsensus();
        CBlock &block = const_cast<CBlock&>(Params().GenesisBlock());
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        EXPECT_EQ(nBlockSize, 1692);

        ss.clear();
        ss << block;
        static constexpr const char* stream_genesis_block_hex =
                "01000000"                                                          // nVersion
                "0000000000000000000000000000000000000000000000000000000000000000"  // hashPrevBlock
                "3ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a"  // hashMerkleRoot
                "0000000000000000000000000000000000000000000000000000000000000000"  // hashFinalSaplingRoot
                "29ab5f49"                                                          // nTime
                "0f0f0f20"                                                          // nBits
                "0b00000000000000000000000000000000000000000000000000000000000000"  // nNonce
                "fd4005"                                                            // nSolution Size (1344)
                "000d5ba7cda5d473947263bf194285317179d2b0d307119c2e7cc4bd8ac456f0774bd52b0cd9249be9d40718b6397a4c7bbd8f2b3272fed2823cd2af4bd1632200ba4bf796727d6347b225f670f292343274cc35099466f5fb5f0cd1c105121b28213d15db2ed7bdba490b4cedc69742a57b7c25af24485e523aadbb77a0144fc76f79ef73bd8530d42b9f3b9bed1c135ad1fe152923fafe98f95f76f1615e64c4abb1137f4c31b218ba2782bc15534788dda2cc08a0ee2987c8b27ff41bd4e31cd5fb5643dfe862c9a02ca9f90c8c51a6671d681d04ad47e4b53b1518d4befafefe8cadfb912f3d03051b1efbf1dfe37b56e93a741d8dfd80d576ca250bee55fab1311fc7b3255977558cdda6f7d6f875306e43a14413facdaed2f46093e0ef1e8f8a963e1632dcbeebd8e49fd16b57d49b08f9762de89157c65233f60c8e38a1f503a48c555f8ec45dedecd574a37601323c27be597b956343107f8bd80f3a925afaf30811df83c402116bb9c1e5231c70fff899a7c82f73c902ba54da53cc459b7bf1113db65cc8f6914d3618560ea69abd13658fa7b6af92d374d6eca9529f8bd565166e4fcbf2a8dfb3c9b69539d4d2ee2e9321b85b331925df195915f2757637c2805e1d4131e1ad9ef9bc1bb1c732d8dba4738716d351ab30c996c8657bab39567ee3b29c6d054b711495c0d52e1cd5d8e55b4f0f0325b97369280755b46a02afd54be4ddd9f77c22272b8bbb17ff5118fedbae2564524e797bd28b5f74f7079d532ccc059807989f94d267f47e724b3f1ecfe00ec9e6541c961080d8891251b84b4480bc292f6a180bea089fef5bbda56e1e41390d7c0e85ba0ef530f7177413481a226465a36ef6afe1e2bca69d2078712b3912bba1a99b1fbff0d355d6ffe726d2bb6fbc103c4ac5756e5bee6e47e17424ebcbf1b63d8cb90ce2e40198b4f4198689daea254307e52a25562f4c1455340f0ffeb10f9d8e914775e37d0edca019fb1b9c6ef81255ed86bc51c5391e0591480f66e2d88c5f4fd7277697968656a9b113ab97f874fdd5f2465e5559533e01ba13ef4a8f7a21d02c30c8ded68e8c54603ab9c8084ef6d9eb4e92c75b078539e2ae786ebab6dab73a09e0aa9ac575bcefb29e930ae656e58bcb513f7e3c17e079dce4f05b5dbc18c2a872b22509740ebe6a3903e00ad1abc55076441862643f93606e3dc35e8d9f2caef3ee6be14d513b2e062b21d0061de3bd56881713a1a5c17f5ace05e1ec09da53f99442df175a49bd154aa96e4949decd52fed79ccf7ccbce32941419c314e374e4a396ac553e17b5340336a1a25c22f9e42a243ba5404450b650acfc826a6e432971ace776e15719515e1634ceb9a4a35061b668c74998d3dfb5827f6238ec015377e6f9c94f38108768cf6e5c8b132e0303fb5a200368f845ad9d46343035a6ff94031df8d8309415bb3f6cd5ede9c135fdabcc030599858d803c0f85be7661c88984d88faa3d26fb0e9aac0056a53f1b5d0baed713c853c4a2726869a0a124a8a5bbc0fc0ef80c8ae4cb53636aa02503b86a1eb9836fcc259823e2692d921d88e1ffc1e6cb2bde43939ceb3f32a611686f539f8f7c9f0bf00381f743607d40960f06d347d1cd8ac8a51969c25e37150efdf7aa4c2037a2fd0516fb444525ab157a0ed0a7412b2fa69b217fe397263153782c0f64351fbdf2678fa0dc8569912dcd8e3ccad38f34f23bbbce14c6a26ac24911b308b82c7e43062d180baeac4ba7153858365c72c63dcf5f6a5b08070b730adb017aeae925b7d0439979e2679f45ed2f25a7edcfd2fb77a8794630285ccb0a071f5cce410b46dbf9750b0354aae8b65574501cc69efb5b6a43444074fee116641bb29da56c2b4a7f456991fc92b2"
                "01"                                                                // txCount
                "01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff4d04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73ffffffff0100f2052a01000000434104678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5fac00000000";
        EXPECT_EQ(HexStr(ss), stream_genesis_block_hex);

        auto blockHash = block.GetHash();
        CBlockIndex fakeIndex {block};
        mapBlockIndex.insert(std::make_pair(blockHash, &fakeIndex));
        chainActive.SetTip(&fakeIndex);
        EXPECT_TRUE(chainActive.Contains(&fakeIndex));
        EXPECT_EQ(0, chainActive.Height());

        CDiskBlockIndex diskindex {&fakeIndex};
        ss.clear();
        ss << diskindex;
        // VARINT(PROTOCOL_VERSION) - 89af1a
        static constexpr const char* stream_genesis_block_diskindex_hex = "89af1a00000000000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000000000000000000003ba3edfd7a7b12b27ac72c3e67768f617fc81bc3888a51323a9fb8aa4b1e5e4a000000000000000000000000000000000000000000000000000000000000000029ab5f490f0f0f200b00000000000000000000000000000000000000000000000000000000000000fd4005000d5ba7cda5d473947263bf194285317179d2b0d307119c2e7cc4bd8ac456f0774bd52b0cd9249be9d40718b6397a4c7bbd8f2b3272fed2823cd2af4bd1632200ba4bf796727d6347b225f670f292343274cc35099466f5fb5f0cd1c105121b28213d15db2ed7bdba490b4cedc69742a57b7c25af24485e523aadbb77a0144fc76f79ef73bd8530d42b9f3b9bed1c135ad1fe152923fafe98f95f76f1615e64c4abb1137f4c31b218ba2782bc15534788dda2cc08a0ee2987c8b27ff41bd4e31cd5fb5643dfe862c9a02ca9f90c8c51a6671d681d04ad47e4b53b1518d4befafefe8cadfb912f3d03051b1efbf1dfe37b56e93a741d8dfd80d576ca250bee55fab1311fc7b3255977558cdda6f7d6f875306e43a14413facdaed2f46093e0ef1e8f8a963e1632dcbeebd8e49fd16b57d49b08f9762de89157c65233f60c8e38a1f503a48c555f8ec45dedecd574a37601323c27be597b956343107f8bd80f3a925afaf30811df83c402116bb9c1e5231c70fff899a7c82f73c902ba54da53cc459b7bf1113db65cc8f6914d3618560ea69abd13658fa7b6af92d374d6eca9529f8bd565166e4fcbf2a8dfb3c9b69539d4d2ee2e9321b85b331925df195915f2757637c2805e1d4131e1ad9ef9bc1bb1c732d8dba4738716d351ab30c996c8657bab39567ee3b29c6d054b711495c0d52e1cd5d8e55b4f0f0325b97369280755b46a02afd54be4ddd9f77c22272b8bbb17ff5118fedbae2564524e797bd28b5f74f7079d532ccc059807989f94d267f47e724b3f1ecfe00ec9e6541c961080d8891251b84b4480bc292f6a180bea089fef5bbda56e1e41390d7c0e85ba0ef530f7177413481a226465a36ef6afe1e2bca69d2078712b3912bba1a99b1fbff0d355d6ffe726d2bb6fbc103c4ac5756e5bee6e47e17424ebcbf1b63d8cb90ce2e40198b4f4198689daea254307e52a25562f4c1455340f0ffeb10f9d8e914775e37d0edca019fb1b9c6ef81255ed86bc51c5391e0591480f66e2d88c5f4fd7277697968656a9b113ab97f874fdd5f2465e5559533e01ba13ef4a8f7a21d02c30c8ded68e8c54603ab9c8084ef6d9eb4e92c75b078539e2ae786ebab6dab73a09e0aa9ac575bcefb29e930ae656e58bcb513f7e3c17e079dce4f05b5dbc18c2a872b22509740ebe6a3903e00ad1abc55076441862643f93606e3dc35e8d9f2caef3ee6be14d513b2e062b21d0061de3bd56881713a1a5c17f5ace05e1ec09da53f99442df175a49bd154aa96e4949decd52fed79ccf7ccbce32941419c314e374e4a396ac553e17b5340336a1a25c22f9e42a243ba5404450b650acfc826a6e432971ace776e15719515e1634ceb9a4a35061b668c74998d3dfb5827f6238ec015377e6f9c94f38108768cf6e5c8b132e0303fb5a200368f845ad9d46343035a6ff94031df8d8309415bb3f6cd5ede9c135fdabcc030599858d803c0f85be7661c88984d88faa3d26fb0e9aac0056a53f1b5d0baed713c853c4a2726869a0a124a8a5bbc0fc0ef80c8ae4cb53636aa02503b86a1eb9836fcc259823e2692d921d88e1ffc1e6cb2bde43939ceb3f32a611686f539f8f7c9f0bf00381f743607d40960f06d347d1cd8ac8a51969c25e37150efdf7aa4c2037a2fd0516fb444525ab157a0ed0a7412b2fa69b217fe397263153782c0f64351fbdf2678fa0dc8569912dcd8e3ccad38f34f23bbbce14c6a26ac24911b308b82c7e43062d180baeac4ba7153858365c72c63dcf5f6a5b08070b730adb017aeae925b7d0439979e2679f45ed2f25a7edcfd2fb77a8794630285ccb0a071f5cce410b46dbf9750b0354aae8b65574501cc69efb5b6a43444074fee116641bb29da56c2b4a7f456991fc92b2";
        EXPECT_EQ(HexStr(ss), stream_genesis_block_diskindex_hex);
    }

    //#define PRG_VALUES_BEFORE_PR 1
    TEST(TestHarakaRemoval, test_block_prg) {

        EXPECT_EQ(ASSETCHAINS_SYMBOL[0], 0);

        // ASSETCHAINS_TIMEUNLOCKFROM = GetArg("-ac_timeunlockfrom", 0) -> 129600
        ASSETCHAINS_TIMEUNLOCKFROM = 129600;
        // ASSETCHAINS_TIMEUNLOCKTO = ASSETCHAINS_TIMEUNLOCKTO = GetArg("-ac_timeunlockto", 0) -> 1180800
        ASSETCHAINS_TIMEUNLOCKTO = 1180800;

        uint32_t blockHeights[] = {0, 1, 2, 777, 12799, 12800, 12801, 77777};

        uint64_t prgs_before[] = {0x6aa022c76ea23202ULL, 0xa4343592ba79a12bULL, 0x8492b17946e3552eULL, 0x621f743cd167c10bULL, 0x7ea1b1eacc9c250aULL, 0xb92db22ac0811ea7ULL, 0x319262efefeb69ddULL, 0x5522d49c081b0043ULL};
        int64_t unlocktimes_before[] = {1096258, 270251, 355694, 691211, 486730, 731303, 1017117, 212419};

        uint64_t prgs_after[] = {0x1b42e023859afad2ULL, 0xc6168584c5efc09cULL, 0x7d593d18594511c0ULL, 0xad7eac80b7188a0cULL, 0x59003b61d7bf2c21ULL, 0x73c53cc4b45e9b90ULL, 0x4ccd186d27aa79a5ULL, 0xcd983da802179b26ULL};
        int64_t unlocktimes_after[] = {433234, 339804, 288576, 505676, 1172513, 976336, 359589, 1098534};

        #ifdef PRG_VALUES_BEFORE_PR
        /*
            Behavior before:

            - VRSC and height < 12800      - use sha256
            - VRSC and height >= 12800     - use verushash
            - not VRSC and height < 12800  - use verushash
            - not VRSC and height >= 12800 - use verushash
        */
        CVerusHash::init();
        for (size_t i = 0; i < sizeof(blockHeights)/sizeof(blockHeights[0]); i++) {

            EXPECT_EQ(komodo_block_prg(blockHeights[i]), prgs_before[i]);
            EXPECT_EQ(komodo_block_unlocktime(blockHeights[i]), unlocktimes_before[i]);

        }
        #else
        /*
            Behavior after:
            - use sha256 always

        */
        for (size_t i = 0; i < sizeof(blockHeights)/sizeof(blockHeights[0]); i++) {

            EXPECT_EQ(komodo_block_prg(blockHeights[i]), prgs_after[i]);
            EXPECT_EQ(komodo_block_unlocktime(blockHeights[i]), unlocktimes_after[i]);

        }
        #endif

    }
}