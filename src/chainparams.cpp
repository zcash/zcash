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

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, const uint256& nNonce, const std::vector<unsigned char>& nSolution, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    // To create a genesis block for a new chain which is Overwintered:
    //   txNew.nVersion = 3
    //   txNew.fOverwintered = true
    //   txNew.nVersionGroupId = OVERWINTER_VERSION_GROUP_ID
    //   txNew.nExpiryHeight = <default value>
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 520617983 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nSolution = nSolution;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = genesis.BuildMerkleTree();
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database (and is in any case of zero value).
 *
 * >>> from pyblake2 import blake2s
 * >>> 'Zcash' + blake2s(b'The Economist 2016-10-29 Known unknown: Another crypto-currency is born. BTC#436254 0000000000000000044f321997f336d2908cf8c8d6893e88dbf067e2d949487d ETH#2521903 483039a6b6bd8bd05f0584f9a078d075e454925eb71c1f13eaff59b405a721bb DJIA close on 27 Oct 2016: 18,169.68').hexdigest()
 *
 * CBlock(hash=00040fe8, ver=4, hashPrevBlock=00000000000000, hashMerkleRoot=c4eaa5, nTime=1477641360, nBits=1f07ffff, nNonce=4695, vtx=1)
 *   CTransaction(hash=c4eaa5, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff071f0104455a6361736830623963346565663862376363343137656535303031653335303039383462366665613335363833613763616331343161303433633432303634383335643334)
 *     CTxOut(nValue=0.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: c4eaa5
 */
static CBlock CreateGenesisBlock(uint32_t nTime, const uint256& nNonce, const std::vector<unsigned char>& nSolution, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Zcash0b9c4eef8b7cc417ee5001e3500984b6fea35683a7cac141a043c42064835d34";
    const CScript genesisOutputScript = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nSolution, nBits, nVersion, genesisReward);
}

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
void *chainparams_commandline(void *ptr);
#include "komodo_defs.h"

extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];
extern uint16_t ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT;
extern uint32_t ASSETCHAIN_INIT, ASSETCHAINS_MAGIC;
extern int32_t VERUS_BLOCK_POSUNITS, ASSETCHAINS_LWMAPOS;
extern uint64_t ASSETCHAINS_SUPPLY, ASSETCHAINS_ALGO, ASSETCHAINS_EQUIHASH, ASSETCHAINS_VERUSHASH;

const arith_uint256 maxUint = UintToArith256(uint256S("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"));

