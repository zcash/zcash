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

#include "base58.h"

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

const arith_uint256 maxUint = UintToArith256(uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        strCurrencyUnits = "ZEC";
        consensus.fCoinbaseMustBeProtected = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 4000;
        consensus.powLimit = uint256S("07ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        /**
         * The message start string should be awesome!
         */
        pchMessageStart[0] = 0xde;
        pchMessageStart[1] = 0x9e;
        pchMessageStart[2] = 0xeb;
        pchMessageStart[3] = 0xca;
        nDefaultPort = 9233;
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
         * database (and is in any case of zero value).
         *
         */
        const char* pszTimestamp = "DeepWebCash 2016-10-29";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 520617983 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 4;
        genesis.nTime    = 1477670400;
        genesis.nBits    = 0x2007ffff;
        genesis.nNonce   = uint256S("0x0000000000000000000000000000000000000000000000000000000000000023");
        genesis.nSolution = ParseHex("00172bbde1da5bed5e86c004404384052b192c0b120a108d26e984a226d3b401133c0ae22a4da999989b0541147e6697fcad63c8f1538bb9f90211ae1b1d805175e362b0a54ba199be16215049bf259e975d9a56016e1f11869b6d70f71d44b0ccb215f1e5039811910886e1316bdb3eede54a36eb2de1c21a5b4e587c07062aaf831941983a94a5720127ef82c56823b8e9e318fb2ec2b7ea135bb94e85a689501c75c72930e072004417f3d4d72994c7aa518d06ef3474d8227a573d1e27077b3214f0cd0aec958555d1c71efc32183efb0639784380d094335bb1819ef4731fc8cfbe7e449208d0f19abf9b11976ae04aa9af5ca5a302253d6679041b5150d16f11ffe420f1b47891e3797ce67a689415fbf41fda8bc3d48c1c4547c4da79f1feef14d9092bf581e95de0124feeff4657f4e8836e1b0712b6f636f9de1f09e09bd1052138900872cdb22f401a541a03272fa2366087e967b112327f46eef0b4a29e42130fc629fd8f485b0d872b42282796a10124be319cdb0cbf11e911941c18d5c030fefd8c4b91f3e5b0afd822917cadc5730601b043a26ecf2417e1340571b78c12e4735a57cc142272a725efbccf86d351d6def05647092f0ec81300374fed549ffb251871cf0538364c1e033314bb8a4466fe53e36a09fc6292381713cb5d308f26d1fad45d6cf2dc65438cfa2eddd3ca3602e904cafc9fcf46619df105b262a2f04193387efe4a55117d07b927a1b35dcc248477f36af7217b30d518c905e699714945a0a4f8c3a4681bd02f9b9481dd046b156237902058965b1fba12e82b44574e69185a15bd0603af3d7741b211322194ab77d654298bb67e818f4dbbabafef194fb34274a950abcb87629e0c3ce1d80bf5c62173d7d26efe9802b78ee594327dd63f491d19bba9792018fa999f8fa23b5d249f6cd8510cc5830122f2c7ae35bd7ffe46c319dec2c29d8df1cd480119b81b1c4fe4df33bd5ff1e4bf55d510f818969d5f0ba3efa7cd604f47eb3010efdf3c1dc4fb173bc3513e9144e11ee906578a920429c1f2b3119008925526071a838d54d8ed1bc47755e9c4372565be70588f791be61e2fb68abce7f30f626c68ff4588c9766b814c14a6de43fbce3dbea0a0f8757af73e0a505412adff39049b92d51129214035b6ea5274deb6a7ee9ffd2604108efcce0928aea50e32638127cf7d2e16908dfe1f2f9b135f1dcb87a79c92316a6573a8961cab89bf069aa55da4445bd7cb88331eda6536cdbd5edcb0d329c24f34cf99d39fe525a4d4c3fb710a9a4cba10711316d479055cbaf8f8a455ed8f62d8fdd7c1bf2f361558f6f4f08a06da7c0b02074e39ba04e7460fa949145a5619a485c529dff1543d39bb60b1304573c6e71d5d99eb6c61f6b5dd474321ec5820f161108ea1ff03558b59021f8d9fdf1d3163cddee6397494f3783b17d58a32507108679715d41159b500b1fb7b1cc3d50cc5874e86ced7919f49a63e7257c3320651f628f82005df9c5f13eaa7bf1b232efae39cb297bd35c3840eb4b52f86cb43e75915f96749798087b312dead5c3388bddbd8937281b81699d4d5e9104690e53817821d6bd6f84588f9a852b984c0b0cba9b2d294daf6f63b14f2826b9823c5eac0542e887c5d779e401f22e504a0ef4967a59c25d92565515efd8ca33f01da23d045ea1f7fd6a4bfb98417659323727fbabc14584de5100adb7c70c958d0d15a9165cc240901993bba775323c916fcf64c1300c6f3c81847568cfa76f8fca69e0df25b4cdb05f0a7376eb2e99fe32aa234b3d654792d5c89f3330f8476cbee164026c11d0eba34d9201a1de4e25121883df4b0c32350db68c0f600285143db21280d4e5a51b2919600360ee84fb521d4b35448b9");

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0134a5b23c738e8b1e73cffb94b9eb2f6e5dc8c2f7a864f200809f0f6d1603af"));
        assert(genesis.hashMerkleRoot == uint256S("0xc5c5256d5c1e5f42daabe53beebe5d74aaac07c50b85c8dab549999fc16ca3d7"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("dw.cash", "dnsseed.dw.cash")); // DeepWebCash
        vSeeds.push_back(CDNSSeedData("deepwebcash.com", "dnsseed.deepwebcash.com`")); // 
        vSeeds.push_back(CDNSSeedData("darkwebcash.com", "dnsseed.darkwebcash.com")); // 

        // guarantees the first 2 characters, when base58 encoded, are "dc"
        base58Prefixes[PUBKEY_ADDRESS]     = {0x14,0x9D};
        // guarantees the first 2 characters, when base58 encoded, are "dw"
        base58Prefixes[SCRIPT_ADDRESS]     = {0x14,0xCC};
        // the first character, when base58 encoded, is "5" or "K" or "L" (as in Bitcoin)
        base58Prefixes[SECRET_KEY]         = {0x80};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x88,0xB2,0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x88,0xAD,0xE4};
        // guarantees the first 2 characters, when base58 encoded, are "dp"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {0x0E,0x72};
        // guarantees the first 2 characters, when base58 encoded, are "DK"
        base58Prefixes[ZCSPENDING_KEY]     = {0x53,0x3A};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            (0, consensus.hashGenesisBlock)
            (2500, uint256S("0x00000006dc968f600be11a86cbfbf7feb61c7577f45caced2e82b6d261d19744")),
            1477973071, // * UNIX timestamp of last checkpoint block
            22063, // * total number of transactions between genesis and last checkpoint
                   //   (the tx=... number in the SetBestChain debug.log lines)
            5083   // * estimated number of transactions per day after checkpoint
        };

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = {
            
        };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight());
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
        strCurrencyUnits = "TAZ";
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 400;
        consensus.powLimit = uint256S("07ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0x6e;
        pchMessageStart[1] = 0x4f;
        pchMessageStart[2] = 0x9e;
        pchMessageStart[3] = 0x6a;
        nDefaultPort = 19233;
        nMinerThreads = 0;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1475337300;
        genesis.nBits = 0x2007ffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000012");
        genesis.nSolution = ParseHex("000152a55ddbb173295635b50f5f0f0b867f5ffafa1615f59a3d4fef3cff26a88deafe6bae5eb1f696f408f443dca48ebb9ddbd0c54d5afed091da08af7d391100d7b8b3ca3560b52ac1d336fa74fa43863e021b01c7f984a422a4dbb05973872f51dd697a656f521a36fc17790219d9612f17fbb825e6e8f355837addb0136480b0a80b2a71593871f3161395128d3ddd188d19dc4461f35a5914fac3c99dc172631bc83bbf38c700a8395958c0767578713744084ca85aee1e1df9db1200fb37da0fb4bd8c5d016a867b3ca8a68f069eaa1413546c24e1a721de5a32a2dab30dc546e130e2e22c5262e06790eaffd6309635cc418a5a4b309dfa42060827f67f0d35f2c4704138c234bc66cbc13ea2a207143b1929521fc2bc2ec37cad31aef4f083e89af90c6263a43fea3869bd9d233887c43add09fe32f1643962da878015573f53f9167ce0ecfb5e1eafdf4b0102721084bfc3d990ec9dd1f2b0c229afa7eafdbd1403bcc9bf99121e1da3bbd9adb56272f29cb51cdc41305585be5917f52f9bb7c311e8ec8e5511dcc8f8ec3586047ab29c126dd01bacb84279385f69d3dc96f50cacb8e7e814d3619817c624bd51dffa021d5609a34072d2411f640e6b20769779f3734f360676143b430ee59d51381dcf5f044344ea91ab2da588cbdea5c359b1cb9225596c274b61270ec97efd0248d19f625a02d4a23e1ee66003a24601a216ae25f0759b44749739415e52ed62b2c55c6e1a2a40dabce36713df4ceb19010b22b2ad6f0dda521286192a26e654f5d7836d32d2b39073db74af90c50841ed7c428e3f565a0ca70fc802294fd8d99f7f7a71c0d9cb1fc19424d7a1113e42cddf84acd7896ef59582e1ba154a68b2f6f11c1af7acf19d94fd6b235e81ce14e225cea1101efdb148773fbe4d198957406285499cd5fd0e31d1fcbce20279c11e88604ee3d0c03267553117b270827fa09c112787853b0721cdc85f643b70e70e7d33477ab7f90aab788e48a179d186617635d1dda416bcd6bba56952b46ab1c3de72af21cb298202d8ca2e887b980730038f08f999d0b9d2c1e9211b7ac782296b63ff30d321995cdff0b65857f627745f64c99635a24badc5300905e3cc580baaf749abd0f0b6c2786a5e2459a02d2afe8e81b92d53c99c5692e0774143b4bcc6a844e302b7b6f8614deaf7a79440f403bac1029c77dd22f60a7ef3798dc78498d170146161f25699ef6e5246ee0c76deb796edef45f4432190a0e44fe6a783bb5d7b103b35b92804cc636e35b8cfcf58b22e4b06ffd4890da23b9034951a5727dea125e56948d07fe031ea0b12c15ada65c533f7b56fe4276f3142f1f3431c88f90ea5945f422b7c9dfb0b0365722a2e6178ce5b9be74af89bf98d20e6a1a100d58ab8c1ad3be6b65fa07d03ff125be59b2a88e04622ecb9c523b6df045a548914b1769c2bca5bf1f0fe122c049d3a66c08afba26a066f71fab3460659e3a251ba75142f6a6aa678baa924feff0e50a3dd83ada2c276df34837cb40d4d74260ddf01e432ba9425f50ab320b14f42952f91ec5ac934478aefd2f4c2cdc26654c85ccbf95971433d4d5114e90c8f1dacf9697c2d02c539b67798cf77b364fb3c53b77e06fde55bf21069cd05fa8e1740729c688304201b69fa60dce3ccf6229a5cc3f42da4735b14ec1133c1c27522e917e612f1f84d960ae9806e3967de283b0ba9361bb6f38b5ba7ee3ae9800218b3fd423650ba23323f1c1e616330dba85defeecb7c38bc7dc805b2f65b8f5c9f85c1e7fa06a2da00fbb3c25f9fdc5aa9e3c25d2f5e07b68927f7f04ef46665651b40980c6ededea4c92ca2894511f3f5ff443b02b6f97d691e75671d0e49b5aef87d9588c8649db9dccd940098");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x079121c01abf32f83adc8adc1ac7b54f8cb6de86775724a7e3738ed7da2298d2"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("dw.cash", "dnsseed.testnet.dw.cash")); // DeepWebCash

        // guarantees the first 2 characters, when base58 encoded, are "td"
        base58Prefixes[PUBKEY_ADDRESS]     = {0x1D,0x11};
        // guarantees the first 2 characters, when base58 encoded, are "tp"
        base58Prefixes[SCRIPT_ADDRESS]     = {0x1D,0x2D};
        // the first character, when base58 encoded, is "9" or "c" (as in Bitcoin)
        base58Prefixes[SECRET_KEY]         = {0xEF};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x35,0x87,0xCF};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x35,0x83,0x94};
        // guarantees the first 2 characters, when base58 encoded, are "dt"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {0x0E,0x79};
        // guarantees the first 2 characters, when base58 encoded, are "DT"
        base58Prefixes[ZCSPENDING_KEY]     = {0x54,0x29};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            ( 0, consensus.hashGenesisBlock),
            genesis.nTime,
            0,
            0
        };

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = {
            
        };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight());
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
        strCurrencyUnits = "REG";
        consensus.fCoinbaseMustBeProtected = false;
        consensus.nSubsidySlowStartInterval = 0;
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 0; // Turn off adjustment down
        consensus.nPowMaxAdjustUp = 0; // Turn off adjustment up
        pchMessageStart[0] = 0x99;
        pchMessageStart[1] = 0xee;
        pchMessageStart[2] = 0xca;
        pchMessageStart[3] = 0xde;
        nMinerThreads = 1;
        nMaxTipAge = 24 * 60 * 60;
        const size_t N = 48, K = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;
        genesis.nTime = 1474098900;
        genesis.nBits = 0x200f0f0f;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000023");
        genesis.nSolution = ParseHex("0039c218d065fda9fd11248b50b2675e69d402c9271e2143e74bcb1e59db1ff3fd2561b9");
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 19444;
        assert(consensus.hashGenesisBlock == uint256S("0x04d86f96bd692b6a974274d429915aabe7700e615bd4fc7c431e3f6a07130de9"));
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

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = {  };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight());
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

    // Some python qa rpc tests need to enforce the coinbase consensus rule
    if (network == CBaseChainParams::REGTEST && mapArgs.count("-regtestprotectcoinbase")) {
        regTestParams.SetRegTestCoinbaseMustBeProtected();
    }
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}


