#include <gtest/gtest.h>
#include <univalue.h>

#include "chain.h"
#include "chainparams.h"
#include "clientversion.h"
#include "main.h"
#include "primitives/block.h"
#include "rpc/server.h"
#include "streams.h"
#include "util/strencodings.h"

extern UniValue blockToJSON(const CBlock& block, const CBlockIndex* blockindex, bool txDetails = false);

TEST(rpc, CheckBlockToJSONReturnsMinifiedSolution) {
    SelectParams(CBaseChainParams::TESTNET);

    // Testnet block 006a87f9f91c1f51c7549e2c8965c0fd4fe8c212798f932efc54dc7bccbec780
    // Height 1391
    CDataStream ss(ParseHex("0400000077be515306e347c6856686d83a229169140a2f7e17281c8319ecf00c49bb6f00994ca400914d6733295faf4e0063998e75a18aae7d39b5244d88d082c13145070000000000000000000000000000000000000000000000000000000000000000ae71c25700737b1f010090f8a62f53105d6b6f173d242fbbf54b0c1024a64520f0020e47fe710000fd4005009f44ff7505d789b964d6817734b8ce1377d456255994370d06e59ac99bd5791b6ad174a66fd71c70e60cfc7fd88243ffe06f80b1ad181625f210779c745524629448e25348a5fce4f346a1735e60fdf53e144c0157dbc47c700a21a236f1efb7ee75f65b8d9d9e29026cfd09048233175202b211b9a49de4ab46f1cac71b6ea57a686377bd612378746e70c61a659c9cd683269e9c2a5cbc1d19f1149345302bbd0a1e62bf4bab01e9caeea789a1519441a61b146de35a4cc75dbdf01029127e311ad5073e7e96397f47226a7df9df66b2086b70756db013bbaeb068260157014b2602fc7dc71336e1439c887d2742d9730b4e79b08ec7839c3e2a037ae1565d04e05e351bb3531e5ef42cf7b71ca1482a9205245dd41f4db0f71644f8bdb88e845558537c03834c06ac83f336651e54e2edfc12e15ea9b7ea2c074e6155654d44c4d3bd90d9511050e9ad87d170db01448e5be6f45419cd86008978db5e3ceab79890234f992648d69bf1053855387db646ccdee5575c65f81dd0f670b016d9f9a84707d91f77b862f697b8bb08365ba71fbe6bfa47af39155a75ebdcb1e5d69f59c40c9e3a64988c1ec26f7f5159eef5c244d504a9e46125948ecc389c2ec3028ac4ff39ffd66e7743970819272b21e0c2df75b308bc62896873952147e57ed79446db4cdb5a563e76ec4c25899d41128afb9a5f8fc8063621efb7a58b9dd666d30c73e318cdcf3393bfec200e160f500e645f7baac263db99fa4a7c1cb4fea219fc512193102034d379f244c21a81821301b8d47c90247713a3e902c762d7bafa6cdb744eeb6d3b50dd175599d02b6e9f5bbda59366e04862aa765135968426e7ac0116de7351940dc57c0ae451d63f667e39891bc81e09e6c76f6f8a7582f7447c6f5945f717b0e52a7e3dd0c6db4061362123cc53fd8ede4abed4865201dc4d8eb4e5d48baa565183b69a5304a44c0600bb24dcaeee9d95ceebd27c1b0a33e0b46f23797d7d7907300b2bb7d62ef2fc5aa139250c73930c621bb5f41fc235534ee8014dfaddd5245aeb01198420ba7b5c076545329c94d54fa725a8e807579f5f0cc9d98170598023268f5930893620190275e6b3c6f5181e36310a9a475208316911d78f917d724c5946c553b7ec042c563c540114b6b78bd4c6e808ee391a4a9d93e127032983c5b3708037b14aa604cfb034e7c8b0ffdd6936446fe80216178506a87402653a373926eeff66e704daf992a0a9a5c3ad80566c0339be9e5b8e35b3b3226b2f7767e20d992ea6c3d6e322eca37b0c7f7e60060802f5abcc1975841365cadbdc3867063addfc803766ae525375ecddee61f9df9ffcd20343c83ab82b0e91de039c59cb435c8d3159cc338b4901f40c9b5c27043bcf2bd5fa9b685b65c9ba5a1e11a51dd3f773051560341f9ec81d05bf259e2d4b7161f896fbb6812cfc924a32120b7367d5e40439e267adda6a1315bb0d6200ce6a503174c8d2a638ea6fd6b1f486d68db11bdca63c4f4a725d1ab6231ea875484e70b27d293c05803386924f283d4c12bb953474d92b7dd43d2d97193bd96281ebb63fa075d2f9ecd310c70ee1d97b5330bd8fb5791c5943ecf084e5f2c83915acac57519c46b166136068d6f9ec0dd598616e32c591128ce13705a283ca39d5b211409600e07b3713113374d9700207a45394eac5b3b7afc9b1b2bad7d89fd3f35f6b2413ce615ee7869b3569009403b96fdacdb32ef0a7e5229e2b666d51e95bdfb009b892e88bde70621a9b6509f068781392df4bdbc5723bb15071993f0d9a11575af5ff6ef85eaea39bc86805b35d8beee91b779354147f2d85304b8b49d053e7444fdd3deb9d16de331f2552af5b3be7766bb8f3f6a78c62148efb231f22680101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff05026f050101ffffffff02b03f250400000000232103885e6a80a5702046eb76c4702921b75858fc633df3cddff827cf7b3602e45cbdacec4f09010000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000"), SER_DISK, CLIENT_VERSION);
    CBlock block;
    ss >> block;

    CBlockIndex index {block};
    index.nHeight = 1391;

    LOCK(cs_main);
    UniValue obj = blockToJSON(block, &index);
    EXPECT_EQ("009f44ff7505d789b964d6817734b8ce1377d456255994370d06e59ac99bd5791b6ad174a66fd71c70e60cfc7fd88243ffe06f80b1ad181625f210779c745524629448e25348a5fce4f346a1735e60fdf53e144c0157dbc47c700a21a236f1efb7ee75f65b8d9d9e29026cfd09048233175202b211b9a49de4ab46f1cac71b6ea57a686377bd612378746e70c61a659c9cd683269e9c2a5cbc1d19f1149345302bbd0a1e62bf4bab01e9caeea789a1519441a61b146de35a4cc75dbdf01029127e311ad5073e7e96397f47226a7df9df66b2086b70756db013bbaeb068260157014b2602fc7dc71336e1439c887d2742d9730b4e79b08ec7839c3e2a037ae1565d04e05e351bb3531e5ef42cf7b71ca1482a9205245dd41f4db0f71644f8bdb88e845558537c03834c06ac83f336651e54e2edfc12e15ea9b7ea2c074e6155654d44c4d3bd90d9511050e9ad87d170db01448e5be6f45419cd86008978db5e3ceab79890234f992648d69bf1053855387db646ccdee5575c65f81dd0f670b016d9f9a84707d91f77b862f697b8bb08365ba71fbe6bfa47af39155a75ebdcb1e5d69f59c40c9e3a64988c1ec26f7f5159eef5c244d504a9e46125948ecc389c2ec3028ac4ff39ffd66e7743970819272b21e0c2df75b308bc62896873952147e57ed79446db4cdb5a563e76ec4c25899d41128afb9a5f8fc8063621efb7a58b9dd666d30c73e318cdcf3393bfec200e160f500e645f7baac263db99fa4a7c1cb4fea219fc512193102034d379f244c21a81821301b8d47c90247713a3e902c762d7bafa6cdb744eeb6d3b50dd175599d02b6e9f5bbda59366e04862aa765135968426e7ac0116de7351940dc57c0ae451d63f667e39891bc81e09e6c76f6f8a7582f7447c6f5945f717b0e52a7e3dd0c6db4061362123cc53fd8ede4abed4865201dc4d8eb4e5d48baa565183b69a5304a44c0600bb24dcaeee9d95ceebd27c1b0a33e0b46f23797d7d7907300b2bb7d62ef2fc5aa139250c73930c621bb5f41fc235534ee8014dfaddd5245aeb01198420ba7b5c076545329c94d54fa725a8e807579f5f0cc9d98170598023268f5930893620190275e6b3c6f5181e36310a9a475208316911d78f917d724c5946c553b7ec042c563c540114b6b78bd4c6e808ee391a4a9d93e127032983c5b3708037b14aa604cfb034e7c8b0ffdd6936446fe80216178506a87402653a373926eeff66e704daf992a0a9a5c3ad80566c0339be9e5b8e35b3b3226b2f7767e20d992ea6c3d6e322eca37b0c7f7e60060802f5abcc1975841365cadbdc3867063addfc803766ae525375ecddee61f9df9ffcd20343c83ab82b0e91de039c59cb435c8d3159cc338b4901f40c9b5c27043bcf2bd5fa9b685b65c9ba5a1e11a51dd3f773051560341f9ec81d05bf259e2d4b7161f896fbb6812cfc924a32120b7367d5e40439e267adda6a1315bb0d6200ce6a503174c8d2a638ea6fd6b1f486d68db11bdca63c4f4a725d1ab6231ea875484e70b27d293c05803386924f283d4c12bb953474d92b7dd43d2d97193bd96281ebb63fa075d2f9ecd310c70ee1d97b5330bd8fb5791c5943ecf084e5f2c83915acac57519c46b166136068d6f9ec0dd598616e32c591128ce13705a283ca39d5b211409600e07b3713113374d9700207a45394eac5b3b7afc9b1b2bad7d89fd3f35f6b2413ce615ee7869b3569009403b96fdacdb32ef0a7e5229e2b666d51e95bdfb009b892e88bde70621a9b6509f068781392df4bdbc5723bb15071993f0d9a11575af5ff6ef85eaea39bc86805b35d8beee91b779354147f2d85304b8b49d053e7444fdd3deb9d16de331f2552af5b3be7766bb8f3f6a78c62148efb231f2268", find_value(obj, "solution").get_str());
}

