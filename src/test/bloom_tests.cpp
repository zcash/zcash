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
    // Random zcash transaction (19cfa5db35f33a2e67fe1b9b738731b62e548d2f27c55a2f28523fec52b71525)
    CTransaction tx;
    CDataStream stream(ParseHex("01000000015ad78be5497476bbf84869d8156761ca850b6e82e48ad1315069a3726516a3d1010000006b483045022100ba5e90204e83c5f961b67c6232c1cc6c360afd36d43fcfae0de7af2e75f4cda7022012fec415a12048dbb70511fda6195b090b56735232281dc1144409833a092edc012102c322382e17c9ed4f47183f219cc5dd7853f939fb8eebae3c943622e0abf8d5e5feffffff0280969800000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988acd694693a000000001976a9145f0d00adba6489150808feb4108d7be582cbb2e188ac0a000000"), SER_DISK, CLIENT_VERSION);
    stream >> tx;

    // and one which spends it (1079621bf638e8bea9ee2f8d15287ae31a269969c360006d68d8ee07a7f532e6)
    CDataStream spendStream(ParseHex("01000000012515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19000000006b4830450221009e496c27d1ec174666da187815407c5b7b4d71ed8935e427a334df6d5d7c9e2c022007ca48eabad68cdec0a7d3be15d64a950bb3d80ac2b988fcd82394064a0a32a0012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cffffffff01808d5b00000000001976a9146d1c88970e614202031a4f2ba13846f8ce91019e88ac00000000"), SER_DISK, CLIENT_VERSION);

    //unsigned char ch[] = {0x01, 0x00, 0x00, 0x00, 0x01, 0x6b, 0xff, 0x7f, 0xcd, 0x4f, 0x85, 0x65, 0xef, 0x40, 0x6d, 0xd5, 0xd6, 0x3d, 0x4f, 0xf9, 0x4f, 0x31, 0x8f, 0xe8, 0x20, 0x27, 0xfd, 0x4d, 0xc4, 0x51, 0xb0, 0x44, 0x74, 0x01, 0x9f, 0x74, 0xb4, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x49, 0x30, 0x46, 0x02, 0x21, 0x00, 0xda, 0x0d, 0xc6, 0xae, 0xce, 0xfe, 0x1e, 0x06, 0xef, 0xdf, 0x05, 0x77, 0x37, 0x57, 0xde, 0xb1, 0x68, 0x82, 0x09, 0x30, 0xe3, 0xb0, 0xd0, 0x3f, 0x46, 0xf5, 0xfc, 0xf1, 0x50, 0xbf, 0x99, 0x0c, 0x02, 0x21, 0x00, 0xd2, 0x5b, 0x5c, 0x87, 0x04, 0x00, 0x76, 0xe4, 0xf2, 0x53, 0xf8, 0x26, 0x2e, 0x76, 0x3e, 0x2d, 0xd5, 0x1e, 0x7f, 0xf0, 0xbe, 0x15, 0x77, 0x27, 0xc4, 0xbc, 0x42, 0x80, 0x7f, 0x17, 0xbd, 0x39, 0x01, 0x41, 0x04, 0xe6, 0xc2, 0x6e, 0xf6, 0x7d, 0xc6, 0x10, 0xd2, 0xcd, 0x19, 0x24, 0x84, 0x78, 0x9a, 0x6c, 0xf9, 0xae, 0xa9, 0x93, 0x0b, 0x94, 0x4b, 0x7e, 0x2d, 0xb5, 0x34, 0x2b, 0x9d, 0x9e, 0x5b, 0x9f, 0xf7, 0x9a, 0xff, 0x9a, 0x2e, 0xe1, 0x97, 0x8d, 0xd7, 0xfd, 0x01, 0xdf, 0xc5, 0x22, 0xee, 0x02, 0x28, 0x3d, 0x3b, 0x06, 0xa9, 0xd0, 0x3a, 0xcf, 0x80, 0x96, 0x96, 0x8d, 0x7d, 0xbb, 0x0f, 0x91, 0x78, 0xff, 0xff, 0xff, 0xff, 0x02, 0x8b, 0xa7, 0x94, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xba, 0xde, 0xec, 0xfd, 0xef, 0x05, 0x07, 0x24, 0x7f, 0xc8, 0xf7, 0x42, 0x41, 0xd7, 0x3b, 0xc0, 0x39, 0x97, 0x2d, 0x7b, 0x88, 0xac, 0x40, 0x94, 0xa8, 0x02, 0x00, 0x00, 0x00, 0x00, 0x19, 0x76, 0xa9, 0x14, 0xc1, 0x09, 0x32, 0x48, 0x3f, 0xec, 0x93, 0xed, 0x51, 0xf5, 0xfe, 0x95, 0xe7, 0x25, 0x59, 0xf2, 0xcc, 0x70, 0x43, 0xf9, 0x88, 0xac, 0x00, 0x00, 0x00, 0x00, 0x00};
    //vector<unsigned char> vch(ch, ch + sizeof(ch) -1);
    //CDataStream spendStream(vch, SER_DISK, CLIENT_VERSION);

    CTransaction spendingTx;
    spendStream >> spendingTx;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(uint256S("0x19cfa5db35f33a2e67fe1b9b738731b62e548d2f27c55a2f28523fec52b71525"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // byte-reversed tx hash
    filter.insert(ParseHex("2515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match manually serialized tx hash");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("3045022100ba5e90204e83c5f961b67c6232c1cc6c360afd36d43fcfae0de7af2e75f4cda7022012fec415a12048dbb70511fda6195b090b56735232281dc1144409833a092edc01"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match input signature");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("02c322382e17c9ed4f47183f219cc5dd7853f939fb8eebae3c943622e0abf8d5e5"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match input pub key");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("30271a250e92135ce0db0783ebb63aaeb58e47f9"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match output address");
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(spendingTx), "Simple Bloom filter didn't add output");

    // Need a second output for this.
    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(ParseHex("5f0d00adba6489150808feb4108d7be582cbb2e1"));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match output address");

    // This prev output has index of 1
    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    filter.insert(COutPoint(uint256S("0xd1a3166572a3695031d18ae4826e0b85ca616715d86948f8bb767449e58bd75a"), 1));
    BOOST_CHECK_MESSAGE(filter.IsRelevantAndUpdate(tx), "Simple Bloom filter didn't match COutPoint");

    filter = CBloomFilter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    COutPoint prevOutPoint(uint256S("0xd1a3166572a3695031d18ae4826e0b85ca616715d86948f8bb767449e58bd75a"), 1);
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
    // zcash regtest block 25abec437fb3e176fc3058076b78ec95249f20ceb4e2cff69f90525d964440b5
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("040000008e0fd453540d05e9a718b02c4643ad30f56bc76b3ae4339b95cc162d272b064b23f737f85b3e014364614b706e37e032820c9b924cf3496d3dd2f5b267f2aa790000000000000000000000000000000000000000000000000000000000000000974b9857ffff7f200400180af05909e4db334305b55e0876b79eaa64ebdd64d56f8a8b503e99000020080000003301000069000000880000004e000000f6010000d9000000e40000001f00000032010000150100002101000036000000ec010000620000000d0100003d00000078010000df0000004b01000070000000e0010000f1000000e70100006c000000ce0100007e010000e30100006f000000b3000000ba010000bf0100000601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff035c0101ffffffff0256dd9a3b0000000023210262e10da104f17773769ad0bfb03bbb98c51e1389e372fcc528bece7ded083955ac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a491870000000001000000018252cdb4d1d1a7417fb063a09b0fb89318fd8b792d93af80e27b8b477df4738c0000000048473044022068ad43fc1c4fb6d37c369ea2ee4120023380753ed7f01fd349b14d86f34acc4a02202b6852e81505e756781c396e99161e8627933730bb057181b94f3e40cd80a57901feffffff02400d0300000000001976a914c390a7a91a06692d15e50eae443018bae3434a3188ac05b9973b000000001976a91492a07418ce1a941746b22bf51bc5d33e8394cf2688ac0100000001000000019f726046a4fb8c30815afe55fd6799eb898c8948e9605c397294a8f25592fba300000000484730440220116b3265320e4b1dece005978b2194009693e5773496ef51ed599400275abd44022025bcf8e76b1399204b36929f6e6e1266d5dcc7939e45d7cb8334bd42dab4868401feffffff02a03f993b000000001976a914899e43d2889647d3c3eefd741e15c1373a4f895188aca0860100000000001976a914202662c4d7b0164bd21f64d5af99a7fdb6eb0d5d88ac0100000001000000012343cf08fa9ab78c3ed238e76c678efcfa54ed52e496eda7873e019a294eff30000000004847304402201c1b8318cd3f869a0a6e69f0577a27a10f531805330ab17f69d61711ee00ab6902207515fbebace064948eba406b541108fe1ad771c9005f849a97bb82a3212a3aaf01feffffff02557c983b000000001976a914ead7e8b79a77c04fe65cb52b969c0cfcf97d632d88acf0490200000000001976a9148298e83e54bf2b016697b5bdebae79f190bbec5588ac000000000100000001d9ce0f8646d4c5ce03d00250bdefd0c268b902f194b6ac6de9e79010f10ea3d1000000004847304402207f68d9fcb9d2376d7755e5819ba905bd6862839a929dc79e9cf0e2fd56b9ded80220600b7aa879d1bcce4c77561c3f3229fd0fa14cf65247cee9ad62d03eb2580d2401feffffff02f0490200000000001976a9148298e83e54bf2b016697b5bdebae79f190bbec5588ac557c983b000000001976a914a97a3406f9b8f7a3720e5d35ff785e756552084188ac010000000100000001394e4d8af2a70d68d1048a91625289e79bc1f3b6e9627eb83b802e6eec46d47a010000006a473044022044d71071d687954d0a72788121b8f5284fbe39bd34ac7c08824ea3b190fb1ad80220255c7820980fd0e0040fed568f00c200d7b564dc452e7e82b8287bb7d67447450121033f69ce5e46b834ef6ac60b50b0e053fab3f2a41fc8656b73296e91415af85a9ffeffffff02400d0300000000001976a914c390a7a91a06692d15e50eae443018bae3434a3188ac5b694b3b000000001976a9146a9912375a85f2bb9137a1bbcd8dd6517763497088ac01000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S("0xe9ad9403edde19b31c09a9b5fd6fbbcaaab9e9905906d04b8acd0528671c6172"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xe9ad9403edde19b31c09a9b5fd6fbbcaaab9e9905906d04b8acd0528671c6172"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 5);
    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Also match the 2nd last
    filter.insert(uint256S("0x8282c7e860d707964a7bdeec5adca426195abc60d295189ca505e79038b6d6e5"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0x8282c7e860d707964a7bdeec5adca426195abc60d295189ca505e79038b6d6e5"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 4);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}


BOOST_AUTO_TEST_CASE(merkle_block_2)
{
    // Regtest zcash block 78d1747c76b6e226a141283fa989b7b4b758ef49f6e329b45f1ea927b5b06582
    // With 5 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000058d318a0ce55f9f9fda7456c0c608ed68653776010cb6c6dcfc9657641506e1cce61be33285db054c4b3f1f2e069c759e94570e4902bcb0dbe549a39e4d98f6b0000000000000000000000000000000000000000000000000000000000000000a7559857fbff7f2003001c9c71914ce8f872f4dbc7c7d8b1615d6f6a1f78d04e07ceb619f0b700002001000000fd00000036000000f90100002600000058010000420000008301000083000000b8000000050100002301000094000000ab0000000d0100007e010000370000004500000006010000d10100007f000000330100009f010000d501000058000000aa000000a100000057010000b600000002010000be010000e10100000401000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0401150101ffffffff02d4dbd73b00000000232102498e387bed66f7ccd4a039208a46ea3e04680501b7a9c574df38bb2f172cbe0fac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a49187000000000100000001451bb020824ae2d6440576e58e9fa9c58086c547f0168c4cafe10f7b4285cded0000000049483045022100cf73e16190bc1a45451d9e603bb828b7b95f0bbae11865b8917bd3aa55f1699b0220454dce223ce17a266d2d193103bdc403378378a6dc9f0ad763537885fb55c69c01feffffff02567a4e3b000000001976a9146b18cb96f32a92fbeefb7cea7eaf0dd2bf95da3c88ac404b4c00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0a00000001000000015ad78be5497476bbf84869d8156761ca850b6e82e48ad1315069a3726516a3d1010000006b483045022100ba5e90204e83c5f961b67c6232c1cc6c360afd36d43fcfae0de7af2e75f4cda7022012fec415a12048dbb70511fda6195b090b56735232281dc1144409833a092edc012102c322382e17c9ed4f47183f219cc5dd7853f939fb8eebae3c943622e0abf8d5e5feffffff0280969800000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988acd694693a000000001976a9145f0d00adba6489150808feb4108d7be582cbb2e188ac0a00000001000000012515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19000000006b4830450221009e496c27d1ec174666da187815407c5b7b4d71ed8935e427a334df6d5d7c9e2c022007ca48eabad68cdec0a7d3be15d64a950bb3d80ac2b988fcd82394064a0a32a0012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cffffffff01808d5b00000000001976a9146d1c88970e614202031a4f2ba13846f8ce91019e88ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the first transaction
    filter.insert(uint256S("0x2c713883648f44be99ff858b283517f571aa725a4fc032ca52c952fc68058d6e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0x2c713883648f44be99ff858b283517f571aa725a4fc032ca52c952fc68058d6e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Match an output from the second transaction (the pubkey for address mjuZa8Dy12HKyUNjc1whNTxRaGU4gGGLxY)
    // It also matches the third transaction, which spends to the pubkey again
    // This should match the last transaction because it spends the output matched
    filter.insert(ParseHex("30271a250e92135ce0db0783ebb63aaeb58e47f9"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 4);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second == uint256S("0x77a47fdfefde8c91d0e0b14e61f1fd480913d609b7f655152abe14b40f4ffea9"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second == uint256S("0x19cfa5db35f33a2e67fe1b9b738731b62e548d2f27c55a2f28523fec52b71525"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[3].second == uint256S("0x1079621bf638e8bea9ee2f8d15287ae31a269969c360006d68d8ee07a7f532e6"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[3].first == 3);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}

BOOST_AUTO_TEST_CASE(merkle_block_2_with_update_none)
{
    // Regtest zcash block 78d1747c76b6e226a141283fa989b7b4b758ef49f6e329b45f1ea927b5b06582
    // With 5 txes
    CBlock block;
    CDataStream stream(ParseHex("0400000058d318a0ce55f9f9fda7456c0c608ed68653776010cb6c6dcfc9657641506e1cce61be33285db054c4b3f1f2e069c759e94570e4902bcb0dbe549a39e4d98f6b0000000000000000000000000000000000000000000000000000000000000000a7559857fbff7f2003001c9c71914ce8f872f4dbc7c7d8b1615d6f6a1f78d04e07ceb619f0b700002001000000fd00000036000000f90100002600000058010000420000008301000083000000b8000000050100002301000094000000ab0000000d0100007e010000370000004500000006010000d10100007f000000330100009f010000d501000058000000aa000000a100000057010000b600000002010000be010000e10100000401000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0401150101ffffffff02d4dbd73b00000000232102498e387bed66f7ccd4a039208a46ea3e04680501b7a9c574df38bb2f172cbe0fac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a49187000000000100000001451bb020824ae2d6440576e58e9fa9c58086c547f0168c4cafe10f7b4285cded0000000049483045022100cf73e16190bc1a45451d9e603bb828b7b95f0bbae11865b8917bd3aa55f1699b0220454dce223ce17a266d2d193103bdc403378378a6dc9f0ad763537885fb55c69c01feffffff02567a4e3b000000001976a9146b18cb96f32a92fbeefb7cea7eaf0dd2bf95da3c88ac404b4c00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0a00000001000000015ad78be5497476bbf84869d8156761ca850b6e82e48ad1315069a3726516a3d1010000006b483045022100ba5e90204e83c5f961b67c6232c1cc6c360afd36d43fcfae0de7af2e75f4cda7022012fec415a12048dbb70511fda6195b090b56735232281dc1144409833a092edc012102c322382e17c9ed4f47183f219cc5dd7853f939fb8eebae3c943622e0abf8d5e5feffffff0280969800000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988acd694693a000000001976a9145f0d00adba6489150808feb4108d7be582cbb2e188ac0a00000001000000012515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19000000006b4830450221009e496c27d1ec174666da187815407c5b7b4d71ed8935e427a334df6d5d7c9e2c022007ca48eabad68cdec0a7d3be15d64a950bb3d80ac2b988fcd82394064a0a32a0012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cffffffff01808d5b00000000001976a9146d1c88970e614202031a4f2ba13846f8ce91019e88ac00000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the first transaction
    filter.insert(uint256S("0x2c713883648f44be99ff858b283517f571aa725a4fc032ca52c952fc68058d6e"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0x2c713883648f44be99ff858b283517f571aa725a4fc032ca52c952fc68058d6e"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Match an output from the second transaction (the pubkey for address mjuZa8Dy12HKyUNjc1whNTxRaGU4gGGLxY)
    // It will match the third transaction, which has another pay-to-pubkey output to the same address
    // This should not match the last transaction though it spends the output matched
    filter.insert(ParseHex("30271a250e92135ce0db0783ebb63aaeb58e47f9"));

    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 3);

    BOOST_CHECK(pair == merkleBlock.vMatchedTxn[0]);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1].second == uint256S("0x77a47fdfefde8c91d0e0b14e61f1fd480913d609b7f655152abe14b40f4ffea9"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[1].first == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[2].second == uint256S("0x19cfa5db35f33a2e67fe1b9b738731b62e548d2f27c55a2f28523fec52b71525"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[2].first == 2);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}


BOOST_AUTO_TEST_CASE(merkle_block_3_and_serialize)
{
    // Regtest zcash block 528d8fc9ff3bb70e84011c1246eafa2977bce1ce0df29bb4b7512154ff8553a9
    // With one tx
    CBlock block;
    CDataStream stream(ParseHex("04000000b54044965d52909ff6cfe2b4ce209f2495ec786b075830fc76e1b37f43ecab254dde7891cec6846a10750145c12984a62d2f001e0640cb4b4fac6ceaca1ca0c70000000000000000000000000000000000000000000000000000000000000000fe4c9857ffff7f20030013205b410bb90025f179fdfc1d28fb9902d7d0c11958e349b2911850000020010000000a000000ae000000040100008d00000014010000b5010000d4010000220000003101000050010000c801000053000000d701000026010000ad01000016000000b60000004f000000ac0000004300000045000000c6000000cc0000002c000000d3000000ec000000b9010000ad000000ba000000e0000000d90100000101000000010000000000000000000000000000000000000000000000000000000000000000ffffffff035d0101ffffffff0200ca9a3b0000000023210262e10da104f17773769ad0bfb03bbb98c51e1389e372fcc528bece7ded083955ac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the only transaction
    filter.insert(uint256S("0xc7a01ccaea6cac4f4bcb40061e002f2da68429c1450175106a84c6ce9178de4d"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xc7a01ccaea6cac4f4bcb40061e002f2da68429c1450175106a84c6ce9178de4d"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 0);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    CDataStream merkleStream(SER_NETWORK, PROTOCOL_VERSION);
    merkleStream << merkleBlock;
    vector<unsigned char> vch = ParseHex("04000000b54044965d52909ff6cfe2b4ce209f2495ec786b075830fc76e1b37f43ecab254dde7891cec6846a10750145c12984a62d2f001e0640cb4b4fac6ceaca1ca0c70000000000000000000000000000000000000000000000000000000000000000fe4c9857ffff7f20030013205b410bb90025f179fdfc1d28fb9902d7d0c11958e349b2911850000020010000000a000000ae000000040100008d00000014010000b5010000d4010000220000003101000050010000c801000053000000d701000026010000ad01000016000000b60000004f000000ac0000004300000045000000c6000000cc0000002c000000d3000000ec000000b9010000ad000000ba000000e0000000d901000001000000014dde7891cec6846a10750145c12984a62d2f001e0640cb4b4fac6ceaca1ca0c70101");
    vector<char> expected(vch.size());

    for (unsigned int i = 0; i < vch.size(); i++)
        expected[i] = (char)vch[i];

    // Uncomment this line to print out the merkleBlock
//    std::cout << HexStr(merkleStream.begin(), merkleStream.end()) << std::endl;

    BOOST_CHECK_EQUAL_COLLECTIONS(expected.begin(), expected.end(), merkleStream.begin(), merkleStream.end());
}


BOOST_AUTO_TEST_CASE(merkle_block_4)
{
    // Regtest zcash block 2af9d3f42301c7411b932268e0ca6dfef3864d2164970dd1dbc351a5a994c988
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("04000000ceafe737af74e90fd22ba542d7fe3cd149d8724e4820ae94d1478b056d2ada166ecd34e2c9cf6458b6dc1e88d6ebbeb68cf61976398d2e25e7af12bdfb9e80b30000000000000000000000000000000000000000000000000000000000000000fa589857feff7f200100766220c3fb1793605fcfa671397bed77091d27090840c09c6c90cddb000020250000003c0100005f0100009a01000048000000e60100006b010000bc010000470000003b0100008300000001010000c800000020010000d1000000b601000034000000080100003d0000004b0000003a000000a900000094000000da010000530000008000000096000000a4010000b700000002010000c1000000d70000000601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0401170101ffffffff024ce29a3b000000002321021046035a4c9cf071676bbb2cafdccf8cdc7b67af239132992273ad53b3c23488ac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a491870000000001000000012bef176bd090bad3344e609e17b1e7b3be71b5e45a4809c917f94a61c8e992060000000049483045022100d2a72fa35d3e8737879d6dd964e5e5704b146871d946d46098edd59ab3415ed00220568bb41fb34f17db1fa609257b4fd04ffb0e35b41d43d288e5576e774f63bfda01feffffff0200848b3b000000001976a914baa161d9bd51f624ff4db28e3f00179c572e18cf88ac40420f00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0c0000000100000001a9fe4f0fb414be2a1555f6b709d6130948fdf1614eb1e0d0918cdeefdf7fa477000000006a47304402201316590dc69641d6c2c31c89b4434c214be5ffa58f8c7488123edb084aa7e72b02205eb24de0dd8dbe872d1221c4aad477f63988d5497b19ec5950ea1285cf3fb915012103e138ca8f557bbfa4260a15d79b849f92430f507c6631703a9c5e35822e9d952ffeffffff026cf12f3b000000001976a91409c106c79fa392733402b3f37c7fa4523158fbdf88ac80841e00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0c00000001000000012515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19010000006a473044022075f197b8d4df09e85b97b3aaeaaf0584ebdfc6b3bdc2743abfe4e573f68dea8002201850c7b96ec1593091d95377e8d7acc005032dce356325547bfb66bcca95d137012103ef486cc2238f19830e7761f015fbfd7df29699c13759abdaa883d78091c8b46dfeffffff02acc93b3a000000001976a914ec7dc17fd971f1d3341a064cf9f40c120075ebe488acc0c62d00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac000000000100000002a9fe4f0fb414be2a1555f6b709d6130948fdf1614eb1e0d0918cdeefdf7fa477010000006b483045022100e43bdeeb6c46a6f6cfc04f4c7365c899d73bab617da50be62ddeaaf8773ee6b4022040d292378e87892711e866fde87fcb9781caede32081e0d8d2ad88c4bf8787ed012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfeffffffe632f5a707eed8686d0060c36999261ae37a28158d2feea9bee838f61b627910000000006a4730440220179282f7f2298b1cdf369f5e36fd0380fdbc47e9b6c5b335d45d4734dc463448022030ec39229fc0daaf52ac6d96cfb739b6a59c880a811a933cb8994e5f5b08a4c80121038b35ef477570faed211a1f119441864d41b014a8a6475aad660c2961f8eb03a7feffffff02404b4c00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac32865b00000000001976a914f5ca46c06a8ee7653ba4c16c392db42d1e2ed3c388ac0c0000000100000001d4766c87e4f552823eea2d7ab989660605e62eb05a6d2b5ad43b36ae4f557c15000000006b4830450221009b226907cab7113b909b020049110c54c295bd693c0650fe6ed75acbc8dc276f02204981fab2548805f1922c1e2929aa2d1072a21cd15f6d2823897cbc5a0986f517012103716c8473f87f1212caf9aa5150cbc16b5059c9a416cd4606149693d3bd477442feffffff0200093d00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac16801e00000000001976a9149ded6f93a3a2b4f67a30ff30f10f5449ce63713488ac0c000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_ALL);
    // Match the last transaction
    filter.insert(uint256S("0xc10fa281cd5d2261a66875ee397b968881e3046e81b7792bcd2c2f9036a3dffa"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 1);
    pair<unsigned int, uint256> pair = merkleBlock.vMatchedTxn[0];

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xc10fa281cd5d2261a66875ee397b968881e3046e81b7792bcd2c2f9036a3dffa"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 5);

    vector<uint256> vMatched;
    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);

    // Also match the 4th transaction
    filter.insert(uint256S("0xd2f48bd2d3ec3679f2af0c79e2ad292fb84987f521704ef10bb9de8e1c50e687"));
    merkleBlock = CMerkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    BOOST_CHECK(merkleBlock.vMatchedTxn.size() == 2);

    BOOST_CHECK(merkleBlock.vMatchedTxn[0].second == uint256S("0xd2f48bd2d3ec3679f2af0c79e2ad292fb84987f521704ef10bb9de8e1c50e687"));
    BOOST_CHECK(merkleBlock.vMatchedTxn[0].first == 3);

    BOOST_CHECK(merkleBlock.vMatchedTxn[1] == pair);

    BOOST_CHECK(merkleBlock.txn.ExtractMatches(vMatched) == block.hashMerkleRoot);
    BOOST_CHECK(vMatched.size() == merkleBlock.vMatchedTxn.size());
    for (unsigned int i = 0; i < vMatched.size(); i++)
        BOOST_CHECK(vMatched[i] == merkleBlock.vMatchedTxn[i].second);
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_p2pubkey_only)
{
    // Regtest zcash block 2af9d3f42301c7411b932268e0ca6dfef3864d2164970dd1dbc351a5a994c988
    // With 6 txes
    CBlock block;
    CDataStream stream(ParseHex("04000000ceafe737af74e90fd22ba542d7fe3cd149d8724e4820ae94d1478b056d2ada166ecd34e2c9cf6458b6dc1e88d6ebbeb68cf61976398d2e25e7af12bdfb9e80b30000000000000000000000000000000000000000000000000000000000000000fa589857feff7f200100766220c3fb1793605fcfa671397bed77091d27090840c09c6c90cddb000020250000003c0100005f0100009a01000048000000e60100006b010000bc010000470000003b0100008300000001010000c800000020010000d1000000b601000034000000080100003d0000004b0000003a000000a900000094000000da010000530000008000000096000000a4010000b700000002010000c1000000d70000000601000000010000000000000000000000000000000000000000000000000000000000000000ffffffff0401170101ffffffff024ce29a3b000000002321021046035a4c9cf071676bbb2cafdccf8cdc7b67af239132992273ad53b3c23488ac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a491870000000001000000012bef176bd090bad3344e609e17b1e7b3be71b5e45a4809c917f94a61c8e992060000000049483045022100d2a72fa35d3e8737879d6dd964e5e5704b146871d946d46098edd59ab3415ed00220568bb41fb34f17db1fa609257b4fd04ffb0e35b41d43d288e5576e774f63bfda01feffffff0200848b3b000000001976a914baa161d9bd51f624ff4db28e3f00179c572e18cf88ac40420f00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0c0000000100000001a9fe4f0fb414be2a1555f6b709d6130948fdf1614eb1e0d0918cdeefdf7fa477000000006a47304402201316590dc69641d6c2c31c89b4434c214be5ffa58f8c7488123edb084aa7e72b02205eb24de0dd8dbe872d1221c4aad477f63988d5497b19ec5950ea1285cf3fb915012103e138ca8f557bbfa4260a15d79b849f92430f507c6631703a9c5e35822e9d952ffeffffff026cf12f3b000000001976a91409c106c79fa392733402b3f37c7fa4523158fbdf88ac80841e00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac0c00000001000000012515b752ec3f52282f5ac5272f8d542eb63187739b1bfe672e3af335dba5cf19010000006a473044022075f197b8d4df09e85b97b3aaeaaf0584ebdfc6b3bdc2743abfe4e573f68dea8002201850c7b96ec1593091d95377e8d7acc005032dce356325547bfb66bcca95d137012103ef486cc2238f19830e7761f015fbfd7df29699c13759abdaa883d78091c8b46dfeffffff02acc93b3a000000001976a914ec7dc17fd971f1d3341a064cf9f40c120075ebe488acc0c62d00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac000000000100000002a9fe4f0fb414be2a1555f6b709d6130948fdf1614eb1e0d0918cdeefdf7fa477010000006b483045022100e43bdeeb6c46a6f6cfc04f4c7365c899d73bab617da50be62ddeaaf8773ee6b4022040d292378e87892711e866fde87fcb9781caede32081e0d8d2ad88c4bf8787ed012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfeffffffe632f5a707eed8686d0060c36999261ae37a28158d2feea9bee838f61b627910000000006a4730440220179282f7f2298b1cdf369f5e36fd0380fdbc47e9b6c5b335d45d4734dc463448022030ec39229fc0daaf52ac6d96cfb739b6a59c880a811a933cb8994e5f5b08a4c80121038b35ef477570faed211a1f119441864d41b014a8a6475aad660c2961f8eb03a7feffffff02404b4c00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac32865b00000000001976a914f5ca46c06a8ee7653ba4c16c392db42d1e2ed3c388ac0c0000000100000001d4766c87e4f552823eea2d7ab989660605e62eb05a6d2b5ad43b36ae4f557c15000000006b4830450221009b226907cab7113b909b020049110c54c295bd693c0650fe6ed75acbc8dc276f02204981fab2548805f1922c1e2929aa2d1072a21cd15f6d2823897cbc5a0986f517012103716c8473f87f1212caf9aa5150cbc16b5059c9a416cd4606149693d3bd477442feffffff0200093d00000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988ac16801e00000000001976a9149ded6f93a3a2b4f67a30ff30f10f5449ce63713488ac0c000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_P2PUBKEY_ONLY);
    // Match the generation pubkey (coinbase tx output scriptpubkey)
    filter.insert(ParseHex("021046035a4c9cf071676bbb2cafdccf8cdc7b67af239132992273ad53b3c23488"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("30271a250e92135ce0db0783ebb63aaeb58e47f9"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We should match the generation outpoint (coinbase tx txid)
    BOOST_CHECK(filter.contains(COutPoint(uint256S("0x760d72ad023855ab213b2be97844b8e56be314510f619a7d647a6ec541f724c3"), 0)));
    // ... but not the 4th transaction's output (its not pay-2-pubkey) (the 4th tx's txid)
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("d2f48bd2d3ec3679f2af0c79e2ad292fb84987f521704ef10bb9de8e1c50e687"), 0)));
}

BOOST_AUTO_TEST_CASE(merkle_block_4_test_update_none)
{
    // Regtest zcash block 0c54eb9bf064c28342e030e940364b1379b0a63a81434d0bbc0246f8ebc1112d
    // With 7 txes
    CBlock block;
    CDataStream stream(ParseHex("04000000e1bf965fddae55b8e781877a939425f839e788959f9fbc72b81aef757848d1558f8f11f4bedfde1217f4b6f96fc9a91d0331386d0e01ea98eeccf03f9a6b35d50000000000000000000000000000000000000000000000000000000000000000005d9857feff7f200100e93d380bd116212dfc1d41e78e4df3d32d275d00d4f79808bbb884d30000201b00000051010000c1000000a1010000650000007a01000099000000b101000056000000700100008d0000007e01000076000000f70100009b010000fe010000260000000a0100008b000000430100002900000093010000a5000000d20000002a000000970000005e0000004601000080000000010100008c000000e70000000701000000010000000000000000000000000000000000000000000000000000000000000000ffffffff04011a0101ffffffff0283f19a3b00000000232102ce609e4c9c1ce8c86b5732cc71879d93d57bdcd78082cffb77bfa5c24c10dc50ac80b2e60e0000000017a9146708e6670db0b950dac68031025cc5b63213a49187000000000100000001c23c131d2130af5157aac9b378b5a2af8e02d6e9be91f2b3cb770731a62e698f000000004847304402204a0b8d78f2ab6b2c91e7e532c2344b064c13e3c198bb8c9e2613f796d028cb8b022040b8ad89ac9e7fa7b803dfe361f8af3681e89246f5f97aa6f5b49555d1f5ab7801feffffff02403f0738000000001976a91447568c26a8bdfa77a9253ed2036724f0b2c1fe5f88ac00879303000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88ac0f0000000100000001824d5f206ecb4d82fe4e7a3a10fe0b4186b0111298529d6d705a9bf56441a66d00000000494830450221009e1b7e896bdea8592040a9b2d40060b5c7440ff8adbe6fe2ce4a9ef003771058022013ef290a725f44147fb01c083ba7b7997f4c2675bdeccc107784cfc86b0f800b01feffffff0280969800000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88acc02f023b000000001976a914a6e9f40a37e4f1bc0f196cc8b8ee387e936e934488ac0f00000001000000017453c9e5a088cb04e40ef59abd459156da3288998b82589ee888c556f74573a4000000006a473044022036be899b55d10fbef581a392a813c9ea25fdbfa577d1e8d8b50c91a39b3d2d050220208f3356f8c599e62486fa67f300629aabba8a9c57a1feb63cfcf8d0197c97620121033506b92281f803dcfab4aedad1cbc4d0617499c55bda8a8704eb1add18e89235feffffff0296252939000000001976a914abc8feb974411a2307badee01e384641d49c186488ac005a6202000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88ac0f0000000100000006ef2f1a8d0ab669daa867c61aaa5446587c685a0a29933a29e3f68947ac9c31ff000000006a473044022069b9dc8e8321b51701f0e4ad74715e9a1928e7b4925d66eb060bcf7674092d1a0220417a26ed0e01c2167cbabfc5dec1a50099d77c666ae04cb3ccd493355830f802012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfeffffffef2f1a8d0ab669daa867c61aaa5446587c685a0a29933a29e3f68947ac9c31ff010000006a473044022078ba511e4c3bd22c66104d4380f553af4a318438da50340fa85e1bb7781a98d3022076022f343a883617be8b84c51127d081d9f533381108343abcc71546b9f05579012102498e387bed66f7ccd4a039208a46ea3e04680501b7a9c574df38bb2f172cbe0ffeffffff22e2702586c53ab876e350837d537cfbe2eda625e90c56bc6ac402e1ba2638cc010000006b483045022100e7e385d8cc3abd1f4d9d2bb2ee53ec75f1cc3aefdaae2e8fb2260d64b52ef2700220155ee4eb5352df0ecf6477ec39588da85983d17b07f8687878727acea9137617012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfefffffffadfa336902f2ccd2b79b7816e04e38188967b39ee7568a661225dcd81a20fc1000000006a4730440220166afc981f529ec240a9c76fd1a13f5266b49462c0d3b28370aa81f34f9de3d90220062b0579b6bae54b03e2606e3fc2c493a6b827884fe6b609e7871ee808675ca8012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfefffffffadfa336902f2ccd2b79b7816e04e38188967b39ee7568a661225dcd81a20fc1010000006a4730440220619df55063805d43bc046f710db900fbfe28ac5194aa3ad377a88765e119460a02205407f3f8272469fad59b0ab2b00c5c373aa35ccf0411040d977b58db9f39fea6012103322175384bd15072c75bc7c1bc0fa5e5595895cb3b8fd179b2b24746b673f5f6feffffff87e6501c8edeb90bf14e7021f58749b82f29ade2790caff27936ecd3d28bf4d2010000006a47304402204b84bf98244adb41dd739285513bcf50007875021257a821f9ae57991eb12cd102203fde9f5076f8dbf65ada8fc1e5ab46df3dba8d7621664cea3b84688a3a4b85d2012102afe67a769ff6a19f6a227b05d7e1a6b0a2e6851481c8c92f232979d33bb2bc0cfeffffff02fe651e00000000001976a91441ec7511554c71640aea061e628067f3db3aafd088ac002d3101000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88ac0f000000010000000122e2702586c53ab876e350837d537cfbe2eda625e90c56bc6ac402e1ba2638cc000000006a4730440220054c70db6a2433e3d94a43f5f198a05eca9b44b4f0763ae78b5bc70b08d52eb10220103062e40bfbd270117b5cca182098d143fedb44b572df912fe04798afba409e012102343107eef41bde463901d99c3011c9be972c9077a0c4c5897bb63eda936f8c74feffffff0287296639000000001976a914fe850141a91ae3eb661d8f425284ba2e3a03af2188ac80c3c901000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88ac0f000000010000000187e6501c8edeb90bf14e7021f58749b82f29ade2790caff27936ecd3d28bf4d2000000006b48304502210081c47f61994bc0999a2bbaefb7ebf625421793025e981564cf50dfa517791960022037d28ed35e2764ab8a5f579c5cfc7eb66dc246aebcb312f683baa82f49f6e492012102fe844d4520021d8f54985995fd57565139ee832b47dcec52106dbcdd928acf6efeffffff02c2d44037000000001976a914236bffa399b69dadbbbe662b59f3ca16327d68b288ac80f0fa02000000001976a914640bac400ad1fecc1391b6f1536924a28e61907d88ac0f000000"), SER_NETWORK, PROTOCOL_VERSION);
    stream >> block;

    CBloomFilter filter(10, 0.000001, 0, BLOOM_UPDATE_NONE);
    // Match the generation pubkey (coinbase tx output script)
    filter.insert(ParseHex("02ce609e4c9c1ce8c86b5732cc71879d93d57bdcd78082cffb77bfa5c24c10dc50"));
    // ...and the output address of the 4th transaction
    filter.insert(ParseHex("640bac400ad1fecc1391b6f1536924a28e61907d"));

    CMerkleBlock merkleBlock(block, filter);
    BOOST_CHECK(merkleBlock.header.GetHash() == block.GetHash());

    // We shouldn't match any outpoints (UPDATE_NONE)
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("0xc234ca41d97fc9da0312899aedff68d71d02dfb0533af97992868772030f028f"), 0)));
    BOOST_CHECK(!filter.contains(COutPoint(uint256S("0x2551b7b6108a633db54c3df7aff09295561fcc2b5b0438b602e5c17deb1d128e"), 0)));
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