// Block height must be >0 and <=last founders reward block height
// Index variable i ranges from 0 - (vFoundersRewardAddress.size()-1)
std::string CChainParams::GetFoundersRewardAddressAtHeight(int nHeight) const {
    int maxHeight = consensus.GetLastFoundersRewardBlockHeight();
    assert(nHeight > 0 && nHeight <= maxHeight);

    size_t addressChangeInterval = (maxHeight + vFoundersRewardAddress.size()) / vFoundersRewardAddress.size();
    size_t i = nHeight / addressChangeInterval;
    return vFoundersRewardAddress[i];
}

// Block height must be >0 and <=last founders reward block height
// The founders reward address is expected to be a multisig (P2SH) address
CScript CChainParams::GetFoundersRewardScriptAtHeight(int nHeight) const {
    assert(nHeight > 0 && nHeight <= consensus.GetLastFoundersRewardBlockHeight());

    CBitcoinAddress address(GetFoundersRewardAddressAtHeight(nHeight).c_str());
    assert(address.IsValid());
    assert(address.IsScript());
    CScriptID scriptID = get<CScriptID>(address.Get()); // Get() returns a boost variant
    CScript script = CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    return script;
}

std::string CChainParams::GetFoundersRewardAddressAtIndex(int i) const {
    assert(i >= 0 && i < vFoundersRewardAddress.size());
    return vFoundersRewardAddress[i];
}
