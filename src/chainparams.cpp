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
        consensus.fCoinbaseMustBeProtected = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 4000;
        consensus.powLimit = uint256S("03ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0x1b;
        pchMessageStart[2] = 0xac;
        pchMessageStart[3] = 0xab;
        vAlertPubKey = ParseHex("04b7ecf0baa90495ceb4e4090f6b2fd37eec1e9c85fac68a487f3ce11589692e4a317479316ee814e066638e1db54e37a10689b70286e6315b1087b6615d179264");
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
         */
        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 4;
        genesis.nTime    = 1231006505;
        genesis.nBits    = 0x2003ffff;
        genesis.nNonce   = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
        genesis.nSolution = ParseHex("0025991209a4a7952e06c65febfd76424b11df202714b3e2083c8598a9083f098459638b2b0d269efcbf0d72d9fdccc92045835652ac372d51d0e23dea6ea31a7625d3c686a5f549aae270912751397f10bfcc89066e50df029f2fe9c72f41dfafc41beca6e9a9c21f2f903f234460610f1c01c54802b8f4edb5611263ae57a50307db731787a281d65046ea7619bc86b515e361e0d4d6e427970b72880952d9d2ab12783498cd5f00b895ab47cecdad71afb53db474875a6c409a863140b73d39d25bb034fa1ce7d4084cba721953de7de0141a1342cbcc28933d52d22624c725ed4a53b0920e29b93a9314e317abe1e1da0af153d8cf05579c2226021a21012872a903d9a5a2e15fade995d2bcdda25c1795cdfc021cf107b3aaf3652b5073de526356de1d09781c6027ec77c9a5f249b98dfd23fb038178aaf229558a30e94e32d9162d073e12ed988e1f1718bb7d00ce6e6b1b2ec8d97719d52e655781cd8279b9644109252b20210f0037696ed1b878cbc8f6b4db95dd1d01c6c53ec18fb4d38526c45a8bd163da81a6df360534b62267ac35a053c7abb5b40ee5a12e2fad1b61940e7269dcf95b12d7eabda16e16c1d5c5e5c015e19723f189a4e5a52161e4c842f4d869028143474fbb5110ee1cf0acf3ad1fecd8e2d58768633b55cb1ab8c518e5a4954ac66fdaffcd93d1f6ab0af4fc885ef15908b1c65a80d31475b868b57c8239676ea288bceae4190781906ecb5dd0fdc3959f0fbe64ea5f3f1f8c030ceafd4feb4c232fd3d4a19711d359051c62ebdb2b2ebc361a34323a25edbe9705f46571c60cfdf327c618a5529a6aea5185578ce1d62543a52141de8e52b819da2ca3dbc89d62db077200df2ceb3160664ff18e1b1cdbbb9cd1e368a3bed50db432202eb204dd186920bfce44c74eaaec7afe4284eab4ce78ef5b78cb9a009092f24ec999f3d7ae71e4dfe559323e5f76d99d3ebf4a663395b39f9657bd831afdab3f9f109ff2f003fe44734054a3c7915dd4a7de4dac7dcfc0f003532a913d2925e0edfd32b043284f66d93911ddd7e591031771fb4bc5d7a058fc519ee7a1a8f87d3a9fea42181da2335e37ddbde6c235425971f81a00743e9c210ffcff2875e8a191d8d2d381efabf801d7887dca33189bce10fb919b214f94a3047eb503d503a61fc76d086d5f1c1d88a663ecbae576f66d6ba1867a3ae1bd28f37c3744e5e72fe022a8eb4349b63edbbddae9291665470af215f63d506b12354a5ff0a332543a8bdf4d3aff12555aa237957939c2be676c4b11233948851823dda4d2d7c421af7241ae8dabf5ce25befb11e62c96c5c13e512bb91cae854e70b3c8fdfc727f117444bafeb9b4dbf637a297b63227cafa7e611c3bd3be507d6e756d6da7116d5eb761f76e0ec62f6a32c8d30176ac03960cd7790d6b218e89a215b86440c88dd40870b853040d8d2510ef918551b86e312f230c475702ee274271d9f9c4f0f880a5c43a9572a1b61dcd290e0d1231c3ea76f5f718a3481674297cd3eaceda2a05868bc0690201d506c8d25137a266daa5d216f7f2067e62db9ba8f841788611cbe5af6bc9532df1fb7b22e3be8e4713f23de2f692b271ffdcf4f43e0838942c9716ae8ea75f97d47ae405967977f6ebdcbc33e408163d8c504e1a6a7828940e44d66b816113fbeb5b24d1f42bd9e0c33523c5d4720fdea6c9a3d634029b1126e96b9945a2ec4c3b98c08b78e262daaad9ceda2e31e99c04507d557cc2077fda6f8cefb0a7df121a17778ea7a336ff17c3132627675d69d69b17da698b2c5756585a731f17c2f97332981efa598c66cfd1f93b61075427e578f57906946ce25a8f1a56c9fffa673fded26154990b27a8a7162e86e427cb50743bb4e8");

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x00982212b34e545f873e3ce30b0babab7d2acdedc52867b2bee24a182538633f"));
        assert(genesis.hashMerkleRoot == uint256S("0xdc6c10ad2a26613ae9b8a156ed9ca15e3e355a994a7e32cd7a4c3d7a478f57d2"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // TODO: set up bootstrapping for mainnet
        //vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be")); // Pieter Wuille
        //vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me")); // Matt Corallo
        //vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org")); // Luke Dashjr
        //vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com")); // Christian Decker
        //vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org")); // Jeff Garzik
        //vSeeds.push_back(CDNSSeedData("bitcoin.jonasschnelli.ch", "seed.bitcoin.jonasschnelli.ch")); // Jonas Schnelli

        // guarantees the first 2 characters, when base58 encoded, are "t1"
        base58Prefixes[PUBKEY_ADDRESS]     = {0x1C,0xB8};
        // guarantees the first 2 characters, when base58 encoded, are "t3"
        base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBD};
        // the first character, when base58 encoded, is "5" or "K" or "L" (as in Bitcoin)
        base58Prefixes[SECRET_KEY]         = {0x80};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x88,0xB2,0x1E};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x88,0xAD,0xE4};
        // guarantees the first 2 characters, when base58 encoded, are "zc"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {0x16,0x9A};
        // guarantees the first 2 characters, when base58 encoded, are "SK"
        base58Prefixes[ZCSPENDING_KEY]     = {0xAB,0x36};

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
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 400;
        consensus.powLimit = uint256S("03ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0x9f;
        pchMessageStart[2] = 0x24;
        pchMessageStart[3] = 0xb6;
        vAlertPubKey = ParseHex("044e7a1553392325c871c5ace5d6ad73501c66f4c185d6b0453cf45dec5a1322e705c672ac1a27ef7cdaf588c10effdf50ed5f95f85f2f54a5f6159fca394ed0c6");
        nDefaultPort = 19233;
        nMinerThreads = 0;
        nMaxTipAge = 0x7fffffff;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1475336100;
        genesis.nBits = 0x2003ffff;
        genesis.nNonce = uint256S("0x000000000000000000000000000000000000000000000000000000000000000a");
        genesis.nSolution = ParseHex("0028ddfcd35c06ddeb8374af425f481ddefe9b4f780b966a66109fdce1f53f31210dcd666144426b2fe904d3b92c6aed7fb574aba3dfa3204216ae355f23833058b1fa89d264fcb2de9500bc61e6fe0318b6f9ba01e3fb7a8bc66ba99402063f0a4f571adb459c8aec27bd4cfe8ee321a12aef232d5856842aa8d51cbb5b10d8241e4c900697ec536525925a839b3f775d85bc16d585b26e245d2b3a2e5444d9fae193ab659fd2fc045979bf8e6317413c23c127bbab52d65172dbda2409c194ae98fc7767e655f2cc14408392350b98ab294b541f085d9e71ab204834e566e6df4947b2fbef895ad7978e92f2e3e9ed814642905d866e88f1bf64060d02c1dbd48988d9fe2350d35e6c2ce5cfcbd3de77503032c5dbd7edb16bd7c6eccdca322a1f257d4df42dd15b7ff799ba18ea42968fa2e1c1e737047bea3f4048c67f1c5f42778f14c5862c64354a9b2e5ce61e02df7b3e4c7831b5e9f48088ceb5fc596b5a92dc160511e87c5c4f32b8fabd52d3cbcb419d12f77ba9cd05d69a05b84c07ce8e8821d56668756514b9f42310171f41d2caa34a25cbca517206ff5bd294051b66fb13ad55c29c0bec4b09b0a5c45fd9f725eca85e99f81933ca1682d4033b250131c69e608ec2afb89fe97a1d2fe5d541270adff918725961da3e752e574a68cf39454b8c60d6432d05060944e6d4ab874e5afe3b55067f0273dcde975797a183059a52838de09bf032872339e3262bcdb0639ae7a2d74522e03b34609ed7dd28f4759325ca90566bcca2aabd963c169ebb99b72156392b5f9ca4dd5f2ad30bfd52e5254f3e259de0050899af8361d7bb33fb9433055cf832c9e2367cfc84230a8bca3bd5647ae000a42f9751bf59dfc40f09121e8a99579b490f5c56a823338da090ae5e46777fd64574f4da6e67c633b43bd8b92356fe7e84647a3863009cd1b16f12a1f37eccd3098cb7cc76986937b28a46d1cd79f8a76e71d973053902e675a14e72fbc3c40677a1cebaab2dc5dece9067f2d9fd6d690f7fd23414dccdbd2b8ba8e19a92d45b72ec6c5da3b83fbb3a03de5d73fcca9e654736619f6839125c9c36bc44bd4d65af25f5685a13d9bce5c1ce31e2761ca49fa1ff113b8b06bba9a7c1a8ba42354e169a6d6498da3aaa12247eea3b57a9f1797965e70b461d820a04b082c005a5a7383c649fc9afbd940b303e9c555a4f3548ea1a9a70de966206edea3071f24db356d2825b7bdf2e0c68d930756df8e7967df3834a5e9fd0ff1ef8434d20e7b2c76cd092e55e51838daeb7d19e4519bbda721100b29144c4f0bc3f58d385e7a95475f2db3cefcc4dfd7367ef9461a8a84ca542685008d77565dd99d71382aff9a29ff5ab89b8a5752cc42492789d1ec1af53517cdb9fd6677d821dc6caaf4af5c9cd361df2f802680a4f0750d91fc2daf042315646af87107c842c2e069b5ea82019d7a725f30b3a5db20761bf5d5f7818705a265709f4edbc3611cc811dbf15636450fcd04d87e6a0539c8ebd8a32464224ff11c668aa9515d60bd4f8ad8dc57cb37c59f28044b0ca216729111bea59a4ef2d63f05a13a813a67def68aa72638c3ad11b0e391b625e20350fe790d20416602609963def30361f51ddb371e52f675dd962e937d53e35820bb42bae04496a84cc663c3fea6e0abb467298d2e3225ad5e90b9cde19648a9f89171982177d404ec4c4fad6304250226e6a865a87b3a2fb47c8124ce7229ac5976dac642e7bf38e2950f1f1af264bd6d890a5d4fcd8a9ae0a016924cbb0c5f79b7510da5c6a7302c7d938841015fa00bd2829769df0ab68155644c1ee1701751c280b18691904992f915e3c8122c3b13095ca8efa592411c6f54fd20d1c316a8c22cae99896a18268fbbd16");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x006ae96052293ac6d455869bd6f8c109474fd642b04deb539d871fc67b7b334c"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("dw.cash", "dnsseed.testnet.dw.cash")); // DeepWebCash

        // guarantees the first 2 characters, when base58 encoded, are "tm"
        base58Prefixes[PUBKEY_ADDRESS]     = {0x1D,0x25};
        // guarantees the first 2 characters, when base58 encoded, are "t2"
        base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBA};
        // the first character, when base58 encoded, is "9" or "c" (as in Bitcoin)
        base58Prefixes[SECRET_KEY]         = {0xEF};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x35,0x87,0xCF};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x35,0x83,0x94};
        // guarantees the first 2 characters, when base58 encoded, are "zt"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {0x16,0xB6};
        // guarantees the first 2 characters, when base58 encoded, are "ST"
        base58Prefixes[ZCSPENDING_KEY]     = {0xAC,0x08};

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
        genesis.nBits = 0x200f0f0f;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000004");
        genesis.nSolution = ParseHex("03d0ef19f0d9e1f5f70c9f577d9256ad837e0ebc043aa26fe4cbce142d870592443a7979");
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 19444;
        assert(consensus.hashGenesisBlock == uint256S("0x01e1d616a228c67500837a97d1b689ed1bf9bb3e912c302e544d44541e71d387"));
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

    // #1398 START
    // We can remove this code when miner_tests no longer expect this script
    if (fMinerTestModeForFoundersRewardScript) {
        auto rewardScript = ParseHex("a9146708e6670db0b950dac68031025cc5b63213a49187");
        return CScript(rewardScript.begin(), rewardScript.end());
    }
    // #1398 END

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