class CMainParams : public CChainParams {
public:
    CMainParams()
    {
        strNetworkID = "main";
        strCurrencyUnits = "KMD";
        consensus.fCoinbaseMustBeProtected = false; // true this is only true wuth Verus and enforced after block 12800
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 4000;
        consensus.powLimit = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        consensus.powAlternate = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        consensus.nPowAveragingWindow = 17;
        consensus.nMaxFutureBlockTime = 7 * 60; // 7 mins

        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPowTargetSpacing = 1 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true; //false;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170004;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0xee;
        pchMessageStart[2] = 0xe4;
        pchMessageStart[3] = 0x8d;
        vAlertPubKey = ParseHex("020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9");
        nDefaultPort = 7770;
        nMinerThreads = 0;
        nMaxTipAge = 24 * 60 * 60;
        nPruneAfterHeight = 100000;
        const size_t N = 200, K = 9;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;

        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1231006505;
        genesis.nBits    = KOMODO_MINDIFF_NBITS;
        genesis.nNonce   = uint256S("0x000000000000000000000000000000000000000000000000000000000000000b");
        genesis.nSolution = ParseHex("000d5ba7cda5d473947263bf194285317179d2b0d307119c2e7cc4bd8ac456f0774bd52b0cd9249be9d40718b6397a4c7bbd8f2b3272fed2823cd2af4bd1632200ba4bf796727d6347b225f670f292343274cc35099466f5fb5f0cd1c105121b28213d15db2ed7bdba490b4cedc69742a57b7c25af24485e523aadbb77a0144fc76f79ef73bd8530d42b9f3b9bed1c135ad1fe152923fafe98f95f76f1615e64c4abb1137f4c31b218ba2782bc15534788dda2cc08a0ee2987c8b27ff41bd4e31cd5fb5643dfe862c9a02ca9f90c8c51a6671d681d04ad47e4b53b1518d4befafefe8cadfb912f3d03051b1efbf1dfe37b56e93a741d8dfd80d576ca250bee55fab1311fc7b3255977558cdda6f7d6f875306e43a14413facdaed2f46093e0ef1e8f8a963e1632dcbeebd8e49fd16b57d49b08f9762de89157c65233f60c8e38a1f503a48c555f8ec45dedecd574a37601323c27be597b956343107f8bd80f3a925afaf30811df83c402116bb9c1e5231c70fff899a7c82f73c902ba54da53cc459b7bf1113db65cc8f6914d3618560ea69abd13658fa7b6af92d374d6eca9529f8bd565166e4fcbf2a8dfb3c9b69539d4d2ee2e9321b85b331925df195915f2757637c2805e1d4131e1ad9ef9bc1bb1c732d8dba4738716d351ab30c996c8657bab39567ee3b29c6d054b711495c0d52e1cd5d8e55b4f0f0325b97369280755b46a02afd54be4ddd9f77c22272b8bbb17ff5118fedbae2564524e797bd28b5f74f7079d532ccc059807989f94d267f47e724b3f1ecfe00ec9e6541c961080d8891251b84b4480bc292f6a180bea089fef5bbda56e1e41390d7c0e85ba0ef530f7177413481a226465a36ef6afe1e2bca69d2078712b3912bba1a99b1fbff0d355d6ffe726d2bb6fbc103c4ac5756e5bee6e47e17424ebcbf1b63d8cb90ce2e40198b4f4198689daea254307e52a25562f4c1455340f0ffeb10f9d8e914775e37d0edca019fb1b9c6ef81255ed86bc51c5391e0591480f66e2d88c5f4fd7277697968656a9b113ab97f874fdd5f2465e5559533e01ba13ef4a8f7a21d02c30c8ded68e8c54603ab9c8084ef6d9eb4e92c75b078539e2ae786ebab6dab73a09e0aa9ac575bcefb29e930ae656e58bcb513f7e3c17e079dce4f05b5dbc18c2a872b22509740ebe6a3903e00ad1abc55076441862643f93606e3dc35e8d9f2caef3ee6be14d513b2e062b21d0061de3bd56881713a1a5c17f5ace05e1ec09da53f99442df175a49bd154aa96e4949decd52fed79ccf7ccbce32941419c314e374e4a396ac553e17b5340336a1a25c22f9e42a243ba5404450b650acfc826a6e432971ace776e15719515e1634ceb9a4a35061b668c74998d3dfb5827f6238ec015377e6f9c94f38108768cf6e5c8b132e0303fb5a200368f845ad9d46343035a6ff94031df8d8309415bb3f6cd5ede9c135fdabcc030599858d803c0f85be7661c88984d88faa3d26fb0e9aac0056a53f1b5d0baed713c853c4a2726869a0a124a8a5bbc0fc0ef80c8ae4cb53636aa02503b86a1eb9836fcc259823e2692d921d88e1ffc1e6cb2bde43939ceb3f32a611686f539f8f7c9f0bf00381f743607d40960f06d347d1cd8ac8a51969c25e37150efdf7aa4c2037a2fd0516fb444525ab157a0ed0a7412b2fa69b217fe397263153782c0f64351fbdf2678fa0dc8569912dcd8e3ccad38f34f23bbbce14c6a26ac24911b308b82c7e43062d180baeac4ba7153858365c72c63dcf5f6a5b08070b730adb017aeae925b7d0439979e2679f45ed2f25a7edcfd2fb77a8794630285ccb0a071f5cce410b46dbf9750b0354aae8b65574501cc69efb5b6a43444074fee116641bb29da56c2b4a7f456991fc92b2");


        /*genesis = CreateGenesisBlock(
            1477641360,
            uint256S("0x0000000000000000000000000000000000000000000000000000000000001257"),
            ParseHex("000a889f00854b8665cd555f4656f68179d31ccadc1b1f7fb0952726313b16941da348284d67add4686121d4e3d930160c1348d8191c25f12b267a6a9c131b5031cbf8af1f79c9d513076a216ec87ed045fa966e01214ed83ca02dc1797270a454720d3206ac7d931a0a680c5c5e099057592570ca9bdf6058343958b31901fce1a15a4f38fd347750912e14004c73dfe588b903b6c03166582eeaf30529b14072a7b3079e3a684601b9b3024054201f7440b0ee9eb1a7120ff43f713735494aa27b1f8bab60d7f398bca14f6abb2adbf29b04099121438a7974b078a11635b594e9170f1086140b4173822dd697894483e1c6b4e8b8dcd5cb12ca4903bc61e108871d4d915a9093c18ac9b02b6716ce1013ca2c1174e319c1a570215bc9ab5f7564765f7be20524dc3fdf8aa356fd94d445e05ab165ad8bb4a0db096c097618c81098f91443c719416d39837af6de85015dca0de89462b1d8386758b2cf8a99e00953b308032ae44c35e05eb71842922eb69797f68813b59caf266cb6c213569ae3280505421a7e3a0a37fdf8e2ea354fc5422816655394a9454bac542a9298f176e211020d63dee6852c40de02267e2fc9d5e1ff2ad9309506f02a1a71a0501b16d0d36f70cdfd8de78116c0c506ee0b8ddfdeb561acadf31746b5a9dd32c21930884397fb1682164cb565cc14e089d66635a32618f7eb05fe05082b8a3fae620571660a6b89886eac53dec109d7cbb6930ca698a168f301a950be152da1be2b9e07516995e20baceebecb5579d7cdbc16d09f3a50cb3c7dffe33f26686d4ff3f8946ee6475e98cf7b3cf9062b6966e838f865ff3de5fb064a37a21da7bb8dfd2501a29e184f207caaba364f36f2329a77515dcb710e29ffbf73e2bbd773fab1f9a6b005567affff605c132e4e4dd69f36bd201005458cfbd2c658701eb2a700251cefd886b1e674ae816d3f719bac64be649c172ba27a4fd55947d95d53ba4cbc73de97b8af5ed4840b659370c556e7376457f51e5ebb66018849923db82c1c9a819f173cccdb8f3324b239609a300018d0fb094adf5bd7cbb3834c69e6d0b3798065c525b20f040e965e1a161af78ff7561cd874f5f1b75aa0bc77f720589e1b810f831eac5073e6dd46d00a2793f70f7427f0f798f2f53a67e615e65d356e66fe40609a958a05edb4c175bcc383ea0530e67ddbe479a898943c6e3074c6fcc252d6014de3a3d292b03f0d88d312fe221be7be7e3c59d07fa0f2f4029e364f1f355c5d01fa53770d0cd76d82bf7e60f6903bc1beb772e6fde4a70be51d9c7e03c8d6d8dfb361a234ba47c470fe630820bbd920715621b9fbedb49fcee165ead0875e6c2b1af16f50b5d6140cc981122fcbcf7c5a4e3772b3661b628e08380abc545957e59f634705b1bbde2f0b4e055a5ec5676d859be77e20962b645e051a880fddb0180b4555789e1f9344a436a84dc5579e2553f1e5fb0a599c137be36cabbed0319831fea3fddf94ddc7971e4bcf02cdc93294a9aab3e3b13e3b058235b4f4ec06ba4ceaa49d675b4ba80716f3bc6976b1fbf9c8bf1f3e3a4dc1cd83ef9cf816667fb94f1e923ff63fef072e6a19321e4812f96cb0ffa864da50ad74deb76917a336f31dce03ed5f0303aad5e6a83634f9fcc371096f8288b8f02ddded5ff1bb9d49331e4a84dbe1543164438fde9ad71dab024779dcdde0b6602b5ae0a6265c14b94edd83b37403f4b78fcd2ed555b596402c28ee81d87a909c4e8722b30c71ecdd861b05f61f8b1231795c76adba2fdefa451b283a5d527955b9f3de1b9828e7b2e74123dd47062ddcc09b05e7fa13cb2212a6fdbc65d7e852cec463ec6fd929f5b8483cf3052113b13dac91b69f49d1b7d1aec01c4a68e41ce157"),
            0x1f07ffff, 4, 0);*/

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x027e3758c3a65b12aa1046462b486d0a63bfa1beae327897f56c5cfb7daaae71"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));
        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("komodoplatform.com", "seeds.komodoplatform.com")); // @kolo - old static dns seeds
        vSeeds.push_back(CDNSSeedData("kolo.supernet.org", "static.kolo.supernet.org")); // @kolo - new static dns seeds ToDo
        vSeeds.push_back(CDNSSeedData("kolo.supernet.org", "dynamic.kolo.supernet.org")); // @kolo - crawler seeds ToDo
        // TODO: set up bootstrapping for mainnet
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,60);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,85);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,188);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();
        // guarantees the first two characters, when base58 encoded, are "zc"
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {22,154};
        // guarantees the first 4 characters, when base58 encoded, are "ZiVK"
        base58Prefixes[ZCVIEWING_KEY]      = {0xA8,0xAB,0xD3};
        // guarantees the first two characters, when base58 encoded, are "SK"
        base58Prefixes[ZCSPENDING_KEY] = {171,54};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

