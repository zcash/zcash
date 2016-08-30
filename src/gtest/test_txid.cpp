#include <gtest/gtest.h>
#include "primitives/transaction.h"
#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "util.h"
#include "utilstrencodings.h"

/*
 Test that removing #1144 succeeded by verifying the hash of a transaction is over the entire serialized form.
 */
TEST(txid_tests, check_txid_and_hash_are_same) {
    // Random zcash transaction aacaa62d40fcdd9192ed35ea9df31660ccf7f6c60566530faaa444fb5d0d410e
    CTransaction tx;
    CDataStream stream(ParseHex("01000000015ad78be5497476bbf84869d8156761ca850b6e82e48ad1315069a3726516a3d1010000006b483045022100ba5e90204e83c5f961b67c6232c1cc6c360afd36d43fcfae0de7af2e75f4cda7022012fec415a12048dbb70511fda6195b090b56735232281dc1144409833a092edc012102c322382e17c9ed4f47183f219cc5dd7853f939fb8eebae3c943622e0abf8d5e5feffffff0280969800000000001976a91430271a250e92135ce0db0783ebb63aaeb58e47f988acd694693a000000001976a9145f0d00adba6489150808feb4108d7be582cbb2e188ac0a000000"), SER_DISK, CLIENT_VERSION);
    stream >> tx;

    uint256 hash = uint256S("aacaa62d40fcdd9192ed35ea9df31660ccf7f6c60566530faaa444fb5d0d410e");
    ASSERT_TRUE(hash == tx.GetHash());
}

TEST(txid_tests, check_txid_and_hash_are_same_coinbase) {
    // Random zcash coinbase transaction 6041357de59ba64959d1b60f93de24dfe5ea1e26ed9e8a73d35b225a1845bad5
    CTransaction tx;
    CDataStream stream(ParseHex("01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff03530101ffffffff02f049020000000000232102f2df427fd4f76552ed0e86b17da8cf9b07754ea23ddc05799410a4a3a5d806ccac7c9200000000000017a9146708e6670db0b950dac68031025cc5b63213a4918700000000"), SER_DISK, CLIENT_VERSION);
    stream >> tx;

    ASSERT_TRUE(tx.IsCoinBase());

    uint256 hash = uint256S("6041357de59ba64959d1b60f93de24dfe5ea1e26ed9e8a73d35b225a1845bad5");
    ASSERT_TRUE(hash == tx.GetHash());
}

