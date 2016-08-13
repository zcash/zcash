// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

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
        consensus.nPowMaxAdjustDown = 16; // 16% adjustment down
        consensus.nPowMaxAdjustUp = 8; // 8% adjustment up
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
        nEquihashN = 200;
        nEquihashK = 9;

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
        genesis.nSolution = {4146, 1464292, 33176, 850474, 1011652, 1980486, 1737636, 1847598, 33815, 118925, 593878, 1477064, 369052, 1521221, 820456, 1197379, 156528, 384828, 273035, 1783180, 453668, 1478665, 472646, 1848239, 195411, 1822988, 574493, 811075, 799088, 1317680, 1129740, 1381549, 68674, 278233, 219896, 864513, 749471, 1701610, 981535, 1087827, 470453, 1933311, 1467172, 1618513, 842622, 1549506, 1446899, 1856275, 75244, 1123406, 228673, 802307, 241094, 1730239, 622916, 691353, 385221, 1338024, 464401, 494923, 862285, 1120677, 1315672, 2056862, 57668, 1098459, 1800273, 2065376, 123639, 1908659, 988119, 1146109, 60412, 1239412, 485776, 1530600, 267884, 1692056, 1052554, 1098179, 71531, 1104794, 498593, 1253378, 375132, 1669670, 922311, 1143482, 104782, 276721, 288460, 1926430, 299397, 1090347, 393338, 1111155, 107414, 1374104, 548781, 1765103, 1208370, 1910647, 1268283, 1952436, 487897, 1795054, 967955, 1050827, 666023, 1853666, 705249, 1395773, 267411, 1028587, 745288, 2037718, 294235, 1643335, 1100591, 1176224, 734587, 1506325, 764624, 916206, 1129405, 2052208, 1510817, 1996864, 110926, 1077619, 250680, 1413463, 432321, 481304, 446894, 1135759, 479382, 800785, 543814, 896734, 839252, 1014309, 1537203, 1633004, 112544, 1796046, 808876, 2058115, 337582, 1726548, 552465, 1961827, 491253, 1738066, 1598676, 1862844, 824938, 1220755, 1006645, 1809357, 164628, 774731, 974867, 1003679, 947029, 2033380, 1050878, 1226504, 218181, 396860, 899711, 1784977, 1027586, 1420073, 1086184, 1541129, 326135, 497653, 1021289, 1678895, 1076498, 1968215, 1694187, 1868544, 348431, 1575054, 1189254, 1945189, 970949, 1934895, 1207350, 2071162, 117166, 1003761, 1838375, 1977361, 470957, 901186, 589257, 1370868, 305034, 1603365, 898369, 1619153, 383808, 740091, 1049480, 1142517, 529505, 902300, 551228, 1940777, 615983, 939155, 780986, 902666, 754489, 1114437, 1109912, 1409465, 982319, 1477405, 1015210, 2009872, 150979, 2055578, 251120, 1026983, 325727, 357371, 1063688, 1540388, 355502, 1892167, 602477, 639500, 487166, 1000064, 879641, 1481481, 382831, 1742703, 414351, 857594, 873435, 999008, 1777366, 1788882, 534996, 1359902, 1182982, 1514367, 1131434, 1216632, 1685548, 1930148, 9720, 1490995, 653203, 854220, 147802, 1918673, 1553543, 1610318, 468514, 1763562, 608562, 1633246, 588905, 1111107, 1799294, 1822492, 135231, 1140309, 876723, 1743315, 726128, 1068828, 1164310, 1536614, 548810, 2088817, 779645, 829326, 568760, 1810502, 627939, 1481283, 30711, 2006500, 621603, 866297, 1018214, 1209378, 1088369, 1453225, 185272, 1311165, 1004234, 1843668, 993773, 1438023, 1112298, 1883388, 56501, 1111717, 905546, 1819772, 1541808, 1683582, 1615425, 1915242, 450370, 715241, 900569, 1628779, 521771, 1343089, 1735560, 2082885, 12367, 1045718, 355776, 508949, 444056, 610888, 664145, 1133951, 206223, 2030441, 1811271, 1840617, 779829, 1318118, 1033597, 2061168, 230466, 481285, 1213718, 1685965, 652990, 1816917, 1512074, 1857703, 369514, 650088, 378938, 516915, 1069997, 1899722, 1576303, 2076649, 79537, 1559960, 186316, 1276241, 220866, 406258, 503321, 1007388, 389668, 1364304, 600860, 868067, 1548501, 2088558, 1761178, 1806576, 109331, 1581088, 1085931, 1365614, 483299, 1278399, 945016, 1437084, 269971, 749331, 787278, 1263054, 400995, 1154060, 847466, 1835093, 42352, 2026310, 310724, 473895, 590405, 970383, 812970, 1332034, 167306, 1937414, 244131, 1019446, 585050, 1261351, 1469945, 1474018, 143941, 1594106, 714936, 2070284, 1044231, 1436315, 1340535, 1967595, 257998, 1032643, 287745, 897225, 439084, 1740146, 556225, 1108290, 136563, 1027909, 492654, 703154, 208993, 1509403, 1020011, 1749085, 354664, 1119658, 404145, 1909286, 1147344, 2062834, 1363642, 1921785, 183510, 1571318, 862676, 1513786, 924323, 1512175, 1459782, 2028731, 592229, 1496602, 1546179, 2026890, 829692, 1317063, 1694972, 1929565, 64087, 1915217, 537918, 1300836, 179459, 1140217, 1451207, 1937113, 192093, 1444782, 1022418, 2074492, 331914, 1896694, 718800, 1047757, 148503, 1933481, 1009075, 1083139, 345208, 889295, 668305, 1435776, 444026, 766573, 515306, 748343, 812735, 2096840, 1122170, 1735767, 73120, 491902, 646283, 810030, 105177, 1365844, 685111, 1382232, 217160, 256417, 1066252, 1100362, 745230, 2025080, 1254777, 1968245, 366353, 685194, 878028, 1539942, 486563, 1899832, 525707, 1701474, 367913, 597173, 1558094, 1634442, 462079, 1973493, 1573448, 1810873};

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x2c06bedaacd119e86f514591dddb39de6a28750d25bc035033cf84d4086fdfe3"));
        assert(genesis.hashMerkleRoot == uint256S("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be")); // Pieter Wuille
        vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me")); // Matt Corallo
        vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org")); // Luke Dashjr
        vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com")); // Christian Decker
        vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org")); // Jeff Garzik
        vSeeds.push_back(CDNSSeedData("bitcoin.jonasschnelli.ch", "seed.bitcoin.jonasschnelli.ch")); // Jonas Schnelli

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
            1231006505, // * UNIX timestamp of last checkpoint block
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
        pchMessageStart[0] = 0x18;
        pchMessageStart[1] = 0x28;
        pchMessageStart[2] = 0x38;
        pchMessageStart[3] = 0x48;
        vAlertPubKey = ParseHex("044e7a1553392325c871c5ace5d6ad73501c66f4c185d6b0453cf45dec5a1322e705c672ac1a27ef7cdaf588c10effdf50ed5f95f85f2f54a5f6159fca394ed0c6");
        nDefaultPort = 18233;
        nMinerThreads = 0;
        nMaxTipAge = 0x7fffffff;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
        genesis.nSolution = {670, 604339, 482254, 933315, 368320, 1071273, 551750, 1247900, 94540, 1038958, 1605494, 1939604, 645061, 1891471, 1103204, 1128234, 73001, 1158025, 521584, 1416737, 78809, 597098, 1115900, 1158683, 415926, 1925723, 937211, 1459316, 431907, 1831013, 823659, 851018, 56509, 933963, 507657, 923665, 259977, 1379784, 1297828, 1755416, 449526, 610741, 1271979, 1432675, 669572, 2038078, 845330, 1273165, 126185, 681452, 405622, 687019, 412845, 862818, 687035, 1512448, 150276, 1004513, 1619076, 1762584, 159236, 475342, 162787, 1945142, 24337, 1473877, 1031382, 1740683, 205941, 420805, 808976, 1082630, 180106, 828760, 569162, 1098783, 605252, 1667551, 915433, 1542955, 62114, 2064786, 1511283, 1626036, 182422, 1540599, 426247, 1600709, 776642, 1393097, 1132005, 1526418, 880962, 1397849, 1183419, 1522163, 25445, 40526, 124674, 907425, 40207, 785298, 1047808, 1707545, 245807, 611629, 258691, 733871, 325940, 1370326, 899949, 1180155, 33878, 173245, 392623, 2013952, 387347, 734385, 1518748, 1759129, 120935, 1885054, 1277212, 1627100, 162195, 331865, 1076968, 1641659, 2039, 741616, 1224942, 1482207, 392764, 718529, 1098781, 1883843, 104652, 1047897, 680962, 704527, 1062338, 1902088, 1639616, 1664126, 20662, 1105867, 1327457, 1649236, 57888, 1410486, 1121263, 1868149, 181871, 699930, 539683, 662087, 304967, 755077, 788445, 1755398, 66140, 1222716, 1034971, 1294718, 764104, 1445948, 843547, 1240327, 122186, 1324142, 140125, 1652737, 1205008, 1875860, 1283905, 1566275, 261564, 344958, 319404, 1002826, 275678, 325438, 509372, 1521582, 487198, 1558398, 751946, 1189250, 607327, 1815606, 1214950, 1872384, 14904, 743830, 227220, 1449160, 1068437, 1291594, 1377737, 2043793, 191555, 1440539, 562983, 724345, 406012, 967306, 1648409, 1651046, 72843, 489032, 952724, 1014305, 501338, 1831872, 602467, 711112, 292383, 502892, 988566, 1268335, 888437, 1203434, 1491576, 2049356, 52209, 1873393, 193450, 366014, 325773, 1508423, 549345, 1191469, 435768, 792593, 1573311, 1903736, 637008, 2011301, 838233, 1128557, 448557, 1961555, 1375189, 1850482, 549053, 1166632, 685758, 1476606, 704455, 1676132, 1536509, 2000166, 1084116, 1948174, 1293664, 1445255, 6030, 803118, 199009, 375154, 489394, 1876387, 1236802, 2049611, 30254, 517863, 1330725, 1410333, 86834, 161552, 759700, 1480155, 50065, 2088563, 310914, 877952, 50333, 490949, 161599, 1815454, 69184, 1167642, 1353780, 2084876, 1198823, 1664665, 1320960, 1334419, 59885, 197671, 66631, 1960991, 472378, 889448, 756867, 921791, 215537, 1580425, 764095, 2028203, 480157, 527584, 1140663, 1492949, 156016, 398663, 942532, 1817331, 1016182, 1544612, 1639535, 2048540, 451726, 1120509, 715433, 2062648, 1005669, 1647769, 1229023, 1641938, 34930, 705508, 86358, 861731, 1224924, 1288258, 1658715, 1809263, 60387, 522504, 290733, 472445, 75077, 1524940, 596001, 1476887, 351172, 637020, 621410, 1107455, 521413, 2001629, 1374047, 1405713, 439298, 1349791, 1098669, 1805548, 859885, 2038024, 882699, 1284152, 50106, 1413145, 682888, 1437957, 652544, 923140, 1141909, 1927847, 614114, 1992907, 675681, 1020147, 1010952, 1877677, 1471889, 1811403, 85898, 991502, 858852, 1516747, 183180, 1008082, 1058331, 1719588, 168719, 811450, 542222, 1897503, 530487, 728667, 1245236, 1640556, 15549, 351957, 503430, 747015, 82508, 1999863, 345730, 424433, 173065, 1718976, 951400, 1634655, 392077, 1328300, 1211709, 1736967, 139648, 157917, 278699, 940663, 575209, 1429741, 921607, 1260714, 561005, 1224858, 1518375, 1597171, 930001, 987330, 1397329, 1856161, 33221, 1068553, 33736, 1586953, 200538, 1159506, 523204, 1170114, 321671, 437081, 927156, 1005738, 393194, 960565, 1129413, 1468394, 220911, 1759286, 949095, 1161405, 759616, 2028336, 814950, 1014281, 342569, 1349295, 1038940, 1589028, 372567, 1856579, 399564, 1805894, 21409, 1437606, 891851, 2030630, 179197, 1821584, 825294, 1620602, 34936, 431121, 1146621, 1158359, 672994, 1498760, 958129, 1135345, 49795, 155825, 145785, 1135382, 852175, 884529, 1448778, 1835378, 321559, 1251589, 1804533, 2086622, 336381, 1297178, 396479, 890655, 61692, 1318427, 317466, 1041324, 174082, 783440, 968467, 1494909, 176695, 230892, 642081, 1992296, 182003, 1012529, 1959035, 1987186, 107878, 1299334, 1045030, 1938117, 258034, 2062821, 431050, 1978272, 127638, 335509, 297692, 1054472, 339953, 1535596, 1530281, 2090889};
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x1941588020fe59cb2dde9cccf9cea219e0348df6c29fbbf1f8e5a7b9097bd1fc"));

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
        nEquihashN = 48;
        nEquihashK = 5;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000006");
        genesis.nSolution = {29, 48, 81, 373, 86, 359, 218, 386, 31, 308, 481, 507, 95, 319, 194, 354, 39, 390, 69, 157, 89, 363, 424, 495, 103, 270, 180, 242, 132, 410, 150, 245};
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(consensus.hashGenesisBlock == uint256S("0x78528a94213478878fedcd7ddf4f884997ac162f823c0ab0a8e21dea3e8997ed"));
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