//        LogPrintf(">>>>>>>> ac_name = %u\n",GetArg("-ac_name","").c_str());

//        if ( GetArg("-ac_name","").c_str()[0] != 0 )
//        {
//        }
//        else
//        {
//        }

        if ( pthread_create((pthread_t *)malloc(sizeof(pthread_t)),NULL,chainparams_commandline,(void *)&consensus) != 0 )
        {

        }
    }
};
static CMainParams mainParams;

void CChainParams::SetCheckpointData(CChainParams::CCheckpointData checkpointData)
{
    CChainParams::checkpointData = checkpointData;
}

void *chainparams_commandline(void *ptr)
{
    CChainParams::CCheckpointData checkpointData;
    while ( ASSETCHAINS_P2PPORT == 0 )
    {
        #ifdef _WIN32
        boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
        #else
        sleep(1);
        #endif
    }
    //fprintf(stderr,">>>>>>>> port.%u\n",ASSETCHAINS_P2PPORT);
    if ( ASSETCHAINS_SYMBOL[0] != 0 )
    {
        mainParams.SetDefaultPort(ASSETCHAINS_P2PPORT);
        if ( ASSETCHAINS_RPCPORT == 0 )
            ASSETCHAINS_RPCPORT = ASSETCHAINS_P2PPORT + 1;
        mainParams.pchMessageStart[0] = ASSETCHAINS_MAGIC & 0xff;
        mainParams.pchMessageStart[1] = (ASSETCHAINS_MAGIC >> 8) & 0xff;
        mainParams.pchMessageStart[2] = (ASSETCHAINS_MAGIC >> 16) & 0xff;
        mainParams.pchMessageStart[3] = (ASSETCHAINS_MAGIC >> 24) & 0xff;
        fprintf(stderr,">>>>>>>>>> %s: p2p.%u rpc.%u magic.%08x %u %u coins\n",ASSETCHAINS_SYMBOL,ASSETCHAINS_P2PPORT,ASSETCHAINS_RPCPORT,ASSETCHAINS_MAGIC,ASSETCHAINS_MAGIC,(uint32_t)ASSETCHAINS_SUPPLY);

        // only require coinbase protection on Verus from the Komodo family of coins
        if (strcmp(ASSETCHAINS_SYMBOL,"VRSC") == 0)
        {
            mainParams.consensus.fCoinbaseMustBeProtected = true;
        }

        if (ASSETCHAINS_ALGO != ASSETCHAINS_EQUIHASH)
        {
            // this is only good for 60 second blocks with an averaging window of 45. for other parameters, use:
            // nLwmaAjustedWeight = (N+1)/2 * (0.9989^(500/nPowAveragingWindow)) * nPowTargetSpacing 
            mainParams.consensus.nLwmaAjustedWeight = 1350;
            mainParams.consensus.nPowAveragingWindow = 45;
            mainParams.consensus.powAlternate = uint256S("00000f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        }

        if (ASSETCHAINS_LWMAPOS != 0)
        {
            mainParams.consensus.posLimit = uint256S("000000000f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
            mainParams.consensus.nPOSAveragingWindow = 45;
            // spacing is 1000 units per block to get better resolution, POS is 50% hard coded for now, we can vary it later
            // when we get reliable integer math on nLwmaPOSAjustedWeight
            mainParams.consensus.nPOSTargetSpacing = VERUS_BLOCK_POSUNITS * 2;
            // nLwmaPOSAjustedWeight = (N+1)/2 * (0.9989^(500/nPOSAveragingWindow)) * nPOSTargetSpacing
            // this needs to be recalculated if VERUS_BLOCK_POSUNITS is changed
            mainParams.consensus.nLwmaPOSAjustedWeight = 46531;
        }

        checkpointData = //(Checkpoints::CCheckpointData)
            {
                boost::assign::map_list_of
                (0, mainParams.consensus.hashGenesisBlock)
                (10000, uint256S("0xac2cd7d37177140ea4991cf630c0b9c7f94d707b84fb0351bf3a44856d2ae5dc"))
                (20000, uint256S("0xb0e8cb9f77aaa7ff5bd90d6c08d06f4c4bf03e00c2b8a35a042e760845590c8a"))
                (30000, uint256S("0xf2112ca577338ad7104bf905fa6a63d36b17a86f914c97b73cd31d43fcd7557c"))
                (40000, uint256S("0x00000000008f83378dab727864b763ce91a4ea5f75d55939c0c1390cfb8c38f1"))
                (49170, uint256S("0x2add646c0089871ec2379f02f7cd60b3af6efd9c152a6f16fc10925458c270cc")),
                (int64_t)1529910234,    // * UNIX timestamp of last checkpoint block
                (int64_t)63661,         // * total number of transactions between genesis and last checkpoint
                                        //   (the tx=... number in the SetBestChain debug.log lines)
                (double)2777            // * estimated number of transactions per day after checkpoint
                                        //   total number of tx / (checkpoint block height / (24 * 24))
            };

    }
    else
    {
            checkpointData = //(Checkpoints::CCheckpointData)
            {
                boost::assign::map_list_of

                (0, mainParams.consensus.hashGenesisBlock)
                (	5000,	uint256S("0x049cfc91eef411e96603a42c9a77c5e30e9fe96f783ab818f4c00fb56fb29b6c"))
                (	10000,	uint256S("0x0a0169db3614311cd4181deb73cfcf7f640e7dc956cda34e0121a0351925e9ae"))
                (	15000,	uint256S("0x00f0bd236790e903321a2d22f85bd6bf8a505f6ef4eddb20458a65d37e14d142"))
                (	20000,	uint256S("0x01bbf0c38892bdcced62b538329cf63bc7badca3e7e1bff8eb10345436871c6e"))
                (	25000,	uint256S("0x04ca27808268dda8f942b647a6df844be1b263a661a13740293db962022d1f9e"))
                (	30000,	uint256S("0x04c9e8cfbcd37399085e529b50147de8afb80c76c48752c122d56f23316a7acb"))
                (	35000,	uint256S("0x00815f1240354cff7487c67f7dff78e248cb9053ed2c92751d1a9ad42d3eaedf"))
                (	40000,	uint256S("0x00eafd9dfb1e5f1bf1cca0c49be628538900daf69b665464443d29c2c3b6a2fe"))
                (	45000,	uint256S("0x0377730632caf694b92f40d03ae0fbe5bd86a1205014b71d975453ac793b0af9"))
                (	50000,	uint256S("0x00076e16d3fa5194da559c17cf9cf285e21d1f13154ae4f7c7b87919549345aa"))
                (	55000,	uint256S("0x0005a0701a83e05b639418ea4c87018544a4d22b2b49e5f111161e8ffc455108"))
                (	60000,	uint256S("0x0000296fc15f8599b7c6561d0e0a96f24766135ed79107b603d6dd6e55142c0d"))
                (	65000,	uint256S("0x000861f5d7970d5399733b4605074d47f877d6536f74ffae6f08e871ee29e6f2"))
                (	70000,	uint256S("0x0002af1d487c567526c517b52996944dca344e139cddca77c2e72f746e73b263"))
                (	75000,	uint256S("0x0d08129659be5f105e70c047769359eaf3475d61a726750859fdca3e1a2bf5cc"))
                (	80000,	uint256S("0x0af5f3f1caae4f08c74a82689731d1ef8e55107c06f9a996e251b8ecb96989ad"))
                (	85000,	uint256S("0x00000c8ee29086c5fb39eddad0619773b9ce936c77c13e5e5118a4998e939544"))
                (	90000,	uint256S("0x06d3bb7f9ee5b55f67b2dc13c680699a2f736f43a44b4e4cfd41a58aa00f063f"))
                (	95000,	uint256S("0x0670981b269879aae83a88f6f0c4db34763c93fd410d96435f2acb4e6580b976"))
                (	100000,	uint256S("0x0f02eb1f3a4b89df9909fec81a4bd7d023e32e24e1f5262d9fc2cc36a715be6f"))
                (	105000,	uint256S("0x018b97d7e6d259add24afe0e08fc125dc21d734e8831b68b430f5c3896deb4af"))
                (	110000,	uint256S("0x09644ff52734e0e911a9ba7ecd03cf7995b25301840a9637891ef9af69f59c32"))
                (	115000,	uint256S("0x0ee382b4729b8ceb918a92913f9c144a6a4f8a50bfc0f8b4aac5b12592caed7f"))
                (	120000,	uint256S("0x082a7918a0dd9cb2df65f55acb8d0a4a535b3fa684d92c3ebcb24ed7019d975b"))
                (	125000,	uint256S("0x00008f76c4484fd539c9d02fc69c40a50b6f9e00984d33890b85cc0324159e9e"))
                (	130000,	uint256S("0x011b09e53acfe46f310e8c960a9c4f4f490cc7b2cd3791d7a6a80d6e8ac96b36"))
                (	135000,	uint256S("0x01e0cd48358fa05646baa6f00e26717474d6049a537c8861b324d1f497dc3d4a"))
                (	140000,	uint256S("0x0e6db36fd8a9d1b7baf359c8bd5c76635d0bcada973a75b5d2028ca3baea4961"))
                (	145000,	uint256S("0x00010c40b57316ce6cde076807c9db956452a3c82cb09fe7d56c6bb1a7e24726"))
                (	150000,	uint256S("0x0a817f15b9da636f453a7a01835cfc534ed1a55ce7f08c566471d167678bedce"))
                (	155000,	uint256S("0x0528084fd00223bd9747635d7a4d8cc79f158795cad654efb78e4e4cc5f23d6a"))
                (	160000,	uint256S("0x00003a09f26ae9fb7ebbfa3ef589b81ccd8909a82430f7414bc68d5a5a3316ab"))
                (	165000,	uint256S("0x00004a0c6a29e7d1f22ea4e44d05e861fec5fcd8eebc5a61574c4ecf29dbb9be"))
                (	170000,	uint256S("0x0cf9eac27badc0ae9a2b370dd7cc3fcb550f139349551e60978f394a2e1b262b"))
                (	175000,	uint256S("0x0000137856b825d431da27ff4c3cf22f5482fa21952d45b0db0ec6774fb9b510"))
                (	180000,	uint256S("0x000000b0afcccf98aa0afb6ac61050892bd9415857d66313d1f67fd1bbac312f"))
                (	185000,	uint256S("0x00c2af8f88d84de080067f8ae1c25754e32e5516d20c11f85b9adae2d683687b"))
                (	190000,	uint256S("0x00000033d85b3e7d19e02278ef300b8ab957d3dd3e58b4c81166ba0a58af5c3f"))
                (	195000,	uint256S("0x000000964b6068be1dd4ee6893d183e86cba82a2744fb5439c463d0ba7e053b6"))
                (	200000,	uint256S("0x000001763a9337328651ca57ac487cc0507087be5838fb74ca4165ff19f0e84f"))
                (	205000,	uint256S("0x049fc6832e64a75ae898b32804e151e7561ea49082858c3d4af89a7de4b82f06"))
                (	210000,	uint256S("0x0000000d9078b9c9604cc663eafafba8f3643bb3f3ddbb78fed4993236e1edb5"))
                (	215000,	uint256S("0x00060089ecc21bcc62094e2f7f0448fe163415f6ef2f2aafe047757889ca82fe"))
                (	220000,	uint256S("0x000082c78e6c2a13a9c23dd7a6faaf962fc133142b4a2d07725561f59c03bfa2"))
                (	225000,	uint256S("0x00030026483167fe13505cf27049307ce42e0d9c5aa093aed10baa4f49edf4ca"))
                (	230000,	uint256S("0x000183a3e17988060a35776b99c1f0b43393bbe7153b2718dfc57f428191de4e"))
                (	235000,	uint256S("0x000184995f0ec024ed3783e322c8cfa5e68d9f0c77c3aaea301b22d311619156"))
                (	240000,	uint256S("0x0000002cc7cf6d0a44ab57f9bd3bfa11a865bbf1cd87a2081095bc90981633a3"))
                (	245000,	uint256S("0x004c5f19a88c8fe8a604006dbd2d44c94baef2a00876a17d8e2be2124003f979"))
                (	250000,	uint256S("0x0dd54ef5f816c7fde9d2b1c8c1a26412b3c761cc5dd3901fa5c4cd1900892fba"))
                (	255000,	uint256S("0x0b6da9e4f50c8bc7a92c539bc7474ffd6c29e0a8531f0dbbbc261fff1f990827"))
                (	260000,	uint256S("0x0cac8b12bf7233ee5a68fcde9e251852b177833fefa2a9f39ec28474b0851cb9"))
                (	265000,	uint256S("0x04feb5b4029f3b8b8eb3e6661a78eadd1a26b4af00ac59b5f05b261afcfd2818"))
                (	270000,	uint256S("0x01bc5897bd20b8b61acf4989987ba85fbc37d9ebe848924aa8effcb08bf48fe0"))
                (	275000,	uint256S("0x0416bc29eb5a12231826e546ba90fcd38aeef387ff77b45849cd418a9c1a6f12"))
                (	280000,	uint256S("0x000007593e9880b171d46bce59aa0cec2a1b1f53d1fd7e8f71ccb2b9182374a4"))
                (	285000,	uint256S("0x05a338b2d90cd79740221fe8635b7a834f2e486fcbb2464a4294f5a21231a5f5"))
                (	290000,	uint256S("0x064ca3912cdcd833702d07a530e98bc5c6c1cd738a8825c7240b17cd68ca0cc4"))
                (	295000,	uint256S("0x036b3bb318d743fd78db983a9aadd52869991d48913c4eebe2a074387d67cc5a"))
                (	300000,	uint256S("0x000000fa5efd1998959926047727519ed7de06dcf9f2cd92a4f71e907e1312dc"))
                (	305000,	uint256S("0x00003656231e83de2348755153ed175794696a113d7e8a15c01f90fdb7c2f287"))
                (	310000,	uint256S("0x0cf6baf727eb931da0813ed8b032648c4766be79e146b0d40c643f9d8edf40f7"))
                (	315000,	uint256S("0x082469974c152ebe69f1787f0d06aa5d9dd1dc69c880febde7eac2bc800146dd"))
                (	320000,	uint256S("0x0000063df36b99bfb2516f55cb548a5baed1f2d8ae69c3559dc478c5c2eb32df"))
                (	325000,	uint256S("0x0cb926b303a1514ba0a2f729af88ccb143517f396e9e0bde09b0736900698e0f"))
                (	330000,	uint256S("0x000000be3d8bb6e31c3b534819aae7014cbbe9a44ab3e799dc1bfc724c6ab184"))
                (	335000,	uint256S("0x0d0756608189fd5bbd8ec50e76180074e69e973439cc09df49134e4cb970ed4d"))
                (	340000,	uint256S("0x0d814eacdb9c97003d703c0ff79b1b97b9ed8615fe12b1afaede946e5fdfe0a7"))
                (	345000,	uint256S("0x000000c2910f510f1de325d300202da1a391f2719dd378173299151c3da94e85"))
                (	350000,	uint256S("0x0000000228ef321323f81dae00c98d7960fc7486fb2d881007fee60d1e34653f"))
                (	355000,	uint256S("0x03e6a55e382b478e0fab9c3584da3629fd9b977986a333a406b24b0d3559bf44"))
                (	360000,	uint256S("0x0859c86dd718bcb5b58af06389197794e2beea6239653f2e6fa7b8a7433d29ea"))
                (	365000,	uint256S("0x07896332665c707a8f55398a998e7878e8d2681ba79dd95c2859b1dafc9343d0"))
                (	370000,	uint256S("0x040efd8c64cf5cf96ecf75468741a8880d1386eb5e349bef0a55116d4023944c"))
                (	375000,	uint256S("0x053029e7599a09fe6c01203997d7ca738dd4c6d216a433695a0d514def1eccc0"))
                (	380000,	uint256S("0x0cae44e7a421c389b88a5a204d3e39779e93aeacaab1b693741bf279fd0c8acd"))
                (	385000,	uint256S("0x0b4032d2c799ba93644231ce57134dd24e13ec0dc267c1ed5912389691d2bd72"))
                (	390000,	uint256S("0x0afd0f166f33a881ef289af7ea7010d58c4bbd560dee10b561c79e1b8dfd0593"))
                (	395000,	uint256S("0x083774b88cf1b138d67c242d9b33c54f69d7e901b5e8144dc4a2303ab9927102"))
                (	400000,	uint256S("0x036d294c5be96f4c0efb28e652eb3968231e87204a823991a85c5fdab3c43ae6"))
                (	405000,	uint256S("0x0522e33bb2161fb1b33acef9a4a438fcf420dcae8a0b472e234d223d731c42b2"))
                (	410000,	uint256S("0x0361d06aa807c66b87befea8119a485341d1118b694c3dbb4c3cf0b85ac69e9b"))
                (	415000,	uint256S("0x072d5653d8673f64ef8b9c655f7b8021072070a072b799013ff6e96de99a59e6"))
                (	420000,	uint256S("0x013b693d66955be69d4501cb1d307ca323a5c8473e25866ae7e700cdce0c654f"))
                (	425000,	uint256S("0x0ef0c55af27c6971289a790dee2b2ec728fb9c6555ff9306c07f1083cf0fb4b5"))
                (	430000,	uint256S("0x0ccbeeaba28291e0316a9cf54c005097c61dc67ba6f32283406d6c83b828da00"))
                (	435000,	uint256S("0x020ed6b7fe1124400baba7feed463ba0c90e7e6903493fdc1a1a18c4a506055a"))
                (	440000,	uint256S("0x055aaadca1908abeedc831a3f9115aa31284fc223d010590caf7b612960b61a4"))
                (	445000,	uint256S("0x06d2327fa25ea7e2be742fc0e45fc4f9adb41811f21be0357f8543c5434df715"))
                (	450000,	uint256S("0x0906ef1e8dc194f1f03bd4ce1ac8c6992fd721ef2c5ccbf4871ec8cdbb456c18"))
                (	455000,	uint256S("0x0b8b92eec29eb20262dcf9916f0ca36d6abf0c39d321d3f480a5535cb978db71"))
                (	460000,	uint256S("0x0cb04591f69a255b1127aaff3bbd59eaa21a5d9cca999de197516c251895c536"))
                (	465000,	uint256S("0x029985ae78d8bb8fd170aeb3ab02ea76134ed0c19ae00211cc28a61fe5755b88"))
                (	470000,	uint256S("0x01a2f4b56f37b223e75572862ad1ba956ec179332f8cd40590d7253563c86ba8"))
                (	475000,	uint256S("0x0a34c6f9d4d9cb8c78c14b8041a7cc1874cfcbb22a34a5c068d1d6ff3ed9fdf0"))
                (	480000,	uint256S("0x0ebab25030179996ae25969f34f6a297c7ffce1994f9b4186082a47032a9a7dc"))
                (	485000,	uint256S("0x06a096e6bccf3b85537a30f95db6a414deacc0509bc84da264c2830df1a1d9b0"))
                (	490000,	uint256S("0x0af828930ef13405cb536b88a3d1d4e0d84dc79ee260402c56bfa86e261c74ff"))
                (	495000,	uint256S("0x09d44905bfd12849d3c2178b2ba882f8e9d6565b6e4d7a97c70a92bd6de7c5e6"))
                (	500000,	uint256S("0x0bebdb417f7a51fe0c36fcf94e2ed29895a9a862eaa61601272866a7ecd6391b"))
                (	505000,	uint256S("0x0c1609f4f3561baa1fc975877948af94d2107c88686a9821bc240016cc87d953"))
                (	510000,	uint256S("0x0cf9a5a4997b871e615e5e398627e45fa15b3e6970ae22b47bdd11b0f5fa0fa7"))
                (	515000,	uint256S("0x034171d4819e9961de13309743a32a179abede97d60ea64101dc04c97a1a0807"))
                (	520000,	uint256S("0x0648fa44d5bbc2cc04a782e083c11df64ac06185f0f8e11a7416625ebb6409a6"))
                (	525000,	uint256S("0x0000000ef17d63af3159e52cd351b6f000536ad88adc3a937bb747955fed58a2"))
                (	530000,	uint256S("0x08e3af153995ba09e50086b64145cf4cd57db6b29f16f06f28d80d7f6121cfad"))
                (	535000,	uint256S("0x02a0ffd00b51e2061b85de50a9223d9c84f4e357dc1046397bb9d7d4a827a3fb"))
                (	540000,	uint256S("0x04bf07d026af29025c1ac2815e067f4a41d2872701ac9780eb3015d51cdcd854"))
                (	545000,	uint256S("0x0a0d6d86635946792ad0dca57ed227a5360fc8b6d79e47132aac11e90a4963ce"))
                (	550000,	uint256S("0x06df52fc5f9ba03ccc3a7673b01ab47990bd5c4947f6e1bc0ba14d21cd5bcccd"))
                (	555000,	uint256S("0x0baf38eea8e08fcad3a9d760f27377e79c291b08e7fb4920cadd5cb7bab547f3"))
                (	560000,	uint256S("0x00000004c34abbf1366adbae965b644c01debf15409acc715ff51cb221d92dd7"))
                (	565000,	uint256S("0x067bae7119f083e0fa1820bc8e25dcfa7717e42aabaef18beefd87d974953dfb"))
                (	570000,	uint256S("0x00000011a7ce7b628b7be17777d8dea2574d83f165e23c9e44aa705975820fd3"))
                (	575000,	uint256S("0x0e1110a193a30d3f8d369017233a2486b11c748b3d033859a2eb7b37062d303e"))
                (	580000,	uint256S("0x083cb58484aff80f48e3537e0451d49e544b3efa3da97274800c91e567d33a92"))
                (	585000,	uint256S("0x0224cf835428d03472edf4f7b6fcc63b9d8d6f1d5a90ad8186bf123d541b4ea8"))
                (	590000,	uint256S("0x0cfcf3b9517894e4df49db5faf8b74f3a9e01eb83c0cc5051c115d4424615dae"))
                (	595000,	uint256S("0x0000000a45266983dd81e0df381a3b0455699b2f76d5b4d3f17b87d657a1b56d"))
                (	600000,	uint256S("0x00000005080d5689c3b4466e551cd1986e5d2024a62a79b1335afe12c42779e4"))
                (	605000,	uint256S("0x0000001c691da36848542299af859d4eb3fa408a0f425b1fbe6d622d2100623a"))
                (	610000,	uint256S("0x040d8c7a0ac89e3ed8605a198583a795986aacbf18722a9897d7b925bcf757f6"))
                (	615000,	uint256S("0x0449cf00fc36206389c14cbf1d762f8b96bb0440ccea5b46703e7c69b0e2bc42"))
                (	620000,	uint256S("0x07227a41340c25ee1a7e9b60414259780202ffa990079fc91d8faeac9af03e60"))
                (	625000,	uint256S("0x047c2472fe2afabb3d38decf24bba4ba712b60e7a1782f4afae3ede3f912f493"))
                (	630000,	uint256S("0x0a7f1f04e66260cf972ab1374a9126b8abc1adaa3ab4669db5d4d4ddb9ad493d"))
                (	635000,	uint256S("0x048df95165eb821dabf37ef28cf7f3be72e216e95377684253dab806985b50a4"))
                (	640000,	uint256S("0x066b3c6a6a3c8dc58bef509a972c3e3ade14493b40e1b361ecbc928134e302be"))
                (	645000,	uint256S("0x07d059888c9ade3bbe16d6b4d70ee9b8302d104b37a3c6cd61f81012aabd0e1e"))
                (	650000,	uint256S("0x039a3cb760cc6e564974caf69e8ae621c14567f3a36e4991f77fd869294b1d52"))
                (	655000,	uint256S("0x089350ee8d28b44837eb4b1fe77704953d5de2077f10c74a888d9d3ea1e13c2a"))
                (	660000,	uint256S("0x000000023f8a582a61ae2f6fab6fe8197e79b7a68aaac67432421b09f1bdd4ba"))
                (	665000,	uint256S("0x0b16edce865e7a0d662115774e0c0d3abbf9c69004155b693ddc933f051bfb26"))
                (	670000,	uint256S("0x09070b109b089490bc372fd8358abae352d6db0e46ade6ed2200e4d4ff7aa6af"))
                (	675000,	uint256S("0x08d9edeed3b6ac55991e9f32af0218ff8fa9dc808078623f4c831eb09d4f186b"))
                (	680000,	uint256S("0x00000003eb2b30bfac929d3496acecab19625ac9f854a86aaf9678bea99e1cc1"))
                (	681777,	uint256S("0x0000243296b9b26c040f471fdd9398ef72e57062cf05c19b9ba2fefac8165306")),
                (int64_t)1516924927,     // * UNIX timestamp of last checkpoint block
                (int64_t)1253783,         // * total number of transactions between genesis and last checkpoint
                                //   (the tx=... number in the SetBestChain debug.log lines)
                (double)2777            // * estimated number of transactions per day after checkpoint
                                //   total number of tx / (checkpoint block height / (24 * 24))
            };
    }

    mainParams.SetCheckpointData(checkpointData);

    ASSETCHAIN_INIT = 1;
    return(0);
}

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        strCurrencyUnits = "TAZ";
        consensus.fCoinbaseMustBeProtected = true;
        consensus.nSubsidySlowStartInterval = 20000;
        consensus.nSubsidyHalvingInterval = 840000;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 400;
        consensus.powLimit = uint256S("07ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.powAlternate = uint256S("07ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowAveragingWindow = 17;
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nMaxFutureBlockTime = 7 * 60;

        vAlertPubKey = ParseHex("00");
        nDefaultPort = 17770;
        nMinerThreads = 0;
        consensus.nPowMaxAdjustDown = 32; // 32% adjustment down
        consensus.nPowMaxAdjustUp = 16; // 16% adjustment up
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170003;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight = 207500;

        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0x5A;
        pchMessageStart[1] = 0x1F;
        pchMessageStart[2] = 0x7E;
        pchMessageStart[3] = 0x62;
        vAlertPubKey = ParseHex("020e46e79a2a8d12b9b5d12c7a91adb4e454edfae43c0a0cb805427d2ac7613fd9");
        nMaxTipAge = 24 * 60 * 60;

        nPruneAfterHeight = 1000;
        const size_t N = 200, K = 9;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nBits = KOMODO_MINDIFF_NBITS;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000009");
        genesis.nSolution = ParseHex("003423da3e41f916bf3ff0ee770eb844a240361abe08a8c9d46bd30226e2ad411a4047b6ddc230d173c60537e470e24f764120f5a2778b2a1285b0727bf79a0b085ad67e6266fb38fd72ef17f827315c42f921720248c983d4100e6ebd1c4b5e8762a973bac3bec7f7153b93752ebbb465f0fc9520bcfc30f9abfe303627338fed6ede9cf1b9173a736cf270cf4d9c6999ff4c3a301a78fd50dab6ccca67a0c5c2e41f216a1f3efd049a74bbe6252f9773bc309d3f9e554d996913ce8e1cec672a1fa4ea59726b61ea9e75d5ce9aa5dbfa96179a293810e02787f26de324fe7c88376ff57e29574a55faff7c2946f3e40e451861c32bf67da7377de3136858a18f34fab1bc8da37726ca2c25fc7b312a5427554ec944da81c7e27255d6c94ade9987ff7daedc2d1cc63d7d4cf93e691d13326fb1c7ee72ccdc0b134eb665fc6a9821e6fef6a6d45e4aac6dca6b505a0100ad56ea4f6fa4cdc2f0d1b65f730104a515172e34163bdb422f99d083e6eb860cf6b3f66642c4dbaf0d0fa1dca1b6166f1d1ffaa55a9d6d6df628afbdd14f1622c1c8303259299521a253bc28fcc93676723158067270fc710a09155a1e50c533e9b79ed5edba4ab70a08a9a2fc0eef0ddae050d75776a9804f8d6ad7e30ccb66c6a98d86710ca7a4dfb4feb159484796b9a015c5764aa3509051c87f729b9877ea41f8b470898c01388ed9098b1e006d3c30fc6e7c781072fa3f75d918505ee8ca75840fc62f67c57060666aa42578a2dd022eda62e3f1e447d7364074d34fd60ad9b138f60422afa6cfcb913fd6c213b496144dbfda7bfc7c24540cfe40ad0c0fd5a8c0902127f53d3178ba1b2a87bf1224d53d3a15e49ccdf121ae872a011c996d1b9793153cdcd4c0a7e99f8a35669788551cca2b62769eda24b6b55e2f4e0ac0d30aa50ecf33c6cdb24adfc922006a7bf434ced800fefe814c94c6fc8caa37b372d5088bb31d2f6b11a7a67ad3f70abbac0d5c256b637828de6cc525978cf151a2e50798e0c591787639a030291272c9ced3ab7d682e03f8c7db51f60163baa85315789666ea8c5cd6f789a7f4a5de4f8a9dfefce20f353cec606492fde8eab3e3b487b3a3a57434f8cf252a4b643fc125c8a5948b06744f5dc306aa587bdc85364c7488235c6edddd78763675e50a9637181519be06dd30c4ba0d845f9ba320d01706fd6dd64d1aa3cd4211a4a7d1d3f2c1ef2766d27d5d2cdf8e7f5e3ea309d4f149bb737305df1373a7f5313abe5986f4aa620bec4b0065d48aafac3631de3771f5c4d2f6eec67b09d9c70a3c1969fecdb014cb3c69832b63cc9d6efa378bff0ef95ffacdeb1675bb326e698f022c1a3a2e1c2b0f05e1492a6d2b7552388eca7ee8a2467ef5d4207f65d4e2ae7e33f13eb473954f249d7c20158ae703e1accddd4ea899f026618695ed2949715678a32a153df32c08922fafad68b1895e3b10e143e712940104b3b352369f4fe79bd1f1dbe03ea9909dbcf5862d1f15b3d1557a6191f54c891513cdb3c729bb9ab08c0d4c35a3ed67d517ffe1e2b7a798521aed15ff9822169c0ec860d7b897340bc2ef4c37f7eb73bd7dafef12c4fd4e6f5dd3690305257ae14ed03df5e3327b68467775a90993e613173fa6650ffa2a26e84b3ce79606bf234eda9f4053307f344099e3b10308d3785b8726fd02d8e94c2759bebd05748c3fe7d5fe087dc63608fb77f29708ab167a13f32da251e249a544124ed50c270cfc6986d9d1814273d2f0510d0d2ea335817207db6a4a23ae9b079967b63b25cb3ceea7001b65b879263f5009ac84ab89738a5b8b71fd032beb9f297326f1f5afa630a5198d684514e242f315a4d95fa6802e82799a525bb653b80b4518ec610a5996403b1391");
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x05a60a92d99d85997cce3b87616c089f6124d7342af37106edc76126334a2c38"));

        vFixedSeeds.clear();
        vSeeds.clear();
        //vSeeds.push_back(CDNSSeedData("z.cash", "dns.testnet.z.cash")); // Komodo

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {20,81};
        // guarantees the first 4 characters, when base58 encoded, are "ZiVt"
        base58Prefixes[ZCVIEWING_KEY]  = {0xA8,0xAC,0x0C};
        base58Prefixes[ZCSPENDING_KEY] = {177,235};

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        //fRequireRPCPassword = true;
        fMiningRequiresPeers = false;//true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (0, consensus.hashGenesisBlock)
            (38000, uint256S("0x001e9a2d2e2892b88e9998cf7b079b41d59dd085423a921fe8386cecc42287b8")),
            1486897419,  // * UNIX timestamp of last checkpoint block
            47163,       // * total number of transactions between genesis and last checkpoint
                         //   (the tx=... number in the SetBestChain debug.log lines)
            715          //   total number of tx / (checkpoint block height / (24 * 24))
        };
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
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
        consensus.powAlternate = uint256S("0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f0f");
        consensus.nPowAveragingWindow = 17;
        consensus.nMaxFutureBlockTime = 7 * 60; // 7 mins
        assert(maxUint/UintToArith256(consensus.powLimit) >= consensus.nPowAveragingWindow);
        consensus.nPowMaxAdjustDown = 0; // Turn off adjustment down
        consensus.nPowMaxAdjustUp = 0; // Turn off adjustment up
        consensus.nPowTargetSpacing = 2.5 * 60;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::BASE_SPROUT].nActivationHeight =
            Consensus::NetworkUpgrade::ALWAYS_ACTIVE;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nProtocolVersion = 170002;
        consensus.vUpgrades[Consensus::UPGRADE_TESTDUMMY].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nProtocolVersion = 170003;
        consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight =
            Consensus::NetworkUpgrade::NO_ACTIVATION_HEIGHT;

        pchMessageStart[0] = 0xaa;
        pchMessageStart[1] = 0x8e;
        pchMessageStart[2] = 0xf3;
        pchMessageStart[3] = 0xf5;
        nMinerThreads = 1;
        nMaxTipAge = 24 * 60 * 60;
        nPruneAfterHeight = 1000;
        const size_t N = 48, K = 5;
        BOOST_STATIC_ASSERT(equihash_parameters_acceptable(N, K));
        nEquihashN = N;
        nEquihashK = K;
        genesis.nTime = 1296688602;
        genesis.nBits = KOMODO_MINDIFF_NBITS;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000021");
        genesis.nSolution = ParseHex("0f2a976db4c4263da10fd5d38eb1790469cf19bdb4bf93450e09a72fdff17a3454326399");
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 17779;
        nPruneAfterHeight = 1000;

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")),
            0,
            0,
            0
        };
        // These prefixes are the same as the testnet prefixes
        base58Prefixes[PUBKEY_ADDRESS]     = {0x1D,0x25};
        base58Prefixes[SCRIPT_ADDRESS]     = {0x1C,0xBA};
        base58Prefixes[SECRET_KEY]         = {0xEF};
        // do not rely on these BIP32 prefixes; they are not specified and may change
        base58Prefixes[EXT_PUBLIC_KEY]     = {0x04,0x35,0x87,0xCF};
        base58Prefixes[EXT_SECRET_KEY]     = {0x04,0x35,0x83,0x94};
        base58Prefixes[ZCPAYMENT_ADDRRESS] = {0x16,0xB6};
        base58Prefixes[ZCVIEWING_KEY]      = {0xA8,0xAC,0x0C};
        base58Prefixes[ZCSPENDING_KEY]     = {0xAC,0x08};

        // Founders reward script expects a vector of 2-of-3 multisig addresses
        vFoundersRewardAddress = { "t2FwcEhFdNXuFMv1tcYwaBJtYVtMj8b1uTg" };
        assert(vFoundersRewardAddress.size() <= consensus.GetLastFoundersRewardBlockHeight());
    }

    void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
    {
        assert(idx > Consensus::BASE_SPROUT && idx < Consensus::MAX_NETWORK_UPGRADES);
        consensus.vUpgrades[idx].nActivationHeight = nActivationHeight;
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
    CScriptID scriptID = boost::get<CScriptID>(address.Get()); // Get() returns a boost variant
    CScript script = CScript() << OP_HASH160 << ToByteVector(scriptID) << OP_EQUAL;
    return script;
}

std::string CChainParams::GetFoundersRewardAddressAtIndex(int i) const {
    assert(i >= 0 && i < vFoundersRewardAddress.size());
    return vFoundersRewardAddress[i];
}

void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight)
{
    regTestParams.UpdateNetworkUpgradeParameters(idx, nActivationHeight);
}
