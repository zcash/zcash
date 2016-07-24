// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bloom.h"

#include "base58.h"
#include "clientversion.h"
#include "key.h"
#include "merkleblock.h"
#include "random.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"
#include "test/test_bitcoin.h"

#include <vector>

#include <boost/test/unit_test.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

BOOST_FIXTURE_TEST_SUITE(bloom_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize)
{
    CBloomFilter filter(3, 0.01, 0, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE( filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(!filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")), "BloomFilter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")), "BloomFilter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("03614e9b050000000000000001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());

    BOOST_CHECK_MESSAGE( filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter doesn't contain just-inserted object!");
    filter.clear();
    BOOST_CHECK_MESSAGE( !filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter should be empty!");
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_serialize_with_tweak)
{
    // Same test as bloom_create_insert_serialize, but we add a nTweak of 100
    CBloomFilter filter(3, 0.01, 2147483649UL, BLOOM_UPDATE_ALL);

    filter.insert(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8"));
    BOOST_CHECK_MESSAGE( filter.contains(ParseHex("99108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter doesn't contain just-inserted object!");
    // One bit different in first byte
    BOOST_CHECK_MESSAGE(!filter.contains(ParseHex("19108ad8ed9bb6274d3980bab5a85c048f0950c8")), "BloomFilter contains something it shouldn't!");

    filter.insert(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee"));
    BOOST_CHECK_MESSAGE(filter.contains(ParseHex("b5a2c786d9ef4658287ced5914b37a1b4aa32eee")), "BloomFilter doesn't contain just-inserted object (2)!");

    filter.insert(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5"));
    BOOST_CHECK_MESSAGE(filter.contains(ParseHex("b9300670b4c5366e95b2699e8b18bc75e5f729c5")), "BloomFilter doesn't contain just-inserted object (3)!");

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("03ce4299050000000100008001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_create_insert_key)
{
    string strSecret = string("5Kg1gnAjaLfKiwhhPpGS3QfRg2m6awQvaj98JCZBZQ5SuS2F15C");
    CBitcoinSecret vchSecret;
    BOOST_CHECK(vchSecret.SetString(strSecret));

    CKey key = vchSecret.GetKey();
    CPubKey pubkey = key.GetPubKey();
    vector<unsigned char> vchPubKey(pubkey.begin(), pubkey.end());

    CBloomFilter filter(2, 0.001, 0, BLOOM_UPDATE_ALL);
    filter.insert(vchPubKey);
    uint160 hash = pubkey.GetID();
    filter.insert(vector<unsigned char>(hash.begin(), hash.end()));

    CDataStream stream(SER_NETWORK, PROTOCOL_VERSION);
    filter.Serialize(stream, SER_NETWORK, PROTOCOL_VERSION);

    vector<unsigned char> vch = ParseHex("038fc16b080000000000000001");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    BOOST_CHECK_EQUAL_COLLECTIONS(stream.begin(), stream.end(), expected.begin(), expected.end());
}

BOOST_AUTO_TEST_CASE(bloom_match)
{
    // Random zcash transaction (dd5ffaa97eb9327b04fa1a73cf2af28e81527b6ad612d728840a7ae3c1093803)
    CTransaction tx;
    CDataStream stream(ParseHex("0100000001d49715970a6b5c491f1d874011973df322a27453e2fdb5a507829893fce22a69000000006a47304402201cfc09e6cfd278289e7312494dfe08be0d18346dc4283e8a78f18a6d8a124556022014042b86888110791bfd95d67c0a88986f2ad24f892d8088f82bc87b88d3bdb1012103a990a2a91a478d867e09e0ce9d48df3e3220423e057e6442dacf7bce14bb29dfffffffff01f0490200000000001976a9144816c219a422243b154337c56562f07ad6ff404d88ac00000000"), SER_DISK, CLIENT_VERSION);
    stream >> tx;

    // and one which spends it (071921cd01ec92e2d8e4aaf6bbf395858576b4bd979da9f88a521d1e619d8c8f)
    CDataStream spendStream(ParseHex("0100000001033809c1e37a0a8428d712d66a7b52818ef22acf731afa047b32b97ea9fa5fdd000000006b483045022100da3bce63d1c8904bad436f06109ab90a1380c560d21fb2298bb3802d8d3f1206022063c5581894818a5a56a22292fc81efc168eeccd69f5642de612b071f129f90f50121020cdf9b096dc2cd4718a05a3c1fa4d317dff2ab6837e0c2f6fd3988fd84b95524ffffffff01f0490200000000001976a9144816c219a422243b154337c56562f07ad6ff404d88ac00000000"), SER_DISK, CLIENT_VERSION);

    //unsigned char ch[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65, 0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8, 0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00, 0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77, 0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f, 0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2, 0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e, 0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4, 0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2, 0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a, 0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34, 0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97, 0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b, 0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f, 0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef, 0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39, 0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93, 0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43, 0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    //vector<unsigned char> vch(ch, ch + sizeof(ch) -1);
    //CDataStream spendStream(vch, SER_DISK, CLIENT_VERSION);

    CTransaction spendingTx;
    spendStream >> spendingTx;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S("0xdd5ffaa97eb9327b04fa1a73cf2af28e81527b6ad612d728840a7ae3c1093803"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // byte-reversed tx hash
    filter.insert(ParseHex("033809c1e37a0a8428d712d66a7b52818ef22acf731afa047b32b97ea9fa5fdd"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match manually serialized tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("304402201cfc09e6cfd278289e7312494dfe08be0d18346dc4283e8a78f18a6d8a124556022014042b86888110791bfd95d67c0a88986f2ad24f892d8088f82bc87b88d3bdb101"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match input signature");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("03a990a2a91a478d867e09e0ce9d48df3e3220423e057e6442dacf7bce14bb29df"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match input pub key");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("4816c219a422243b154337c56562f07ad6ff404d"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match output address");
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(spendingTx), "Simple Bloom filter didn't add output");

    // Need a second output for this.
    /*
    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("a266436d2965547608b9e15d9032a7b9d64fa431"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match output address");
    */

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0x692ae2fc93988207a5b5fde25374a222f33d971140871d1f495c6b0a971597d4"), 0));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    COutPoint prevOutPoint(uint256S("0x692ae2fc93988207a5b5fde25374a222f33d971140871d1f495c6b0a971597d4"), 0);
    {
        vector<unsigned char> data(32 + sizeof(unsigned int));
        memcpy(&data[0], prevOutPoint.hash.begin(), 32);
        memcpy(&data[32], &prevOutPoint.n, sizeof(unsigned int));
        filter.insert(data);
    }
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match manually serialized COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S("00000009e784f32f62ef849763d4f45b98e07ba658647343b915ff832b110436"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx), "Simple Bloom filter matched random tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("0000006d2965547608b9e15d9032a7b9d64fa431"));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx), "Simple Bloom filter matched random address");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0x90c122d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"), 1));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx), "Simple Bloom filter matched COutPoint for an output we didn't care about");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0x000000d70786e899529d71dbeba91ba216982fb6ba58f3bdaab65e73b7e9260b"), 0));
    BOOST_CHECK_MESSAGE(!filter.IsRelevantAndUpdate(tx), "Simple Bloom filter matched COutPoint for an output we didn't care about");
}

BOOST_AUTO_TEST_CASE(merkle_block_1)
{
    // Random zcash block (0a18aa311b3c81284754677e66e3a78e5390680e54e4d9c1a716a74bf7bdd850)
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000088804e46ac93ae538cc4977aa65752be37e5af84cffdbab6d329b907622d324afd2edd33b94f6d28167d53a815af40448150897061caa5ebd85074525a9c2c3100000000000000000000000000000000000000000000000000000000000000006b019457ffff7f200000bfc18df0babc36d4e1d6ef8a6e343c15c3bec1b7b936440f839d99d1000008cf940200d0e6730013b2580060577f00f3c74a0041951001e43707015a1f1b010601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff035e0101ffffffff0283c70a0000000000232102ffa90cf88320e4cbaa282506f6b2c1df39b0fabdcd0ceb685979bb6345acfd95ac98ab02000000000017a9146708e6670db0b950dac68031025cc5b63213a49187000000000100000001a2d8e5984af5f902ce8ee42a431f2211cd92e0347eb8304794bb29a95385eeab000000004847304402200b8b7d29ac534c83d57771215b1af572a96b41283ced1a2a8c3b7258a5687c7c02202e989869b11085745e04d5a23bdcca5e02ef6263270f5b8c55e400a10fcc31ed01feffffff02602c0500000000001976a91482d75bc161b2230c6fbbeb586d10c66c3f962fc288ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac0200000001000000015339144fde1f417adeabc7d3e8fcf01bee9cc5da5fe7a5f80a93fdd1b69393430000000049483045022100cb6d1d202701d5a13a896048731426f7bddc7c03e85009dc6533709bdbf6e339022056fa5c9cfb60cb5c55af6c90583e079049545c7ec082452b69b7df6bdd322e3501feffffff02b0ef0500000000001976a91457db1edd2585f4140bbcd0d9e27821f04fcca4b688ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac020000000100000001f503717810123e7f40a6ab13c7866102d137fc2b077dfca39ff61f3879e37ceb0000000049483045022100a4063874bb0ae4710c75ab45d437cc30894bfc89c7bb2ad0cc7739785d25879d02204783c8a6b99f50134539697a7ce86ed80a9f2ea2975c5386e66154b2a6c0a55401feffffff0210270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac10690400000000001976a914e50a70d55a860cb7e73f7584cff7875ed620919a88ac020000000100000001b077d12d457292c4d8b2e0e853eea6eb19eba20984e0cffc8441ba78111f9b89000000004847304402205ef1baaa781496b89d34aaae4c98b7e36f3235fc91e7e94481ce1b401eae758702200ae0b18264154229fc8d0a63df6737f1be63fe8b50401394f03711bf92d5ea3201feffffff0205b30600000000001976a914aa3f898ea469eb2fc2e404ed145dc53cc586ce9e88ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac02000000010000000399a7a65dcc321f2586b61d091b663cd68042804b583540924591ae20f4f26cd9000000006a47304402202508bd6e4d2e0c93275beffa034b55f0aabde470147606cedb794534eac4a3c80220098a202bd0863ed621abe251f31b26d01cb5c652d7da51c4ed582f5ff13d0458012103fb660ee1d5341e4e908d8d779af7a5afd05906142cd5f84ff30fc34264408b2dfeffffff1122a4ff1174c6d3e022dd56a0940a2a937bd90493a7b4f2711f75c9a6ecdade000000006a473044022002d71379dd96afb965ae88992b5b809c08805a1235e9ce25737131f08b1f81160220316b075e4f137729023e81b54278baa6cfb3c2dd327e8fff763c255787b347a9012102fd61e686c93ef2881c60b9635d53559445de6ef6cb6ba2f91a7ac11ab6eaff87feffffff21d123d2ea376cd4a671678145e34cf1a3322bd569866d59ca0edf425da0d1cc010000006b483045022100ae2709f53a71a7be9acb46bd2571352bfecf33ddbb0796b87c13884038fed5e20220784b8895c3a3c9784a6184a323f6324d6997e9cfae907bcdd3377a73334cb3370121037c0af65516ad1cd82ff72e3582aea350d98844713ed4088cad27c67ed98a73b7feffffff0210270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ace8530f00000000001976a9142306e76c457f7ba143eb0ecc7dbeb2d493f4b5d088ac02000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S("0xa562d78f7a20d11e43fe5eabbcf90a25b60f28cd59363c4250ad3a9024057c9e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xa562d78f7a20d11e43fe5eabbcf90a25b60f28cd59363c4250ad3a9024057c9e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 5);
    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Also match the 2nd last
    filter.insert(uint256S("0x028ab77cd33e7bbd91a7119d755bc7c151f8fb3cd307066fde28a87dc669fa4f"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0x028ab77cd33e7bbd91a7119d755bc7c151f8fb3cd307066fde28a87dc669fa4f"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 4);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}


BOOST_AUTO_TEST_CASE(merkle_block_2)
{
    // Random zcash block (7d7f21eed27f0cef570542da3f3221c9e3ce5378539b8a241a3b627ae87112f1)
    // With 5 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000086fc337f4d96ad6fe26922993aff7f460610f023947fb1524a9796c6039e336e9e3f50e8efd148368f28751b175acc0a2f84e90d95804b0240ed77805208d7140000000000000000000000000000000000000000000000000000000000000000085f9457ffff7f200000eeab3b01a349c2321e62021d3127a7453012509b43b617b84d8ade01000008ad2d65008026b20155667d00e6102901b0ff6800ff82f10088cc9500c51eff010501000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0502a0000101ffffffff02e1e3c100000000002321032b5658ac8bc397cb7eec32bda5e0126b07237ad06416c4b0c26515d200f271abac80841e000000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000010000000219e8b9f1338a730a196b90c9cade66f30da41ee3b887216ca938debc1e4ab49f0000000048473044022027471af3b10251d4246e0d2835a06e4db14cc0395d0b366f520401768fd959a902203cbf3bcfe6223a475fe768eec6f4e799c37c0b68eecde417a1c52d0df25211e901feffffff7586bc0810ba8f280ebfd28331cbbc9d0340f1e0843d591c85434355aedf5af60000000049483045022100e74bb84b3ff5173db4ac0e1bfd662585a29015e30939ef702bcc1b78e585e46102202dfa012c71c1f57a6d8abf95f028b565523fc781435bc157157fc211e1948dae01feffffff0280969800000000001976a914c580c53320dea648a03a97593d8a1c808da144e688ac39530f00000000001976a9144e208bf1ea539cfc42abba2ed9ed471dd37273ce88ac950000000100000002bb040278a4d7db142130f76cad84a73c92acbf4d09b14a1b4b4a20e3f03813ff0000000049483045022100a26a7b988da9ec27c5296da2284d98caf000b4eac1228755020fb081fb087520022046d3afe1ac0e272a2dc471a4c428330889dd2105a0b0522eb048e336fd15e1f101feffffff0c142569ea608b72f23b0865c631fdc7e1f121934a2c4ae4d38927922c162089010000006b483045022100ddc61e56211a99738b4d6f5a23b1b6e5e3cbea61c2f4450aa805fcb89f8af79502200802e6b51b56624942edf13a5e1acf37e670d7bd91de2b2ee02b4494c9cce0a5012103a3097a1563701a2e7f49986980d7e771837bae6500776c555916e9251fe30bd3feffffff02d8590f00000000001976a914a4456fc397881ed5f12dea3fd0dea651320f49d388ac00093d00000000001976a914c580c53320dea648a03a97593d8a1c808da144e688ac950000000100000003669fb2f8f1bd9108db56b69ac744c5c99c4bc2007450f0d583f62b547d0dba7e0000000048473044022072fdbcd8824d8b24c3f28a2a47d041e206bf8d04ba87b0cda501961dcbfc1de7022076ec3a543b2c2577e91ff575353d29203eeaafd5acd5563f4782e80494da75d301feffffff810f324b04fc23388ea298b07333eb61c332ac18ff956930c7159d2f1f3e7e71000000006a4730440220686bb245a82628d546f4cd707080106f49344aeda007f579fc4d65f39ffa9478022015ebda540b1cc62859fb388c5ee886c072ef0b2bcacfca9cdf59f66994e3e03d012103f795e1424eab94769f4652dd54ab14d7a8e404620c0e8b66586dde70485f723bfeffffff0c142569ea608b72f23b0865c631fdc7e1f121934a2c4ae4d38927922c162089000000006b483045022100f6de5dd27bba83f56a47f92aa196899bee0d8bd64198c5b4e5ff28bb410fb2d002204058b435554da16a7a9551ed3186e6eb5322b24886435aa75f50f91735e7ee60012102ce64b6712044dae5a45b4167a28a746ba2053b300ad1eae2f873a7b93ba4417efeffffff02404b4c00000000001976a914c580c53320dea648a03a97593d8a1c808da144e688acfc4e0f00000000001976a91471738f751839f99768c4a2e9c689acd9b184225488ac950000000100000001880a0b5d9e485fee5e457c41451e7fc9ca947bd1a8f5cc92f94ac6a42aa1c178000000006b483045022100c0345a7fdeabdfa6178b4ffff027c50c9ae8ba6ee13f70889d36063e863a326302202c7bc4146cf2b4d6f225db5ad2cf6bafeac04f4d121cf2428318d3795890dffb012102a7c4ed2842941f56e666820b4fd2db00ad04c2336c4e22d89c9c6a632cc10721ffffffff01e0930400000000001976a91472dd7e185c3f069223560a1df310c81d4d12804c88ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the first transaction
    filter.insert(uint256S("0xbadc26343ec0d00d1106e5715a15aa10d4000c2211dca8cb76db51560302367e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xbadc26343ec0d00d1106e5715a15aa10d4000c2211dca8cb76db51560302367e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Match an output from the second transaction (the pubkey for address myXFfxnnizWcNmgeA7iccxZgCcoL2g9RjC)
    // It also matches the third transaction, which spends to the pubkey again
    // It also matches the fourth transaction, which spends to the pubkey again
    // This should match the last transaction because it spends the output matched
    filter.insert(ParseHex("c580c53320dea648a03a97593d8a1c808da144e6"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 5);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second == uint256S("0x9042d2c9222e7b6ee8901950ae2fd4b2be10a9914f282bb6b01c98bc66630dae"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second == uint256S("0x3a691704797375f98ea0c48ed60b8c4c8e616e4031ca43a2300df105ef47dee3"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[3].second == uint256S("0x78c1a12aa4c64af992ccf5a8d17b94cac97f1e45417c455eee5f489e5d0b0a88"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[3].first == 3);

    BOOST_CHECK(merkleBlock.vMatchedTxn[4].second == uint256S("0x55a46dbe172f7a453caf98bcc144d732e62b2a2f91615cd21e0571491e13ade6"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[4].first == 4);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}

BOOST_AUTO_TEST_CASE(merkle_block_2_with_update_none)
{
    // Random zcash block (7d7f21eed27f0cef570542da3f3221c9e3ce5378539b8a241a3b627ae87112f1)
    // With 5 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000086fc337f4d96ad6fe26922993aff7f460610f023947fb1524a9796c6039e336e9e3f50e8efd148368f28751b175acc0a2f84e90d95804b0240ed77805208d7140000000000000000000000000000000000000000000000000000000000000000085f9457ffff7f200000eeab3b01a349c2321e62021d3127a7453012509b43b617b84d8ade01000008ad2d65008026b20155667d00e6102901b0ff6800ff82f10088cc9500c51eff010501000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0502a0000101ffffffff02e1e3c100000000002321032b5658ac8bc397cb7eec32bda5e0126b07237ad06416c4b0c26515d200f271abac80841e000000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000010000000219e8b9f1338a730a196b90c9cade66f30da41ee3b887216ca938debc1e4ab49f0000000048473044022027471af3b10251d4246e0d2835a06e4db14cc0395d0b366f520401768fd959a902203cbf3bcfe6223a475fe768eec6f4e799c37c0b68eecde417a1c52d0df25211e901feffffff7586bc0810ba8f280ebfd28331cbbc9d0340f1e0843d591c85434355aedf5af60000000049483045022100e74bb84b3ff5173db4ac0e1bfd662585a29015e30939ef702bcc1b78e585e46102202dfa012c71c1f57a6d8abf95f028b565523fc781435bc157157fc211e1948dae01feffffff0280969800000000001976a914c580c53320dea648a03a97593d8a1c808da144e688ac39530f00000000001976a9144e208bf1ea539cfc42abba2ed9ed471dd37273ce88ac950000000100000002bb040278a4d7db142130f76cad84a73c92acbf4d09b14a1b4b4a20e3f03813ff0000000049483045022100a26a7b988da9ec27c5296da2284d98caf000b4eac1228755020fb081fb087520022046d3afe1ac0e272a2dc471a4c428330889dd2105a0b0522eb048e336fd15e1f101feffffff0c142569ea608b72f23b0865c631fdc7e1f121934a2c4ae4d38927922c162089010000006b483045022100ddc61e56211a99738b4d6f5a23b1b6e5e3cbea61c2f4450aa805fcb89f8af79502200802e6b51b56624942edf13a5e1acf37e670d7bd91de2b2ee02b4494c9cce0a5012103a3097a1563701a2e7f49986980d7e771837bae6500776c555916e9251fe30bd3feffffff02d8590f00000000001976a914a4456fc397881ed5f12dea3fd0dea651320f49d388ac00093d00000000001976a914c580c53320dea648a03a97593d8a1c808da144e688ac950000000100000003669fb2f8f1bd9108db56b69ac744c5c99c4bc2007450f0d583f62b547d0dba7e0000000048473044022072fdbcd8824d8b24c3f28a2a47d041e206bf8d04ba87b0cda501961dcbfc1de7022076ec3a543b2c2577e91ff575353d29203eeaafd5acd5563f4782e80494da75d301feffffff810f324b04fc23388ea298b07333eb61c332ac18ff956930c7159d2f1f3e7e71000000006a4730440220686bb245a82628d546f4cd707080106f49344aeda007f579fc4d65f39ffa9478022015ebda540b1cc62859fb388c5ee886c072ef0b2bcacfca9cdf59f66994e3e03d012103f795e1424eab94769f4652dd54ab14d7a8e404620c0e8b66586dde70485f723bfeffffff0c142569ea608b72f23b0865c631fdc7e1f121934a2c4ae4d38927922c162089000000006b483045022100f6de5dd27bba83f56a47f92aa196899bee0d8bd64198c5b4e5ff28bb410fb2d002204058b435554da16a7a9551ed3186e6eb5322b24886435aa75f50f91735e7ee60012102ce64b6712044dae5a45b4167a28a746ba2053b300ad1eae2f873a7b93ba4417efeffffff02404b4c00000000001976a914c580c53320dea648a03a97593d8a1c808da144e688acfc4e0f00000000001976a91471738f751839f99768c4a2e9c689acd9b184225488ac950000000100000001880a0b5d9e485fee5e457c41451e7fc9ca947bd1a8f5cc92f94ac6a42aa1c178000000006b483045022100c0345a7fdeabdfa6178b4ffff027c50c9ae8ba6ee13f70889d36063e863a326302202c7bc4146cf2b4d6f225db5ad2cf6bafeac04f4d121cf2428318d3795890dffb012102a7c4ed2842941f56e666820b4fd2db00ad04c2336c4e22d89c9c6a632cc10721ffffffff01e0930400000000001976a91472dd7e185c3f069223560a1df310c81d4d12804c88ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the first transaction
    filter.insert(uint256S("0xbadc26343ec0d00d1106e5715a15aa10d4000c2211dca8cb76db51560302367e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xbadc26343ec0d00d1106e5715a15aa10d4000c2211dca8cb76db51560302367e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Match an output from the second transaction (the pubkey for address myXFfxnnizWcNmgeA7iccxZgCcoL2g9RjC)
    // It will match the third transaction, which has another pay-to-pubkey output to the same address
    // It will match the fourth transaction, which has another pay-to-pubkey output to the same address
    // This should not match the last transaction though it spends the output matched
    filter.insert(ParseHex("c580c53320dea648a03a97593d8a1c808da144e6"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 4);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second == uint256S("0x9042d2c9222e7b6ee8901950ae2fd4b2be10a9914f282bb6b01c98bc66630dae"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second == uint256S("0x3a691704797375f98ea0c48ed60b8c4c8e616e4031ca43a2300df105ef47dee3"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[3].second == uint256S("0x78c1a12aa4c64af992ccf5a8d17b94cac97f1e45417c455eee5f489e5d0b0a88"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[3].first == 3);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}


BOOST_AUTO_TEST_CASE(merkle_block_3_and_serialize)
{
    // Random zcash block (149e54e5b639d1be22337e2176311ef37f487fcfb12036155407aaaa710b8104)
    // With one tx
    CBlock block;
    CDataStream stream(ParseHex("04000000443e8135b55929f59ff07da32013a08cb71e2f33e0538183ac1956c52124183c2a77a9bad5cc4a5ae1866799d1e148bf9196f25abfbf1758fb242f9cfe6a018e0000000000000000000000000000000000000000000000000000000000000000b45a94574b716f200000a78cee268a82089ff84d8769980442c6335768c5177e3d57f19e0527000008fa9220006d111601601cae01f546da01834874003649fa0124abdb00fd3366010101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff050296000101ffffffff02e07072000000000023210304fe1f36dbe31372a1d9b711a9ad8c9713b3b3e66ca9d4815f16d876a243a0f3ac389c1c000000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the only transaction
    filter.insert(uint256S("0x8e016afe9c2f24fb5817bfbf5af29691bf48e1d1996786e15a4accd5baa9772a"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0x8e016afe9c2f24fb5817bfbf5af29691bf48e1d1996786e15a4accd5baa9772a"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    CDataStream merkleStream(SER_NETWORK, PROTOCOL_VERSION);
    merkleStream << merkleBlock;
    vector<unsigned char> vch = ParseHex("04000000443e8135b55929f59ff07da32013a08cb71e2f33e0538183ac1956c52124183c2a77a9bad5cc4a5ae1866799d1e148bf9196f25abfbf1758fb242f9cfe6a018e0000000000000000000000000000000000000000000000000000000000000000b45a94574b716f200000a78cee268a82089ff84d8769980442c6335768c5177e3d57f19e0527000008fa9220006d111601601cae01f546da01834874003649fa0124abdb00fd33660101000000012a77a9bad5cc4a5ae1866799d1e148bf9196f25abfbf1758fb242f9cfe6a018e0101");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

//    std::cout << HexStr(merkleStream.begin(), merkleStream.end()) << std::endl;

    BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), merkleStream.begin(), merkleStream.end());
}


BOOST_AUTO_TEST_CASE(merkle_block_4)
{
    // Random zcash block (0a18aa311b3c81284754677e66e3a78e5390680e54e4d9c1a716a74bf7bdd850)
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000088804e46ac93ae538cc4977aa65752be37e5af84cffdbab6d329b907622d324afd2edd33b94f6d28167d53a815af40448150897061caa5ebd85074525a9c2c3100000000000000000000000000000000000000000000000000000000000000006b019457ffff7f200000bfc18df0babc36d4e1d6ef8a6e343c15c3bec1b7b936440f839d99d1000008cf940200d0e6730013b2580060577f00f3c74a0041951001e43707015a1f1b010601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff035e0101ffffffff0283c70a0000000000232102ffa90cf88320e4cbaa282506f6b2c1df39b0fabdcd0ceb685979bb6345acfd95ac98ab02000000000017a9146708e6670db0b950dac68031025cc5b63213a49187000000000100000001a2d8e5984af5f902ce8ee42a431f2211cd92e0347eb8304794bb29a95385eeab000000004847304402200b8b7d29ac534c83d57771215b1af572a96b41283ced1a2a8c3b7258a5687c7c02202e989869b11085745e04d5a23bdcca5e02ef6263270f5b8c55e400a10fcc31ed01feffffff02602c0500000000001976a91482d75bc161b2230c6fbbeb586d10c66c3f962fc288ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac0200000001000000015339144fde1f417adeabc7d3e8fcf01bee9cc5da5fe7a5f80a93fdd1b69393430000000049483045022100cb6d1d202701d5a13a896048731426f7bddc7c03e85009dc6533709bdbf6e339022056fa5c9cfb60cb5c55af6c90583e079049545c7ec082452b69b7df6bdd322e3501feffffff02b0ef0500000000001976a91457db1edd2585f4140bbcd0d9e27821f04fcca4b688ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac020000000100000001f503717810123e7f40a6ab13c7866102d137fc2b077dfca39ff61f3879e37ceb0000000049483045022100a4063874bb0ae4710c75ab45d437cc30894bfc89c7bb2ad0cc7739785d25879d02204783c8a6b99f50134539697a7ce86ed80a9f2ea2975c5386e66154b2a6c0a55401feffffff0210270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac10690400000000001976a914e50a70d55a860cb7e73f7584cff7875ed620919a88ac020000000100000001b077d12d457292c4d8b2e0e853eea6eb19eba20984e0cffc8441ba78111f9b89000000004847304402205ef1baaa781496b89d34aaae4c98b7e36f3235fc91e7e94481ce1b401eae758702200ae0b18264154229fc8d0a63df6737f1be63fe8b50401394f03711bf92d5ea3201feffffff0205b30600000000001976a914aa3f898ea469eb2fc2e404ed145dc53cc586ce9e88ac10270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ac02000000010000000399a7a65dcc321f2586b61d091b663cd68042804b583540924591ae20f4f26cd9000000006a47304402202508bd6e4d2e0c93275beffa034b55f0aabde470147606cedb794534eac4a3c80220098a202bd0863ed621abe251f31b26d01cb5c652d7da51c4ed582f5ff13d0458012103fb660ee1d5341e4e908d8d779af7a5afd05906142cd5f84ff30fc34264408b2dfeffffff1122a4ff1174c6d3e022dd56a0940a2a937bd90493a7b4f2711f75c9a6ecdade000000006a473044022002d71379dd96afb965ae88992b5b809c08805a1235e9ce25737131f08b1f81160220316b075e4f137729023e81b54278baa6cfb3c2dd327e8fff763c255787b347a9012102fd61e686c93ef2881c60b9635d53559445de6ef6cb6ba2f91a7ac11ab6eaff87feffffff21d123d2ea376cd4a671678145e34cf1a3322bd569866d59ca0edf425da0d1cc010000006b483045022100ae2709f53a71a7be9acb46bd2571352bfecf33ddbb0796b87c13884038fed5e20220784b8895c3a3c9784a6184a323f6324d6997e9cfae907bcdd3377a73334cb3370121037c0af65516ad1cd82ff72e3582aea350d98844713ed4088cad27c67ed98a73b7feffffff0210270000000000001976a9142b891e223001d0d68bdbcc9d339057aaaa86f92188ace8530f00000000001976a9142306e76c457f7ba143eb0ecc7dbeb2d493f4b5d088ac02000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S("0xa562d78f7a20d11e43fe5eabbcf90a25b60f28cd59363c4250ad3a9024057c9e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xa562d78f7a20d11e43fe5eabbcf90a25b60f28cd59363c4250ad3a9024057c9e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 5);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Also match the 4th transaction
    filter.insert(uint256S("0xccd1a05d42df0eca596d8669d52b32a3f14ce345816771a6d46c37ead223d121"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xccd1a05d42df0eca596d8669d52b32a3f14ce345816771a6d46c37ead223d121"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 3);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_p2pubkey_only)
{
    // Random zcash block (3c182421c55619ac838153e0332f1eb78ca01320a37df09ff52959b535813e44)
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("04000000b976a3322ee5e9d242dc0b465f5c27fdcf5e292789694ed46531e41e509257059dcff7d722ef68818ac8238e312071dfcc7dc004bcb808f1196c9ba9e32fed450000000000000000000000000000000000000000000000000000000000000000675a94575b297620010080cd55554a4c899735a7d13984e7b20959398ffc253a3e540d2c0645000008d69f170034ebec002551500179097601c8363900b62f4d00f976870072b012010601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff050295000101ffffffff0280cc710000000000232103c5da583f3ee0da22004d314b45e6be77cace71778644260032e11bf981db2d42ac646b1c000000000017a9146708e6670db0b950dac68031025cc5b63213a491870000000001000000024ffa69c67da828de6f0607d33cfbf851c1c75b759d11a791bd7b3ed37cb78a02000000006a47304402204a1a53914484608f77a6b2a8263321df466585cc7b7fe83efb020a756e3de67402200c3f8f4067b7a7e4bd69e3e470c863c1cccde825dc95f065bc64fd4f0fd57770012102e1234490210e241ff92318a602b03c9f96e61e48cefc308d09ce8e4d89aa826efeffffff6b2b1e73383909bb5166fd7480c6e320510a81d7314edb72cbc5c548fb35439e000000004847304402200324ce69f57ad21500c4e63ddb51e2bcbf8bb16c5dbf935292c52f19d72f8fcb02207542fd826f72803cf37fe7cbf3559bbc26291754141983ca3d8d47e31f8e4a3e01feffffff02e0930400000000001976a914b444970f859e4deba8942c7fc3fea7126b10497888ac76971000000000001976a914c471e3193347fa63e6e6654097aa16b341d76bca88ac890000000100000001388af1a41c9c99286e272124e32fbc5c1f4af7f55b9a9198dffb6dace9ec33ab00000000494830450221008d6e09b6edf96bbbe53a29527db12eede18f50edb5b7b561c9ab5d48ece3d22a02202c94705e6eeb22b2721ca6ed5876b49f01cd9be461805211d6421ee78f3a0d4b01feffffff02dd4a0f00000000001976a914f35f951a3bc9abb208046df56f34dfe08e23a0fa88ac801a0600000000001976a91472dd7e185c3f069223560a1df310c81d4d12804c88ac890000000100000002032108d0cabea40dccb01020f6e28b747ce9e02312301de9633b58deb022e039000000004847304402200cb4558d6ca6fae452e2972b464944f4c5c0db190e7c4e89c504b52fcbc3d83602203bb3a0e861dae27dbec077fe58aa25272303bb1df4f542b8dda3522bc6d763d901feffffff1edf71037dbc7f30406afe7a0bc1577dd4e9bf460c4c7bddf48ab1e6e855b08f010000006b483045022100af83a715b4a1d2a1d72736e4705b1bc2b3270fd77451db27478f851031416f6b02201dd22bde30b1ba3e7e31044a53ddcd8ed5d82f552dc547a3cc099debaae05bb4012102319fecfd88d7872bd1298222a0b45567da9ff3af29cf890ee60b2b10192bfaf5feffffff02a0860100000000001976a91472dd7e185c3f069223560a1df310c81d4d12804c88acac620f00000000001976a914c3da60b701d2e0b4d31566301c9c9f5071f01bc188ac89000000010000000125c2dae4318d08dd07a3cd3bed9c7de519e0eee0f210f630c2753208f234f51b0000000049483045022100e410502257a92db55a6bfbb7833aef74e9e48c1a4b02037ab837305c4476599b02207b0c33e54a717105a6e80003276a7f63dea9b7db2cef2578cb6b986d8e4d838401feffffff0270881100000000001976a91412517b5217b2c1700d92f9b8aad67453e24e3c5888aca0860100000000001976a914b444970f859e4deba8942c7fc3fea7126b10497888ac890000000100000002aa0a10864e11d225322564862705caf4cb6c30c928793278483a4f5f638779d0000000006b4830450221009530514326b8de4ba1aebd06b5f74903fb3550d846047677b3e882bc379cfa7602203b57f2bd869e7e1d7217b2139ffa8a491ed3c07d01c832389ce01ec99dbccf10012102ce64b6712044dae5a45b4167a28a746ba2053b300ad1eae2f873a7b93ba4417efeffffffc88a41b10c0e433a90d99b4435b8281deb830a6a25eb8dff8194e48910a39bf0010000006b483045022100d0de362b1b8d4e3d7005bd13f870d7cd5872a39bb2371c2f3b60df893147b827022030999f2355ce3f5da6be0dd2bb74b7521271e76e4182b91934a4b25ed6d03ac1012103e0a3402c211ee59bd3a3f5213db04ccda564276b607271e87f6dd58f697b65a7feffffff027f580f00000000001976a9145733181088dfc1f3ca8a77126a3beb06fc72dd5888ace0930400000000001976a91472dd7e185c3f069223560a1df310c81d4d12804c88ac89000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_P2PUBKEY_ONLY);
    // Match the generation pubkey
    filter.insert(ParseHex("03c5da583f3ee0da22004d314b45e6be77cace71778644260032e11bf981db2d42"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("72dd7e185c3f069223560a1df310c81d4d12804c"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We should match the generation outpoint
    BOOST_CHECK(filter.contains(COutPoint(uint256S("0xdebdc61ca196e4a84bb5eedf07600af651d8bf351b04b592243319286f064f80"), 0)));
    // ... but not the 4th transaction's output (its not pay-2-pubkey)
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("0x8920162c922789d3e44a2c4a9321f1e1c7fd31c665083bf2728b60ea6925140c"), 0)));
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_update_none)
{
    // Random real block (000000000000b731f2eef9e8c63173adfb07e41bd53eb0ef0a6b720d6cb6dea4)
    // With 7 txes
    CBlock block;
    CDataStream stream(ParseHex("0100000082bb869cf3a793432a66e826e05a6fc37469f8efb7421dc880670100000000007f16c5962e8bd963659c793ce370d95f093bc7e367117b3c30c1f8fdd0d9728776381b4d4c86041b000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000554b8529000701000000010000000000000000000000000000000000000000000000000000000000000000ffffffff07044c86041b0136ffffffff0100f2052a01000000434104eaafc2314def4ca98ac970241bcab022b9c1e1f4ea423a20f134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce13ad1357231a2252247d97a46a91ac000000000100000001bcad20a6a29827d1424f08989255120bf7f3e9e3cdaaa6bb31b0737fe048724300000000494830450220356e834b046cadc0f8ebb5a8a017b02de59c86305403dad52cd77b55af062ea10221009253cd6c119d4729b77c978e1e2aa19f5ea6e0e52b3f16e32fa608cd5bab753901ffffffff02008d380c010000001976a9142b4b8072ecbba129b6453c63e129e643207249ca88ac0065cd1d000000001976a9141b8dd13b994bcfc787b32aeadf58ccb3615cbd5488ac000000000100000003fdacf9b3eb077412e7a968d2e4f11b9a9dee312d666187ed77ee7d26af16cb0b000000008c493046022100ea1608e70911ca0de5af51ba57ad23b9a51db8d28f82c53563c56a05c20f5a87022100a8bdc8b4a8acc8634c6b420410150775eb7f2474f5615f7fccd65af30f310fbf01410465fdf49e29b06b9a1582287b6279014f834edc317695d125ef623c1cc3aaece245bd69fcad7508666e9c74a49dc9056d5fc14338ef38118dc4afae5fe2c585caffffffff309e1913634ecb50f3c4f83e96e70b2df071b497b8973a3e75429df397b5af83000000004948304502202bdb79c596a9ffc24e96f4386199aba386e9bc7b6071516e2b51dda942b3a1ed022100c53a857e76b724fc14d45311eac5019650d415c3abb5428f3aae16d8e69bec2301ffffffff2089e33491695080c9edc18a428f7d834db5b6d372df13ce2b1b0e0cbcb1e6c10000000049483045022100d4ce67c5896ee251c810ac1ff9ceccd328b497c8f553ab6e08431e7d40bad6b5022033119c0c2b7d792d31f1187779c7bd95aefd93d90a715586d73801d9b47471c601ffffffff0100714460030000001976a914c7b55141d097ea5df7a0ed330cf794376e53ec8d88ac0000000001000000045bf0e214aa4069a3e792ecee1e1bf0c1d397cde8dd08138f4b72a00681743447000000008b48304502200c45de8c4f3e2c1821f2fc878cba97b1e6f8807d94930713aa1c86a67b9bf1e40221008581abfef2e30f957815fc89978423746b2086375ca8ecf359c85c2a5b7c88ad01410462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffffffffd669f7d7958d40fc59d2253d88e0f248e29b599c80bbcec344a83dda5f9aa72c000000008a473044022078124c8beeaa825f9e0b30bff96e564dd859432f2d0cb3b72d3d5d93d38d7e930220691d233b6c0f995be5acb03d70a7f7a65b6bc9bdd426260f38a1346669507a3601410462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95fffffffff878af0d93f5229a68166cf051fd372bb7a537232946e0a46f53636b4dafdaa4000000008c493046022100c717d1714551663f69c3c5759bdbb3a0fcd3fab023abc0e522fe6440de35d8290221008d9cbe25bffc44af2b18e81c58eb37293fd7fe1c2e7b46fc37ee8c96c50ab1e201410462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffffffff27f2b668859cd7f2f894aa0fd2d9e60963bcd07c88973f425f999b8cbfd7a1e2000000008c493046022100e00847147cbf517bcc2f502f3ddc6d284358d102ed20d47a8aa788a62f0db780022100d17b2d6fa84dcaf1c95d88d7e7c30385aecf415588d749afd3ec81f6022cecd701410462bb73f76ca0994fcb8b4271e6fb7561f5c0f9ca0cf6485261c4a0dc894f4ab844c6cdfb97cd0b60ffb5018ffd6238f4d87270efb1d3ae37079b794a92d7ec95ffffffff0100c817a8040000001976a914b6efd80d99179f4f4ff6f4dd0a007d018c385d2188ac000000000100000001834537b2f1ce8ef9373a258e10545ce5a50b758df616cd4356e0032554ebd3c4000000008b483045022100e68f422dd7c34fdce11eeb4509ddae38201773dd62f284e8aa9d96f85099d0b002202243bd399ff96b649a0fad05fa759d6a882f0af8c90cf7632c2840c29070aec20141045e58067e815c2f464c6a2a15f987758374203895710c2d452442e28496ff38ba8f5fd901dc20e29e88477167fe4fc299bf818fd0d9e1632d467b2a3d9503b1aaffffffff0280d7e636030000001976a914f34c3e10eb387efe872acb614c89e78bfca7815d88ac404b4c00000000001976a914a84e272933aaf87e1715d7786c51dfaeb5b65a6f88ac00000000010000000143ac81c8e6f6ef307dfe17f3d906d999e23e0189fda838c5510d850927e03ae7000000008c4930460221009c87c344760a64cb8ae6685a3eec2c1ac1bed5b88c87de51acd0e124f266c16602210082d07c037359c3a257b5c63ebd90f5a5edf97b2ac1c434b08ca998839f346dd40141040ba7e521fa7946d12edbb1d1e95a15c34bd4398195e86433c92b431cd315f455fe30032ede69cad9d1e1ed6c3c4ec0dbfced53438c625462afb792dcb098544bffffffff0240420f00000000001976a9144676d1b820d63ec272f1900d59d43bc6463d96f888ac40420f00000000001976a914648d04341d00d7968b3405c034adc38d4d8fb9bd88ac00000000010000000248cc917501ea5c55f4a8d2009c0567c40cfe037c2e71af017d0a452ff705e3f1000000008b483045022100bf5fdc86dc5f08a5d5c8e43a8c9d5b1ed8c65562e280007b52b133021acd9acc02205e325d613e555f772802bf413d36ba807892ed1a690a77811d3033b3de226e0a01410429fa713b124484cb2bd7b5557b2c0b9df7b2b1fee61825eadc5ae6c37a9920d38bfccdc7dc3cb0c47d7b173dbc9db8d37db0a33ae487982c59c6f8606e9d1791ffffffff41ed70551dd7e841883ab8f0b16bf04176b7d1480e4f0af9f3d4c3595768d068000000008b4830450221008513ad65187b903aed1102d1d0c47688127658c51106753fed0151ce9c16b80902201432b9ebcb87bd04ceb2de66035fbbaf4bf8b00d1cfe41f1a1f7338f9ad79d210141049d4cf80125bf50be1709f718c07ad15d0fc612b7da1f5570dddc35f2a352f0f27c978b06820edca9ef982c35fda2d255afba340068c5035552368bc7200c1488ffffffff0100093d00000000001976a9148edb68822f1ad580b043c7b3df2e400f8699eb4888ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the generation pubkey
    filter.insert(ParseHex("04eaafc2314def4ca98ac970241bcab022b9c1e1f4ea423a20f134c876f2c01ec0f0dd5b2e86e7168cefe0d81113c3807420ce13ad1357231a2252247d97a46a91"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("b6efd80d99179f4f4ff6f4dd0a007d018c385d21"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We shouldn't match any outpoints (UPDATE_NONE)
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("0x147caa76786596590baa4e98f5d9f48b86c7765e489f7a6ff3360fe5c674360b"), 0)));
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("0x02981fa052f0481dbc5868f4fc2166035a10f27a03cfd2de67326471df5bc041"), 0)));
}

static std::vector<unsigned char> RandomData()
{
    uint256 r = GetRandHash();
    return std::vector<unsigned char>(r.begin(), r.end());
}

BOOST_AUTO_TEST_CASE(rolling_bloom)
{
    // last-100-entry, 1% false positive:
    CRollingBloomFilter rb1(100, 0.01);

    // Overfill:
    static const int DATASIZE=399;
    std::vector<unsigned char> data[DATASIZE];
    for (int i = 0; i < DATASIZE; i++) {
        data[i] = RandomData();
        rb1.insert(data[i]);
    }
    // Last 100 guaranteed to be remembered:
    for (int i = 299; i < DATASIZE; i++) {
        BOOST_CHECK(rb1.contains(data[i]));
    }

    // false positive rate is 1%, so we should get about 100 hits if
    // testing 10,000 random keys. We get worst-case false positive
    // behavior when the filter is as full as possible, which is
    // when we've inserted one minus an integer multiple of nElement*2.
    unsigned int nHits = 0;
    for (int i = 0; i < 10000; i++) {
        if (rb1.contains(RandomData()))
            ++nHits;
    }
    // Run test_bitcoin with --log_level=message to see BOOST_TEST_MESSAGEs:
    BOOST_TEST_MESSAGE("RollingBloomFilter got " << nHits << " false positives (~100 expected)");

    // Insanely unlikely to get a fp count outside this range:
    BOOST_CHECK(nHits > 25);
    BOOST_CHECK(nHits < 175);

    BOOST_CHECK(rb1.contains(data[DATASIZE-1]));
    rb1.reset();
    BOOST_CHECK(!rb1.contains(data[DATASIZE-1]));

    // Now roll through data, make sure last 100 entries
    // are always remembered:
    for (int i = 0; i < DATASIZE; i++) {
        if (i >= 100)
            BOOST_CHECK(rb1.contains(data[i-100]));
        rb1.insert(data[i]);
    }

    // Insert 999 more random entries:
    for (int i = 0; i < 999; i++) {
        rb1.insert(RandomData());
    }
    // Sanity check to make sure the filter isn't just filling up:
    nHits = 0;
    for (int i = 0; i < DATASIZE; i++) {
        if (rb1.contains(data[i]))
            ++nHits;
    }
    // Expect about 5 false positives, more than 100 means
    // something is definitely broken.
    BOOST_TEST_MESSAGE("RollingBloomFilter got " << nHits << " false positives (~5 expected)");
    BOOST_CHECK(nHits < 100);

    // last-1000-entry, 0.01% false positive:
    CRollingBloomFilter rb2(1000, 0.001);
    for (int i = 0; i < DATASIZE; i++) {
        rb2.insert(data[i]);
    }
    // ... room for all of them:
    for (int i = 0; i < DATASIZE; i++) {
        BOOST_CHECK(rb2.contains(data[i]));
    }
}

BOOST_AUTO_TEST_SUITE_END()
