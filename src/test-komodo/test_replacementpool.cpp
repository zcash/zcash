#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "base58.h"
#include "core_io.h"
#include "key.h"
#include "komodo_cryptoconditions.h"
#include "main.h"
#include "miner.h"
#include "random.h"
#include "rpcserver.h"
#include "rpcprotocol.h"
#include "replacementpool.h"
#include "txdb.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "script/interpreter.h"
#include "cryptoconditions/include/cryptoconditions.h"


extern int32_t USE_EXTERNAL_PUBKEY;
extern std::string NOTARY_PUBKEY;
std::string _NOTARY_PUBKEY = "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47";
std::string NOTARY_SECRET = "UxFWWxsf1d7w7K5TvAWSkeX4H95XQKwdwGv49DXwWUTzPTTjHBbU";
CKey notaryKey;

#define VCH(a,b) std::vector<unsigned char>(a, a + b)


/*
 * We need to have control of clock,
 * otherwise block production can fail.
 */
int64_t nMockTime;


void testSetup()
{
    SelectParams(CBaseChainParams::REGTEST);

    // enable CC
    ASSETCHAINS_CC = 1;
    ASSETCHAINS_CC_TEST = true;

    // Settings to get block reward
    NOTARY_PUBKEY = _NOTARY_PUBKEY;
    USE_EXTERNAL_PUBKEY = 1;
    mapArgs["-mineraddress"] = "bogus";
    COINBASE_MATURITY = 1;
    // Global mock time
    nMockTime = GetTime();

    // Init blockchain
    ClearDatadirCache();
    auto pathTemp = GetTempPath() / strprintf("test_komodo_%li_%i", GetTime(), GetRand(100000));
    boost::filesystem::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();
    pblocktree = new CBlockTreeDB(1 << 20, true);
    CCoinsViewDB *pcoinsdbview = new CCoinsViewDB(1 << 23, true);
    pcoinsTip = new CCoinsViewCache(pcoinsdbview);
    InitBlockIndex();

    // Set address
    ECC_Start();

    // Notary key
    CBitcoinSecret vchSecret;
    // this returns false due to network prefix mismatch but works anyway
    vchSecret.SetString(NOTARY_SECRET);
    notaryKey = vchSecret.GetKey();
}


void generateBlock(CBlock *block=NULL)
{
    UniValue params;
    params.setArray();
    params.push_back(1);
    uint256 blockId;

    SetMockTime(nMockTime++);  // CreateNewBlock can fail if not enough time passes

    try {
        UniValue out = generate(params, false);
        blockId.SetHex(out[0].getValStr());
    } catch (const UniValue& e) {
        FAIL() << "failed to create block: " << e.write().data();
    }
    if (block) ASSERT_TRUE(ReadBlockFromDisk(*block, mapBlockIndex[blockId]));
}


void acceptTx(const CTransaction tx)
{
    CValidationState state;
    LOCK(cs_main);
    if (!AcceptToMemoryPool(mempool, state, tx, false, NULL))
        FAIL() << state.GetRejectReason();
}


static CMutableTransaction spendTx(const CTransaction &txIn, int nOut=0)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = txIn.GetHash();
    mtx.vin[0].prevout.n = nOut;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = txIn.vout[nOut].nValue - 1000;
    return mtx;
}


/*
 * In order to do tests there needs to be inputs to spend.
 * This method creates a block and returns a transaction that spends the coinbase.
 */
void getInputTx(CScript scriptPubKey, CTransaction &txIn)
{
    // Get coinbase
    CBlock block;
    generateBlock(&block);
    CTransaction coinbase = block.vtx[0];

    // Create tx
    auto mtx = spendTx(coinbase);
    mtx.vout[0].scriptPubKey = scriptPubKey;
    uint256 hash = SignatureHash(coinbase.vout[0].scriptPubKey, mtx, 0, SIGHASH_ALL);
    std::vector<unsigned char> vchSig;
    notaryKey.Sign(hash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Accept
    acceptTx(mtx);
    txIn = CTransaction(mtx);
}


std::string HexToB64(std::string hexStr)
{
    auto d = ParseHex(hexStr);
    return EncodeBase64(d.data(), d.size());
}


CC *getReplaceCond()
{
    const char *condJsonTpl = R"V0G0N(
    { "type": "threshold-sha-256",
      "threshold": 2,
      "subfulfillments": [
          { "type": "secp256k1-sha-256", "publicKey": "%s"},
          { "type": "eval-sha-256", "method": "testReplace", "params": "" }
      ] })V0G0N";
    char condJson[1000];
    sprintf(condJson, condJsonTpl, (char*)HexToB64(NOTARY_PUBKEY).data());

    unsigned char err[1000] = "\0";
    return cc_conditionFromJSONString((const unsigned char*)condJson, err);
    // above could fail
}

