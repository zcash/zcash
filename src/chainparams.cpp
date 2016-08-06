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
        genesis.nNonce   = uint256S("0x0000000000000000000000000000000000000000000000000000000000000000");
        genesis.nSolution = {413, 192420, 848198, 1412644, 504977, 1615456, 2008416, 2068300, 408868, 998607, 562401, 1959957, 1317667, 1572344, 1752626, 1961920, 2859, 1813535, 276061, 1642859, 233798, 574896, 724583, 2078370, 456040, 1955501, 896074, 1564747, 771441, 965878, 1315391, 2080925, 68650, 842066, 147360, 574415, 100703, 1802565, 866003, 1818624, 131442, 2061029, 362313, 652676, 435309, 1351081, 861385, 1726516, 179646, 243585, 1406671, 1850020, 474775, 1164076, 848106, 1721236, 565908, 1164997, 925132, 1346963, 889323, 1669648, 1077581, 2093975, 12554, 1743021, 1317958, 1552981, 229392, 309410, 1728018, 1770578, 514935, 1110875, 1318761, 1900398, 750152, 1255157, 1922894, 2069854, 398656, 2065572, 1375866, 1971161, 603107, 647963, 1603976, 1712409, 1214499, 1316833, 1623450, 2000687, 1296483, 1356708, 1345569, 1487042, 176818, 531206, 1853740, 2017534, 400505, 2036139, 950343, 1497393, 453105, 1841409, 605365, 1797994, 516776, 1260851, 1746107, 1927643, 193526, 219072, 606550, 627937, 1107944, 1435499, 1884410, 2083424, 254036, 1358379, 936473, 1420657, 273483, 1670774, 710833, 1477396, 28118, 504742, 672706, 1674451, 520413, 823804, 871349, 1506815, 74218, 527534, 576571, 1401027, 871560, 1875714, 1331616, 1803217, 61519, 1149279, 118296, 1068007, 244996, 1573992, 731876, 1165425, 209809, 1672848, 889556, 1790650, 247811, 554133, 468611, 753541, 70585, 772171, 665205, 1124020, 636709, 1344854, 1377816, 2004575, 278927, 1368271, 733905, 1864188, 537462, 1547196, 698165, 1015231, 255742, 1383993, 1674711, 1865585, 437316, 1299293, 691252, 1071693, 315151, 1535194, 579434, 1416852, 622404, 1628943, 1381119, 2033521, 61659, 1403386, 612684, 1810857, 495822, 1943472, 1183679, 1427799, 305456, 644359, 707088, 1505664, 475604, 960801, 1839829, 1928603, 145702, 1345153, 732871, 1515322, 248743, 1486694, 778342, 2075889, 160780, 305343, 263597, 1695428, 213255, 1063834, 1467086, 1476747, 127863, 1310373, 1248292, 1994921, 388267, 1942948, 1429154, 1872875, 536436, 633335, 1850690, 2079492, 1417146, 1985964, 1495155, 2033501, 193076, 462843, 818692, 1164464, 1026194, 1337456, 1133757, 1900091, 643805, 1638128, 1135266, 1526751, 1153573, 1328988, 1320920, 1434040, 21797, 454169, 1061772, 1909582, 1109748, 1348969, 1237822, 1871013, 213148, 1245044, 797383, 1954332, 275842, 418387, 440276, 1077571, 285393, 1660656, 875511, 1683214, 779133, 1346398, 851861, 1968667, 406461, 1695797, 435382, 1226675, 925292, 1765916, 1322483, 2018031, 40889, 1948363, 177300, 1669852, 1353483, 1355277, 1597289, 1781607, 293362, 1325344, 590055, 1633833, 780464, 1652119, 982095, 1223068, 91838, 1946324, 1556264, 1820736, 151866, 771308, 966495, 1822046, 764803, 2020310, 830745, 1314140, 804684, 1698841, 1468736, 1832066, 58310, 677749, 480979, 1887615, 88233, 635429, 474847, 1975006, 282583, 550719, 863464, 1011081, 1124299, 1148676, 2010517, 2063277, 118884, 1912501, 961975, 1869203, 141578, 1325556, 973753, 1577514, 462713, 1529188, 774266, 1994695, 1221985, 1909476, 1433943, 1685845, 80772, 1635923, 1381891, 1469512, 216484, 863394, 363800, 1124791, 158470, 1485491, 1209115, 1484957, 1004160, 1262680, 1174370, 1325662, 128408, 475150, 1341360, 1743807, 805152, 1937402, 1012137, 1023028, 158599, 1711972, 244897, 1782296, 560785, 1593728, 1393267, 1998730, 32396, 455733, 759623, 1455352, 555232, 834849, 819918, 1137045, 197161, 1447245, 267891, 2074262, 1439669, 1724434, 1837292, 1880590, 295900, 2085435, 1799148, 1821600, 361200, 905847, 615277, 1613700, 1059474, 1787136, 1751634, 2070189, 1463132, 1888358, 1714975, 1870929, 46516, 1877683, 769150, 908962, 419465, 718137, 624089, 848083, 78882, 1181051, 760558, 1793461, 856110, 1813842, 1198050, 1571768, 309311, 1261413, 655604, 1759098, 629940, 1330633, 1906612, 1949881, 466870, 730362, 1183143, 1845367, 618245, 1929435, 1483477, 1881605, 48338, 1157428, 775347, 993527, 59050, 771456, 656839, 1045597, 272340, 1506636, 853963, 1882733, 749514, 929424, 1541760, 1931266, 405205, 801642, 1391025, 1393392, 538162, 1401623, 822281, 1417036, 846345, 2096301, 1187266, 1740202, 937780, 1295340, 1476921, 1698227, 80176, 1409813, 358142, 371022, 254866, 1645450, 859281, 1405318, 182217, 1788096, 988664, 1108912, 183890, 984336, 1636147, 2012994, 506702, 511163, 686691, 1335024, 729370, 1199059, 980674, 1419496, 1099645, 1667108, 2008093, 2027417, 1407313, 1446636, 1511180, 1925809};

        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x0de0a3851fef2d433b9b4f51d4342bdd24c5ddd793eb8fba57189f07e9235d52"));
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
        pchMessageStart[0] = 0xa9;
        pchMessageStart[1] = 0xf0;
        pchMessageStart[2] = 0x94;
        pchMessageStart[3] = 0x11;
        vAlertPubKey = ParseHex("044e7a1553392325c871c5ace5d6ad73501c66f4c185d6b0453cf45dec5a1322e705c672ac1a27ef7cdaf588c10effdf50ed5f95f85f2f54a5f6159fca394ed0c6");
        nDefaultPort = 18233;
        nMinerThreads = 0;
        nMaxTipAge = 0x7fffffff;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000001");
        genesis.nSolution = {670, 218356, 1533285, 1736104, 252149, 1768761, 1015871, 1249679, 20182, 1963146, 633663, 1080234, 828001, 1453884, 871033, 1995561, 20061, 1462697, 1173859, 1796011, 479734, 1920773, 665377, 1307450, 145410, 339797, 1033865, 1118184, 355544, 1851492, 737676, 997135, 20208, 412852, 486588, 2016074, 301392, 904769, 816423, 1845307, 412846, 1419080, 1031677, 1331819, 910857, 1724836, 1794060, 1907703, 47857, 1826537, 153952, 547192, 1004706, 1791588, 1533909, 2010873, 309882, 1558232, 591293, 1371341, 358614, 1766127, 578789, 1641812, 18806, 1402619, 38851, 1922978, 124409, 1220593, 244170, 942520, 153263, 587337, 1725085, 1765168, 306965, 799664, 1318840, 1360031, 28366, 886336, 1071650, 1106395, 143113, 1520559, 673848, 1587188, 347701, 1918091, 1435742, 1706979, 1135784, 1933754, 1186074, 2023139, 36680, 1673778, 591224, 596882, 391903, 1988113, 695243, 1636422, 64645, 221153, 676320, 1464644, 440506, 731257, 919160, 1121510, 105898, 426091, 465362, 546485, 120782, 1925212, 252272, 393224, 694315, 1531475, 700051, 1305421, 779651, 969872, 1144296, 1650787, 24505, 130279, 1028397, 1809649, 527954, 1215404, 771768, 984411, 461198, 1027142, 690895, 766205, 590166, 656328, 1392936, 1543328, 128088, 1987152, 514906, 700346, 218448, 275248, 1541562, 1905003, 162534, 599872, 234124, 1454685, 564743, 1828309, 1651853, 1747339, 238843, 402860, 424758, 1622046, 738382, 1944740, 1411940, 1918912, 626170, 654180, 711861, 868765, 679225, 797723, 849197, 1284604, 248293, 1549274, 336295, 918824, 434922, 563778, 891051, 1941031, 501868, 1603486, 1440165, 1734747, 569838, 726281, 1139672, 2026757, 30618, 297069, 519000, 1782944, 114114, 618816, 1202620, 1680623, 92684, 1759252, 1363358, 1553895, 320168, 1333725, 500705, 1950750, 125696, 870383, 880566, 945104, 1123839, 1538157, 1437850, 1798241, 159976, 1880026, 413121, 980453, 1078352, 1763749, 1135812, 1393041, 38911, 1255970, 77520, 672609, 243361, 1111751, 305829, 592570, 115371, 1334256, 161177, 1928069, 232796, 1092953, 1130323, 1421654, 58285, 1131089, 630669, 1207301, 426721, 678050, 917744, 1310009, 437946, 695085, 1305719, 1633815, 734734, 1476034, 2030965, 2096260, 8487, 1869810, 321072, 1482132, 184584, 1171699, 1010169, 1548221, 468733, 1382424, 963657, 1777578, 897487, 1663222, 1492439, 1696913, 15033, 1613588, 180157, 1447722, 1449734, 1706605, 1778121, 2061918, 16349, 1253342, 47674, 153583, 85927, 905770, 167274, 387446, 38362, 2035081, 1023544, 1406652, 666821, 1763745, 1187076, 1309136, 419826, 1414198, 489773, 938188, 996872, 1146666, 1645303, 1864783, 379639, 1767391, 472669, 1978775, 554762, 1358127, 816896, 1625000, 534274, 975366, 1131451, 1781642, 581087, 636115, 1709464, 1913278, 54815, 1526959, 997489, 1092996, 327051, 2022609, 530062, 1660217, 192428, 826905, 1459522, 1578867, 905454, 2090087, 1049945, 1530402, 162924, 542835, 1684127, 1810958, 457678, 1045810, 535459, 1927151, 233903, 905209, 354311, 1620531, 669024, 1852173, 1300118, 1596339, 70846, 1969483, 146919, 197368, 587057, 621566, 1110436, 1425504, 425514, 1659447, 715265, 1642303, 1389546, 2015502, 1438335, 1914617, 242033, 458038, 636635, 1850338, 657882, 1608308, 1124638, 1146779, 324011, 1585055, 770600, 1067592, 361419, 1532411, 483088, 1474502, 9158, 1773694, 223945, 1547457, 563604, 702276, 579006, 949771, 368380, 1738481, 1842287, 1891030, 605605, 801107, 1497429, 1899299, 41696, 425082, 678727, 1951977, 99324, 656704, 301064, 1984936, 624771, 686084, 812237, 1681572, 676574, 854727, 1375119, 1694245, 41721, 646247, 1165113, 1622960, 448858, 1975088, 1058559, 1987514, 153603, 642652, 1071790, 1259258, 307146, 585138, 344873, 1916508, 247895, 515281, 420322, 1813358, 271042, 1342896, 304231, 1302391, 435065, 725416, 687901, 1648359, 573895, 1058734, 743796, 1277772, 80298, 1869183, 1448433, 2046643, 822639, 1900599, 1218167, 1475583, 500428, 1096194, 1281777, 1941324, 1152649, 1483619, 1175280, 1800101, 560585, 1222239, 692492, 792125, 884799, 1081857, 1364375, 1547503, 582829, 1663744, 1823612, 1989615, 869748, 1318152, 966334, 1441895, 101855, 138066, 683416, 1830812, 267443, 1712897, 1046064, 1408505, 134175, 2056647, 271475, 1567149, 274544, 2081969, 947980, 1417472, 555309, 1651776, 1093419, 1745275, 792299, 1143676, 889112, 1033988, 670364, 1405570, 1329883, 2044691, 710918, 2040164, 739852, 1984165};
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256S("0x51c2f288f9bb93d49ca55dd1dae38d4f4b9d52576d737dc04ae2f2234b753258"));

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
        genesis.nNonce = uint256S("0x0000000000000000000000000000000000000000000000000000000000000005");
        genesis.nSolution = {8, 205, 95, 334, 13, 385, 266, 399, 58, 389, 129, 383, 110, 284, 460, 499, 45, 496, 269, 348, 65, 242, 212, 272, 141, 477, 394, 469, 175, 185, 270, 365};
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        assert(consensus.hashGenesisBlock == uint256S("0x0d5badaa07ac1914c9b2429825cafe9273763f2c5d44eadabf1e333e50a9e281"));
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
