#include <cryptoconditions.h>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

#include "core_io.h"
#include "key.h"
#include "main.h"
#include "miner.h"
#include "notarisationdb.h"
#include "random.h"
#include "rpc/server.h"
#include "rpc/protocol.h"
#include "txdb.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "consensus/validation.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "script/interpreter.h"

#include "testutils.h"


std::string notaryPubkey = "0205a8ad0c1dbc515f149af377981aab58b836af008d4d7ab21bd76faf80550b47";
std::string notarySecret = "UxFWWxsf1d7w7K5TvAWSkeX4H95XQKwdwGv49DXwWUTzPTTjHBbU";
CKey notaryKey;


/*
 * We need to have control of clock,
 * otherwise block production can fail.
 */
int64_t nMockTime;

extern uint32_t USE_EXTERNAL_PUBKEY;
extern std::string NOTARY_PUBKEY;

void setupChain()
{
    SelectParams(CBaseChainParams::REGTEST);

    // Settings to get block reward
    NOTARY_PUBKEY = notaryPubkey;
    USE_EXTERNAL_PUBKEY = 1;
    mapArgs["-mineraddress"] = "bogus";
    COINBASE_MATURITY = 1;
    // Global mock time
    nMockTime = GetTime();
    
    // Unload
    UnloadBlockIndex();

    // Init blockchain
    ClearDatadirCache();
    auto pathTemp = GetTempPath() / strprintf("test_komodo_%li_%i", GetTime(), GetRand(100000));
    if (ASSETCHAINS_SYMBOL[0])
        pathTemp = pathTemp / strprintf("_%s", ASSETCHAINS_SYMBOL);
    boost::filesystem::create_directories(pathTemp);
    mapArgs["-datadir"] = pathTemp.string();
    pblocktree = new CBlockTreeDB(1 << 20, true);
    CCoinsViewDB *pcoinsdbview = new CCoinsViewDB(1 << 23, true);
    pcoinsTip = new CCoinsViewCache(pcoinsdbview);
    pnotarisations = new NotarisationDB(1 << 20, true);
    InitBlockIndex();
}


void generateBlock(CBlock *block)
{
    UniValue params;
    params.setArray();
    params.push_back(1);
    uint256 blockId;

    SetMockTime(nMockTime+=100);  // CreateNewBlock can fail if not enough time passes

    try {
        UniValue out = generate(params, false, CPubKey());
        blockId.SetHex(out[0].getValStr());
        if (block) ASSERT_TRUE(ReadBlockFromDisk(*block, mapBlockIndex[blockId], false));
    } catch (const UniValue& e) {
        FAIL() << "failed to create block: " << e.write().data();
    }
}


void acceptTxFail(const CTransaction tx)
{
    CValidationState state;
    if (!acceptTx(tx, state)) FAIL() << state.GetRejectReason();
}


bool acceptTx(const CTransaction tx, CValidationState &state)
{
    LOCK(cs_main);
    return AcceptToMemoryPool(mempool, state, tx, false, NULL);
}


CMutableTransaction spendTx(const CTransaction &txIn, int nOut)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = txIn.GetHash();
    mtx.vin[0].prevout.n = nOut;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = txIn.vout[nOut].nValue - 1000;
    return mtx;
}


std::vector<uint8_t> getSig(const CMutableTransaction mtx, CScript inputPubKey, int nIn)
{
    uint256 hash = SignatureHash(inputPubKey, mtx, nIn, SIGHASH_ALL, 0, 0);
    std::vector<uint8_t> vchSig;
    notaryKey.Sign(hash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    return vchSig;
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
    uint256 hash = SignatureHash(coinbase.vout[0].scriptPubKey, mtx, 0, SIGHASH_ALL, 0, 0);
    std::vector<unsigned char> vchSig;
    notaryKey.Sign(hash, vchSig);
    vchSig.push_back((unsigned char)SIGHASH_ALL);
    mtx.vin[0].scriptSig << vchSig;

    // Accept
    acceptTxFail(mtx);
    txIn = CTransaction(mtx);
}