CScript condPK(CC *cond)
{
    unsigned char buf[1000];
    size_t bufLen = cc_conditionBinary(cond, buf);
    return CScript() << VCH(buf, bufLen) << OP_CHECKCRYPTOCONDITION;
}

void setFulfillment(CMutableTransaction &mtx, CC *cond, const CScript &spk, int nIn=0)
{
    uint256 hash = SignatureHash(spk, mtx, nIn, SIGHASH_ALL);
    int nSigned = cc_signTreeSecp256k1Msg32(cond, notaryKey.begin(), hash.begin());
    unsigned char buf[1000];
    size_t bufLen = cc_fulfillmentBinary(cond, buf, 1000);
    mtx.vin[nIn].scriptSig = CScript() << VCH(buf, bufLen);
}


CScript getReplaceOut(unsigned char replacementWindow, unsigned char priority)
{
    std::vector<unsigned char> v = {replacementWindow, priority};
    return CScript() << OP_RETURN << v;
}


CTransaction _txout;
#define ONLY_REPLACEMENT_POOL(hash) ASSERT_TRUE(replacementPool.lookup(hash, _txout)); \
                                    ASSERT_FALSE(mempool.lookup(hash, _txout));
#define ONLY_MEM_POOL(hash) ASSERT_FALSE(replacementPool.lookup(hash, _txout)); \
                            ASSERT_TRUE(mempool.lookup(hash, _txout));
                                      


// Setup environment and perform basic spend as test
TEST(replacementpool, 0_setup)
{
    testSetup();  // Only call this method here

    CTransaction txIn;
    getInputTx(CScript() << OP_RETURN << VCH("1", 1), txIn);
}


// Perform replaceable spend
TEST(replacementpool, basic)
{
    CTransaction txIn;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);

    // Spend output to replaceable
    auto mtx = spendTx(txIn);
    mtx.vout[0].scriptPubKey = getReplaceOut(2, 100);
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);

    acceptTx(mtx);
    
    ONLY_REPLACEMENT_POOL(mtx.GetHash());
    generateBlock();
    ONLY_REPLACEMENT_POOL(mtx.GetHash());
    generateBlock();
    ONLY_MEM_POOL(mtx.GetHash());
}


/*
 * replacementWindow is 0, transaction should go direct to mempool
 */
TEST(replacementpool, noWindow)
{
    CTransaction txIn;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);

    // First set a tx with a 1 block wait. It should stay in the replacement pool.
    auto mtx = spendTx(txIn);
    mtx.vout[0].scriptPubKey = getReplaceOut(1, 100);
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx);
    ONLY_REPLACEMENT_POOL(mtx.GetHash());

    // Now set a transaction with a 0 block wait and higher priority.
    // It should go direct to the mem pool.
    auto mtx2 = spendTx(txIn);
    mtx2.vout[0].scriptPubKey = getReplaceOut(0, 101);
    setFulfillment(mtx2, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx2);
    ONLY_MEM_POOL(mtx2.GetHash());

    // Additionally, there should be no replacement remaining for txIn in the mempool
    ASSERT_FALSE(replacementPool.lookup(mtx.GetHash(), _txout));
}


/*
 * Multiple replaceable transactions dont interfere
 */
TEST(replacementpool, noInterfere)
{
    CTransaction txIn1, txIn2;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn1);
    getInputTx(condPK(cond), txIn2);

    // First set a transaction with a low window
    auto mtx = spendTx(txIn1);
    mtx.vout[0].scriptPubKey = getReplaceOut(1, 100);
    setFulfillment(mtx, cond, txIn1.vout[0].scriptPubKey);
    acceptTx(mtx);
    ONLY_REPLACEMENT_POOL(mtx.GetHash());

    // Now, a different spend with a higher window
    auto mtx2 = spendTx(txIn2);
    mtx2.vout[0].scriptPubKey = getReplaceOut(10, 100);
    setFulfillment(mtx2, cond, txIn2.vout[0].scriptPubKey);
    acceptTx(mtx2);
    ONLY_REPLACEMENT_POOL(mtx2.GetHash());

    generateBlock();

    // mtx has gone to mempool
    ONLY_MEM_POOL(mtx.GetHash());

    // mtx2 still in replacementpool
    ONLY_REPLACEMENT_POOL(mtx2.GetHash());

    // But 9 blocks later...
    for (int i=0; i<9; i++)
    {
        generateBlock();
        ASSERT_EQ(i == 8, mempool.lookup(mtx2.GetHash(), _txout));
    }
}


/*
 * Multiple inputs is invalid
 */
