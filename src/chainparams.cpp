// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "crypto/equihash.h"

#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

using namespace std;

#include "chainparamsseeds.h"

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.fCoinbaseMustBeProtected = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 4000;
        // TODO generate harder genesis block
        //consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        /** 
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0x9f;
        pchMessageStart[1] = 0xee;
        pchMessageStart[2] = 0x4e;
        pchMessageStart[3] = 0xd8;
        vAlertPubKey = ParseHex("04b7ecf0baa90495ceb4e4090f6b2fd37eec1e9c85fac68a487f3ce11589692e4a317479316ee814e066638e1db54e37a10689b70286e6315b1087b6615d179264");
        nDefaultPort = 8233;
        nMinerThreads = 0;
        nMaxTipAge = 24 * 60 * 60;
        nPruneAfterHeight = 100000;
        const size_t N = 200, K = 9;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;

        /**
         * Build the genesis block. Note that the output of its generation
         * transaction cannot be spent since it did not originally exist in the
         * database.
         *
         * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
         *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
         *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
         *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
         *   vMerkleTree: 4a5e1e
         */
        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1231006505;
        // TODO generate harder genesis block
        //genesis.nBits    = 0x1d00ffff;
        genesis.nBits    = 0x207fffff;
        genesis.nNonce   = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
        genesis.nSolution = ParseHex("00819595f9010330cfa2a7b7e278e11b50749c312e0420b87423521fad689c82d0ce5cd915909d124543131b8177cf085517b358c376125a4024e6c8dc33af17da9ef44311883ac6043618b8506cc227a19514ad0862110fb646b5f0d31015b7cfe7dba9df43f09953396daf5fffecc6498b25166dbf5e930ac27e7c5313092f64491386fa82c3e031d6e3699afd30288a8c992f062d1aaa0e2c2278d4b69426c46696826b1f629e070a2430b6f6f0a3f83e00f17bf47ecde27af17cfd075fe4ba5d0ed32175ae820b3667466201f150c1c308bb5c36e68f3743320022dcae65e899c258f172ba0cca710e3c48cd99d651e248c2c28cacc00f50f4730d1cb53de610bf5baeeef93819749dde6b477dcab43b8eced8fb9d8a27008cb514d3f12389585c354c3d20a49bec7ad6be91f17d623eade44d1e1965f1f2a059abddbf055755a0dfaee89ddefd41c2e1b43e78400d8a741c5cc7a6715915734c609d6060da35d1548f3a84b30e0450988cdaede6672a3de896ee9678eaec0dbd06d9f398af59f6783293576961510dc23def633bf7aea154b0c9a9c6cbc64b354a824deb86bb9bcd1418a2f492ddc026f509f739aafc1b92011fd2b7081aa229838f1b74ffb3c917d70156aca6125d17840927cfb9e5fd5f2ad399e2f8368978215f3b3d7c83002a887e0223a44b0ddae6576862f618be4d86df9a7a0e4d73d43c781a4fe2c11397d6b701091fb934eaf4253c561dc95b6a838b4d12eda02d2bee0071116ef540a30b712710d279d9d294b317b9524d7d574dc60a5c19cc405161df31581b977e97da2c75efb55eab10126e1fd76687a9e0faba727c2f95cfee07611781242b657737d1d262da9c20c3b77f3d0a01ad83369b092ebb7ea5dbcca51ed15fa6a9edbcf98363dadb4bd2414ea53007a41a0d71b7f8a1d54a41e337059d73a4012fc5b00cd3ef26d08cc120ad751b46f690f8924e393116ba3a9292658ebde47e34c3d10f6e8fdbcf1c1081fc59955ac167a99d358a38413c723882d7726642fe57f7dc57cafaca78e456dc6e8119329c769a4303bfbfa77912f846d37f97c4b349d08a136e362ca9169dc5006f5ea595c21d4794f6d7c51e1f1d5cbcfc06e5ac3da95ba295bc47cbc35866c1fb14c83d396a36fa12ba7a5b7bb38da6b3fb15d1f9c74f711fc84501827bfd358adb807c4153634c254921444a314d7f192c7fbeda77468fc15e95f31ad07399f8afbf73701c2211d601650a2d9b9cd4fb5f6ee556e2515c58a72d1b527ada0b90747e333829d6f3f32b01adffafe909b58df36605af99379511af6118cbc8f5c32f5f1c2f91253454125638d3ee3bd06aff79bb5bf35b90f00d589e08082123d74d66e3aff1ce06fdcd6f15ed9c20f49adbc4d8069d345ce30f31c670319dcd5c0055052b87bad1897b8873b2748122bb3a3d8cf5545342146c576401877346f8e36476ad4cfc9ecdbf367de211922e14be95d171f970c7f783d7aa6e8e8efe05eb1f7e73f070c8c802db0c9359966a35c90f9830e94210ab9bebd14f08dcabab219830dc206df20d7ab05d2b4b44456a8c5563d22268c0e87de7ca99d75d52f91666b5fe7d9a53a97193a70d51dc4bbec8c8def4bb484b2db586af2f87eed8a6547e50631f3b9f9d715d07d2bf4e54506a7d3d96415e81c597e6c498fd8ed91772ed82eb9f33a5fa77c2884573c3d95efa0ffccd1220bf602a5ecb67087032a23c36473d465235e8803633d2ec9b4fb9d4b6b376335ffffb2223ef5a7c5708ed01e05f93b916c5c2e0cd6cd35d514e86f517581a8240fa68608a190ca4a5af877b99e264af3e08752cb88a9d229acb9977f663b651f3f4e100b179f6622ce94a472d6f8c9d8f08a3867ff873d700491ba1b9");

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x69aca142ef33f5d4cdbce357e717dddbb849a8f62f4a9fad3e633c5d0e11974b"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // TODO: set up bootstrapping
        //vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be")); // Pieter Wuille
        //vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me")); // Matt Corallo
        //vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org")); // Luke Dashjr
        //vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com")); // Christian Decker
        //vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org")); // Jeff Garzik
        //vSeeds.push_back(CDNSSeedData("bitcoin.jonasschnelli.ch", "seed.bitcoin.jonasschnelli.ch")); // Jonas Schnelli

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();
        // guarantees the first two characters, when base58 encoded, are "zc"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {22,154};
        // guarantees the first two characters, when base58 encoded, are "SK"
        base58Prefixes[ZCSPENDING_KEY] = {171,54};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            ( 0, consensus.hashGenesisBlock),
            genesis.nTime, // * UNIX timestamp of last checkpoint block
            0,   // * total number of transactions between genesis and last checkpoint
                 //   (the tx=... number in the SetBestChain debug.log lines)
            0    // * estimated number of transactions per day after checkpoint
        };
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 400;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0x26;
        pchMessageStart[1] = 0xA7;
        pchMessageStart[2] = 0x24;
        pchMessageStart[3] = 0xB6;
        vAlertPubKey = ParseHex("044e7a1553392325c871c5ace5d6ad73501c66f4c185d6b0453cf45dec5a1322e705c672ac1a27ef7cdaf588c10effdf50ed5f95f85f2f54a5f6159fca394ed0c6");
        nDefaultPort = 18233;
        nMinerThreads = 0;
        nMaxTipAge = 0x7fffffff;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
        genesis.nSolution = ParseHex("001c96e65b62215b5f9411f798c206fc94ecb99b9a1fbbd973b5e7452fec7465b221ecb8da33f17f11bb0e81112e514782ba6b75512b75601888a53dffe0540f37038b35194f9d0e71589207dbf42652d4f6e06b02ec0e7c6f0b5624f85912ca96374c6e1de25ef4e118b1d2647124691bc62d83992bffae9f6c405f5baa33dec2d8065b667adcd0d48a8a37538d2b2e6e2cb074387d6f18b77091d879878e06647993ab395e09b6039c661297682c916793a24464171bc5fb7835470e4d9a8b3dc7a1ca2d2c41560a17edd989adc47bd5ab06ddcbf4d862e347fb1cd7e2bcfaafd2d7d156c08112b02c678d4c6dedf12df18d4848d049ab03df98b30ae434c3c7edc4b3bfd643e0b56a72250966cb33393999b27c6eebdfc57ceef3c7e274f0851ee2e9b561257e767a61b000e1f5f2f8f628dc5a7ae62bd8ba032dab8ad2704d7dd2b0dda43286c0b6cecee859612b01d0e609092b06c58fddb4ec80342681ab80bdc92e07d1f121761e2a0d60903324fce3f159d84faf67fa226c92e49d55528b374e02fa52d3c23fe4069ff9483894f33180f30cdbbc4f96c6363b9eceeef357fab1228d79455db7a91ffba4f338e2ce73e29b9a7634f6286191a5fda30e9db80d36294bf4d0b5de141c865b24b3c595c7a860b785ea95ab4d7d3eea920038b90f409a2fefa2e29ee5af3de6d2f7f4f6935e95be92070b3bb65c7d627d6f83bd72ba3b6a71c4e163d4733e2d3856f3aa9d4e192866940bea6fa41123e6524f7026ffb4dbdcae44dbc1b3a3af78b123f6bd2656722c2941ee83cd20031b06214476cecb80b7be5edf51f60fc7e88e47590e8558ab512fba3934d0a78e590c0030b0b4288f168996d89e154c9935894df3c177551d10b03a65131e4c91b40be57381fa355e55b67d4846580772d94ba30e21ba235b09ed69a4b2fa89dbb2d401b7db79420bf8bf2af3e2eba7734ad8d3d586e2781392b6d60c29c7478104526d605fcf7a7ce69bf1e81282c66d0e6a581fe9e8312ed0eb616e6cbeff6a99234d2e0e604d6b55942839539cd050f71cee9a77a9070040699a53ec8fd305a6d18e6f5c91d447f13e520de5af9bca43b640bf83639505e8ad962b5a9e901711fb212b8d85b0e68028e19b88ede4a88f449fac2a296c0f60f724d6e7b0588691c07dc9ee2f079f6d74033d32ebbaced319ed4941a8033a3ec4ad9ced218d110e199b15fa000fdc77e2d93bd59a1dced9eff8f1062c4c00d00222e79659e477dfd1d6d9914a7da433079124e2b31189e5614ec1d23c1df305f4bcf6056e0f6e8d06f42976a959ec646b28adfac57cd90f4f15434d55994073c603a5f55592bdfdc08b20b3dc13d02dc654f19479a77bdef3a499f6af563a73c4149ee64597ca9f6e25db55bf9524ec4df6dbd682463564df026ecbcbedb5ed33c339230b5dacdf0121c5f409e90cc5386d691d974ba8d8e50fc9d61f5f4f6dbfdbb7152e65bf10e2d7d5fff212a84a2b0fa5e44cbb032a210906e5d50ee449af0fa4cad575b177e5665f960a0f27256f70df30317f1257720beac8db9a621d1dee3908ca28a199a57fe48885b6eef15e46db23bd70a429e8540b9fbc0a11fd0145da4dbf3292aa5158a045433d272b215cb262ed5b9650b47a3ad5d0ba7ebc1b03002755d24386b18ef901989aab6da611d517abf2030a134b17d4865ae59a22d86345e3fd1c02dcfb1d4a927c121195617f3cd2e662aeb42d1e9ae33a9ca84b376aeaad5cc5c9ffb7e4bb8770dba5e120909af808bbdd8844214c0d0e887affc06a827eef0559352120f4aae43e9b69cbf66dc2adf5be5d1cae51097a9d0ab0fd26795d2273a44a6875d6d94efaae773a93a347290ad59ee39ef13f5d26da3e548c8dfb39770956");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x77ac3acbaea6aff515d5090ffbc8611e9774e658d38f1e0a173e29b63af6d1f8"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // TODO: set up bootstrapping
        //vSeeds.push_back(CDNSSeedData("alexykot.me", "testnet-seed.alexykot.me"));
        //vSeeds.push_back(CDNSSeedData("bitcoin.petertodd.org", "testnet-seed.bitcoin.petertodd.org"));
        //vSeeds.push_back(CDNSSeedData("bluematt.me", "testnet-seed.bluematt.me"));
        //vSeeds.push_back(CDNSSeedData("bitcoin.schildbach.de", "testnet-seed.bitcoin.schildbach.de"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {20,81};
        base58Prefixes[ZCSPENDING_KEY] = {177,235};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            ( 0, consensus.hashGenesisBlock),
            genesis.nTime,
            0,
            0
        };

    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.fCoinbaseMustBeProtected = false;
        consensus.nSubsidySlowStartInterval = 0;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowMaxAdjustDown = 0; // Turn off adjustment down
        consensus.nPowMaxAdjustUp = 0; // Turn off adjustment up
        pchMessageStart[0] = 0xaa;
        pchMessageStart[1] = 0xe8;
        pchMessageStart[2] = 0x3f;
        pchMessageStart[3] = 0x5f;
        nMinerThreads = 1;
        nMaxTipAge = 24 * 60 * 60;
        const size_t N = 48, K = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000002");
        genesis.nSolution = ParseHex("08f58d7eb23488e1e60f2c8d0b417651419c0ddc37bf92467cff1911135d3be14be893a0");
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(consensus.hashGenesisBlock == uint256S("0x4ed22ec3bf7a7bfc9138132005b28773ddd935f929f9ee26868b088e1605381b"));
        nPruneAfterHeight = 1000;

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (Checkpoints::CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")),
            0,
            0,
            0
        };
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