TEST(rpc, CheckExperimentalDisabledHelpMsg) {

    EXPECT_EQ(experimentalDisabledHelpMsg("somerpc", {"somevalue"}),
        "\nWARNING: somerpc is disabled.\n"
        "To enable it, restart " DAEMON_NAME " with the following command line options:\n"
        "-experimentalfeatures and -somevalue\n\n"
        "Alternatively add these two lines to the zcash.conf file:\n\n"
        "experimentalfeatures=1\n"
        "somevalue=1\n");

    EXPECT_EQ(experimentalDisabledHelpMsg("somerpc", {"somevalue", "someothervalue"}),
        "\nWARNING: somerpc is disabled.\n"
        "To enable it, restart " DAEMON_NAME " with the following command line options:\n"
        "-experimentalfeatures and -somevalue"
        " or:\n-experimentalfeatures and -someothervalue"
        "\n\n"
        "Alternatively add these two lines to the zcash.conf file:\n\n"
        "experimentalfeatures=1\n"
        "somevalue=1\n"
        "\nor:\n\n"
        "experimentalfeatures=1\n"
        "someothervalue=1\n");

    EXPECT_EQ(experimentalDisabledHelpMsg("somerpc", {"somevalue", "someothervalue", "athirdvalue"}),
        "\nWARNING: somerpc is disabled.\n"
        "To enable it, restart " DAEMON_NAME " with the following command line options:\n"
        "-experimentalfeatures and -somevalue"
        " or:\n-experimentalfeatures and -someothervalue"
        " or:\n-experimentalfeatures and -athirdvalue"
         "\n\n"
        "Alternatively add these two lines to the zcash.conf file:\n\n"
        "experimentalfeatures=1\n"
        "somevalue=1\n"
        "\nor:\n\n"
        "experimentalfeatures=1\n"
        "someothervalue=1\n"
        "\nor:\n\n"
        "experimentalfeatures=1\n"
        "athirdvalue=1\n");
}