TEST(replacementpool, invalidMultipleInputs)
{
    LOCK(cs_main);

    CTransaction txIn, txIn2;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);
    getInputTx(condPK(cond), txIn2);

    CMutableTransaction mtx;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = txIn.vout[0].nValue * 2 - 1000;
    mtx.vout[0].scriptPubKey = getReplaceOut(1, 100);
    mtx.vin.resize(2);
    mtx.vin[0].prevout.hash = txIn.GetHash();
    mtx.vin[0].prevout.n = 0;
    mtx.vin[1].prevout.hash = txIn2.GetHash();
    mtx.vin[1].prevout.n = 0;
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);
    setFulfillment(mtx, cond, txIn2.vout[0].scriptPubKey, 1);

    CValidationState state;
    ASSERT_FALSE(AcceptToMemoryPool(mempool, state, mtx, false, NULL));
    ASSERT_EQ("replacement-invalid", state.GetRejectReason());
}


extern bool AddOrphanTx(const CTransaction& tx, NodeId peer);
extern void ProcessOrphanTransactions(uint256 parentHash);
struct COrphanTx { CTransaction tx; NodeId fromPeer; };
extern std::map<uint256, COrphanTx> mapOrphanTransactions;

/*
 * Orphans are processed
 */
TEST(replacementpool, orphansAreProcessed)
{
    LOCK(cs_main);

    CTransaction txIn;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);

    // Make basic replaceable spend and dont submit
    auto mtx = spendTx(txIn);
    mtx.vout.resize(2);
    mtx.vout[0].nValue = txIn.vout[0].nValue - 1000;
    mtx.vout[0].scriptPubKey = condPK(cond);
    mtx.vout[1].nValue = 1;
    mtx.vout[1].scriptPubKey = getReplaceOut(1, 100);
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);

    // Make an orphan and add it
    auto orphan = spendTx(mtx);
    orphan.vout[0].scriptPubKey = getReplaceOut(0, 100);
    setFulfillment(orphan, cond, mtx.vout[0].scriptPubKey);
    ASSERT_TRUE(AddOrphanTx(orphan, 1001));
    ASSERT_EQ(1, mapOrphanTransactions.count(orphan.GetHash()));

    // parent goes into replacement pool
    acceptTx(mtx);
    ONLY_REPLACEMENT_POOL(mtx.GetHash());

    // this should not result in the movement of any orphans
    ProcessOrphanTransactions(mtx.GetHash());
    ASSERT_EQ(1, mapOrphanTransactions.count(orphan.GetHash()));

    // Processing of parent transaction also un-orphanises orphan
    generateBlock();
    ONLY_MEM_POOL(mtx.GetHash());
    ONLY_MEM_POOL(orphan.GetHash());
    ASSERT_EQ(0, mapOrphanTransactions.count(orphan.GetHash()));
}


/*
 * Add transaction with lower priority, already have better
 */
TEST(replacementpool, haveBetter)
{
    LOCK(cs_main);

    CTransaction txIn;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);

    // A replaceable tx.
    auto mtx = spendTx(txIn);
    mtx.vout[0].scriptPubKey = getReplaceOut(2, 100);
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx);
    ONLY_REPLACEMENT_POOL(mtx.GetHash());

    // Another one, but not as good.
    auto mtx2 = spendTx(txIn);
    mtx2.vout[0].scriptPubKey = getReplaceOut(2, 99);
    setFulfillment(mtx2, cond, txIn.vout[0].scriptPubKey);
    CValidationState state;
    ASSERT_FALSE(AcceptToMemoryPool(mempool, state, mtx, false, NULL));
    ASSERT_EQ("replacement-is-worse", state.GetRejectReason());
    ONLY_REPLACEMENT_POOL(mtx.GetHash());
}


/*
 * Add transaction with lower priority, but shorter replacementWindow
 */
TEST(replacementpool, shorterReplacementWindow)
{
    LOCK(cs_main);

    CTransaction txIn;
    CC *cond = getReplaceCond();
    getInputTx(condPK(cond), txIn);

    // A replaceable tx.
    auto mtx = spendTx(txIn);
    mtx.vout[0].scriptPubKey = getReplaceOut(2, 100);
    setFulfillment(mtx, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx);
    ONLY_REPLACEMENT_POOL(mtx.GetHash());

    // Another one, lower priority but shorter replacementWindow so wins.
    auto mtx2 = spendTx(txIn);
    mtx2.vout[0].scriptPubKey = getReplaceOut(1, 99);
    setFulfillment(mtx2, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx2);
    ONLY_REPLACEMENT_POOL(mtx2.GetHash());
    ASSERT_FALSE(replacementPool.lookup(mtx.GetHash(), _txout));

    // Shorter still, in fact direct to mem pool
    auto mtx3 = spendTx(txIn);
    mtx3.vout[0].scriptPubKey = getReplaceOut(0, 98);
    setFulfillment(mtx3, cond, txIn.vout[0].scriptPubKey);
    acceptTx(mtx3);
    ONLY_MEM_POOL(mtx3.GetHash());
}
