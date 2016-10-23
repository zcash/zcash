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
            "t3QW3CN6TzmTVH8M5E5aJoKzN838zAEBCuS", "t3c6yYJxXBGr3A2KjATDwDsbn8iLP9AsXfk", "t3TuV45WwNVwo3Qc4o2Mr1TenrY9FwRM18Z", "t3hmq4gpfPGhbaRaCsmD2ePidaPxWf3upsg",
            "t3Y8pxVmXa4URdCWLC4cmeGPQ9nu8eSJMvG", "t3aE5FYLigL7VZw6qjmM6ftjBiAvGiPkjo5", "t3eiUmJQGPykPH8U5gzbPd1SjGDK2k6SFvb", "t3N1ybrjKLyn7q7tpQSRxSZmYp7UgkEgHDb",
            "t3c8uR6tn6KWwrPWApBXJQuQ6daAr8bcaMg", "t3PGLi4mkL5FkMNjp9ERHtTBQguHPjT3yqA", "t3NYWdEZKfKwvBGCtJM2oMWhy1Rzot3yL3Q", "t3N7PKyV2yQnciu9cx5UbaqDRCVtev9qHpN",
            "t3YPB1besUqKeEtx9FZX1A6HcXnkXq9Vc3r", "t3UrKCimx2FA9LU4ZMHE5A9dWbxEzm5MgLj", "t3WF7VvMmEh5teWs3WnW6shMqMj3UbX6p33", "t3aBmkm1cZoF5dZuumscMjWviipvSx6hFLR",
            "t3VMf2EpazVsr4mKy4Gyp9qizCdQLrWTm9J", "t3NtLU7S7fYCtLMCjJHkxGd8kr79w7UhtpX", "t3SwJTa8B4zCEX6fth9BMaejp3pQW11DTB8", "t3PCnGjULLqDUq91XNAwU7aAx6v2WkAP12v",
            "t3MAXyTGySnb6b3No3LuoVuFL3fY7recEin", "t3hbK2KY4BQ1rQuzwufyxLzSKQQPiP9XidW", "t3aF3uT8jQSmVetYwP5qLAuTnNqWJRNLQu7", "t3PWLsr8ru9MXyq7YWGAHPcY3N9t3Zp45uo",
            "t3WzEt94EcdNXWjL8sb2KFTawZPUcL42MoX", "t3cjgEXQH2dp4Gozu6Mn45LC5HGfjrSAf9V", "t3ZmskqAyBLXkYBMGfBBpVmR3sNKN79anu3", "t3hGLeywDDaiqBro2Mi2eLD1i8LUttrBfbT",
            "t3eQdDzPZMdN4vCziFt5zar7qp1XyMwgnD6", "t3PXk3FyBi7q48paPDLtCKdzF1zDS4bpjM3", "t3W65DrnBYmU8DPJgQ44acpz3YPwoNE8pc5", "t3Tj5a6hNPksnpcRSXS2pv1f2mrPtL2bqGb",
            "t3heu9Ty3XwHAJbux6Bm4MakzS8jUL3EkFD", "t3WFEk3WwAXF6ByHxfLohtz22J1TUwsL6iu", "t3gxDHAf8iW7xBFB7JsScngfJjH6h3QBfot", "t3bn1o8hx4jYjxgC8Pdt6Z6oeFrPQPtT8pC",
            "t3XHGtWbALCLfY1jAtkUpVELPJMZpqxBtgE", "t3RUBEzMhjHpM2DQLsAmpAm9ZUHUxnEQAtM", "t3KdS1RGsiDXBKebyZvwVE2pfacNj28KLqB", "t3MmVaTutnXLqyD2F6EKnDtCKs38HxkVAdH",
            "t3LesWouCkeWkwkaQUoGnD4k1pqmn3AYkt4", "t3PmV5vRVByLMTjR1W8ZCp2xy82kpb6n99u", "t3KX34whzndhhxPKJwAyETkeVA2Q1TzBhor", "t3SPSmxWnzNYr7WQooNxkHMhVYdAKY6M5Dr",
            "t3KpcuJDA82TZhJoygng6eq72LWBCYtp7W8", "t3cHewjCnajURPGXA74kdLUkcqtRWpN3QkG", "t3a3zS49EqTFJWfDgNQhTNNFjkkhveqQssP", "t3eunPgNGpcSqEUSgAwip3Mm5nYQcPvacvX",
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
        nDefaultPort = 18233;
        nMinerThreads = 0;
        nMaxTipAge = 0x7fffffff;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nBits = 0x2003ffff;
        genesis.nNonce = uint256S("0x000000000000000000000000000000000000000000000000000000000000001d");
        genesis.nSolution = ParseHex("00206211d02ae0a9f111f32b83725bca34081ff8640a5782bc4e8d60e306a7128f5d48d37e93851c61fc103065cd85a59943a364e38f1dc1b14242af5b21831864c6938faae7d5f1ef23124f514b592fd63ebde70f771709d587887080aec110a1740594bb747fb9ef143722a94b13394bf8a7a256bb471371aa440fd7ed107c058413639c89268353bb609f97b2c7365def1d2a0316b6f89dd317a4312a07a67e59ab04647fcf00015a0d40a5c78a9cc60b22be495713addb8c945adf2830fda88b517900fafa9976917edd7ea673fb12340279b7733d06eb284a48b0b39f8dda397f4779df2934a4a1cc7095f854e9ae74890f39ff3d9cd4745e4a0269ad941f6f195bbdcb929023364c3a2ce33af6715a218b1282a28e2f8c28075525dba772e4e95d63f5093ffbd1b499b0233427b1e21eabcce4c7b65b1f570b564497e9d7c88b0f4d21b09b2e78e74dd99bf2d902b08a50f1dc718bdb7893bee8c490127abb3a91d944145ab08e19e4259470365d8460135643aa5bf4020f566250782b11fba024b2332c596aed96680ef49d444a870eae5ea3218bc5b65a9e481dd70f3eff81c80b56be8aaa109fb555aef367961ef58338cfff8e5f13efc65daf246fc5ab45016b2cdc761932953b46f31c39d2b292971f77bbfe8461d13598876b6bbeef552495b9529a91956b3262741f6b250c4a3247fb656b07c64349c1cdcea7dddf24f28fb97ec69e76d6779125673e0110db2a61af8f72ca9a4114d93e1eee80831f87eecf001de2fb33c716e7ac68281e897ad6d4b737da42c080d4717eb17ee7eb2ae2e3762c11bebde107f84b79e596b6b97237e576a4b95a3f1758bef2ba0cfbb44da9f4b641ddbaa38aa56d88521aca5b8d382749b25b16cc5979c95b82e0115e0cca1fb3726ae637eddb2d3b14616fbff816afa25cba12a9231fedc300a2bff8e446c68752055209a16cf2d16af5363ebb4411a43c0191cffb2f42a8cb4f6b4b1a96cc190f02025719eaf449dc7ac83aa1e8829cdc909efe6922dd065109e407db885f6923833ce1e74a3511a6da28a20f43eba46a21594de3248a9b726bc5eebe76fb592f1955c6bde7cdb04b1d7666824cee2906d9f757a5a716f54a42f6a3fd59b36e92b44dcd5b325295b5cfbd227ba3f03c4a1765df12827484c2b1b8a02586d8c2048bf651e2d474450f96a5bb7c3ac0760392d8c324112e0b90cf652779b116856e0e71a3e33ed47ef2f41cd445781047e86da04a41df294ba208d76a15827e1cebd3329763ff45a9f512dfc43ada1db7bc1b357f120645cfa68f47992c128786526459466c08b3f54e5a3fdcebcf33b981f5b5d88f32df9e1e444bf970c320356f7f8511ae0b6502f519b3d8c41eea4a3a7ca2a0096df56fb5fa09d774ea3524dd9376b2c4f837e300c3411b07238e532d0f93ad16defd9a42737c814704544ab5742fdd21c84bf3f3f4e76201ba6691f639088f10fa409d91351a338672f9f279dad2701efa253807ba27296d851fb007b38476a9d8f242903ee620092a61d3d34357b5249c84453e370c89f53d33d9800dd0834f3d58943d4f12855077e3e3755eb3b38df73f68ae258616ddc9d385a80d09f9a9a21a42126e7b437a4aacfed26cf53c8d480f0e4b6f3b16f3df322303198649ef4d4f8d779a61c705edb7b17d1fdefab118f60e48ce62376d963ad69af5c1ad225c003f55201132b1a2e15e248cf947019a64d2767d99adafa2b71cc1a70698d2b5dd14f8c548e7ebe0be9c82f9435f06c95cd912abae35dae3935e92486eeb25539d733239773d7a55e07aa5ed29781bd246a48ac961bc1bf907f3f2a516d353ed0cc0b1fe3ede9dea60077ba1745272ecd0ade27581e362a684175071d2e8f39af430");
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x01f4137ae8aced18e017119ec1a5ddd7f29a163e84e5809eb37aa6bd736b86a2"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("z.cash", "dnsseed.testnet.z.cash")); // Zcash

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
            "t2UNzUUx8mWBCRYPRezvA363EYXyEpHokyi", "t2N9PH9Wk9xjqYg9iin1Ua3aekJqfAtE543", "t2NGQjYMQhFndDHguvUw4wZdNdsssA6K7x2", "t27ktmq1kbeCWiQ5TZ7w5npSzcdbBmTB7v6",
            "t2GcBttAKD2WTHka8HyGc2dfvVTKYZUfHmJ", "t2Q3vxWaD9LrdqUE8Xd9Ddjpr9pUQ2aGotK", "t2TTfWDsYu998fHWzVP9Gns4fgxXXRi1Wzu", "t2KS6R4MMWdSBMjLCiw2iMyhWGRQPmyRqDn",
            "t2Q2ELrgotWv3Eec6LEtMMiiQ8dtW38u8Tj", "t2AEgJA88vTWAKqxJDFUEJWyHUtQAZi5G1D", "t2HCSdmpq1TQKksuwPQevwAzPTgfJ2rkMbG", "t2HQCPFAUQaUdJWHPhg5pPBxit7inaJzubE",
            "t2Fzqvq8Y9e6Mn3JNPb982aYsLmq4b5HmhH", "t2HEz7YZQqDUgC5h4y2WSD3mWneqJNVRjjJ", "t2GCR1SCk687Eeo5NEZ23MLsms7JjVWBgfG", "t2KyiPR9Lztq2w1w747X6W4nkUMAGL8M9KN",
            "t2UxymadyxSyVihmbq7S1yxw5dCBqJ1S4jT", "t2AVeMy7fdmTcJhckqiKRG8B7F1vccEhSqU", "t26m7LwihQzD2sH7ZVhYpPJM5j7kzwbfKW9", "t2DgwUNTe7NxuyPU6fxsB5xJXap3E4yWXrN",
            "t2U6funcXA11fC9SZehyvUL3rk3Vhuh7fzS", "t284JhyS8LGM72Tx1porSqwrcq3CejthP1p", "t29egu8QcpzKeLoPLqWS6QVMnUUPQdF6eNm", "t29LqD9p9D3B26euBwFi6mfcWu8HPA38VNs",
            "t28GsAMCxAyLy85XaasddDzaYFTtfewr86y", "t2GV44QyaikQPLUfm6oTfZnw71LLjnR7gDG", "t2U2QzNLQ1jtAu4L6xxVnRXLBsQpQvGRR2g", "t2QKGr5PNan7nrwDgseyHMN9NFeeuUjCh8b",
            "t2AfS8u6HwBeJpKpbuxztvRjupKQDXqnrwa", "t2CTRQUViQd3CWMhnKhFnUHqDLUyTxmWhJs", "t2CbM9EqszNURqh1UXZBXYhwp1R4GwEhWRE", "t2LM7uYiAsKDU42GNSnMwDxbZ8s1DowQzYH",
            "t2AgvT35LHR378AE3ouz6xKMhkTLHLJC6nD", "t285EAQXUVyi4NMddJv2QqTrnv45GRMbP8e", "t2EpMRCD5b8f2DCQ37npNULcpZhkjC8muqA", "t2BCmWXrRPiCeQTpizSWKKRPM5X6PS7umDY",
            "t2DN7X6wDFn5hYKBiBmn3Z98st419yaTVTH", "t2QJj8HeCwQ6mHwqekxxDLZntYpZTHNU62t", "t2QdHBR1Yciqn4j8gpS8DcQZZtYetKvfNj3", "t2E5cpLA1ey5VNxFNcuopeQMq2rH2NHiPdu",
            "t2EVRGtzjFAyz8CF8ndvLuiJu7qZUfDa93H", "t2KoQDk3BSFadBkuaWdLwchFuQamzw9RE4L", "t2FnR3yhTmuiejEJeu6qpidWTghRd1HpjLt", "t2BAuBAAospDc9d1u5nNGEi6x4NRJBD2PQ2",
            "t2RtKrLCGcyPkm4a4APg1YY9Wu2m4R2PgrB", "t28aUbSteZzBq2pFgj1K1XNZRZP5mMMyakV", "t2Urdy1ERfkvsFuy6Z4BkhvYGzWdmivfAFR", "t2ADinR4JrvCMd4Q1XGALPajzFrirqvhED6",
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
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000009");
        genesis.nSolution = ParseHex("0905111770fb9072e53075375f5d77c379e30bd25270f8778abdca2ee5156f5635b9b391");
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(consensus.hashGenesisBlock == uint256S("0x0b9d8b1be2041720ab879592c3be0ec7ee79fcb2d1e238f93d3d1afe58d0d867"));
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
        vFoundersRewardAddress = { "t2FwcEhFdNXuFMv1tcYwaBJtYVtMj8b1uTg" };
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