TEST(rpc, ParseHeightArg) {
    EXPECT_EQ(parseHeightArg("15", 21), 15);
    EXPECT_EQ(parseHeightArg("21", 21), 21);
    ASSERT_THROW(parseHeightArg("22", 21), UniValue);
    EXPECT_EQ(parseHeightArg("0", 21), 0);
    EXPECT_EQ(parseHeightArg("011", 21), 11); // allowed and parsed as decimal, not octal

    // negative values count back from current height
    EXPECT_EQ(parseHeightArg("-1", 21), 21);
    EXPECT_EQ(parseHeightArg("-2", 21), 20);
    EXPECT_EQ(parseHeightArg("-22", 21), 0);
    ASSERT_THROW(parseHeightArg("-23", 21), UniValue);
    ASSERT_THROW(parseHeightArg("-0", 21), UniValue);

    // currentHeight zero
    EXPECT_EQ(parseHeightArg("0", 0), 0);
    EXPECT_EQ(parseHeightArg("-1", 0), 0);

    // maximum possible height, just beyond, far beyond
    EXPECT_EQ(parseHeightArg("2147483647", 2147483647), 2147483647);
    ASSERT_THROW(parseHeightArg("2147483648", 2147483647), UniValue);
    ASSERT_THROW(parseHeightArg("999999999999999999999999999999999999999", 21), UniValue);

    // disallowed characters and formats
    ASSERT_THROW(parseHeightArg("5.21", 21), UniValue);
    ASSERT_THROW(parseHeightArg("5.0", 21), UniValue);
    ASSERT_THROW(parseHeightArg("a21", 21), UniValue);
    ASSERT_THROW(parseHeightArg(" 21", 21), UniValue);
    ASSERT_THROW(parseHeightArg("21 ", 21), UniValue);
    ASSERT_THROW(parseHeightArg("21x", 21), UniValue);
    ASSERT_THROW(parseHeightArg("+21", 21), UniValue);
    ASSERT_THROW(parseHeightArg("0x15", 21), UniValue);
    ASSERT_THROW(parseHeightArg("-0", 21), UniValue);
    ASSERT_THROW(parseHeightArg("-01", 21), UniValue);
    ASSERT_THROW(parseHeightArg("-0x15", 21), UniValue);
    ASSERT_THROW(parseHeightArg("", 21), UniValue);
}
