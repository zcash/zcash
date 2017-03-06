#include <future>
#include <thread>
#include <unistd.h>
#include <boost/filesystem.hpp>

#include "coins.h"
#include "util.h"
#include "init.h"
#include "primitives/transaction.h"
#include "base58.h"
#include "crypto/equihash.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "main.h"
#include "miner.h"
#include "pow.h"
#include "script/sign.h"
#include "sodium.h"
#include "streams.h"
#include "utiltest.h"
#include "wallet/wallet.h"

#include "zcbenchmarks.h"

#include "zcash/Zcash.h"
#include "zcash/IncrementalMerkleTree.hpp"

using namespace libzcash;

void timer_start(timeval &tv_start)
{
    gettimeofday(&tv_start, 0);
}

double timer_stop(timeval &tv_start)
{
    double elapsed;
    struct timeval tv_end;
    gettimeofday(&tv_end, 0);
    elapsed = double(tv_end.tv_sec-tv_start.tv_sec) +
        (tv_end.tv_usec-tv_start.tv_usec)/double(1000000);
    return elapsed;
}

double benchmark_sleep()
{
    struct timeval tv_start;
    timer_start(tv_start);
    sleep(1);
    return timer_stop(tv_start);
}

double benchmark_parameter_loading()
{
    // FIXME: this is duplicated with the actual loading code
    boost::filesystem::path pk_path = ZC_GetParamsDir() / "sprout-proving.key";
    boost::filesystem::path vk_path = ZC_GetParamsDir() / "sprout-verifying.key";

    struct timeval tv_start;
    timer_start(tv_start);

    auto newParams = ZCJoinSplit::Unopened();

    newParams->loadVerifyingKey(vk_path.string());
    newParams->setProvingKeyPath(pk_path.string());
    newParams->loadProvingKey();

    double ret = timer_stop(tv_start);

    delete newParams;

    return ret;
}

double benchmark_create_joinsplit()
{
    uint256 pubKeyHash;

    /* Get the anchor of an empty commitment tree. */
    uint256 anchor = ZCIncrementalMerkleTree().root();

    struct timeval tv_start;
    timer_start(tv_start);
    JSDescription jsdesc(*pzcashParams,
                         pubKeyHash,
                         anchor,
                         {JSInput(), JSInput()},
                         {JSOutput(), JSOutput()},
                         0,
                         0);
    double ret = timer_stop(tv_start);

    auto verifier = libzcash::ProofVerifier::Strict();
    assert(jsdesc.Verify(*pzcashParams, verifier, pubKeyHash));
    return ret;
}

std::vector<double> benchmark_create_joinsplit_threaded(int nThreads)
{
    std::vector<double> ret;
    std::vector<std::future<double>> tasks;
    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; i++) {
        std::packaged_task<double(void)> task(&benchmark_create_joinsplit);
        tasks.emplace_back(task.get_future());
        threads.emplace_back(std::move(task));
    }
    std::future_status status;
    for (auto it = tasks.begin(); it != tasks.end(); it++) {
        it->wait();
        ret.push_back(it->get());
    }
    for (auto it = threads.begin(); it != threads.end(); it++) {
        it->join();
    }
    return ret;
}

double benchmark_verify_joinsplit(const JSDescription &joinsplit)
{
    struct timeval tv_start;
    timer_start(tv_start);
    uint256 pubKeyHash;
    auto verifier = libzcash::ProofVerifier::Strict();
    joinsplit.Verify(*pzcashParams, verifier, pubKeyHash);
    return timer_stop(tv_start);
}

double benchmark_solve_equihash()
{
    CBlock pblock;
    CEquihashInput I{pblock};
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << I;

    unsigned int n = Params(CBaseChainParams::MAIN).EquihashN();
    unsigned int k = Params(CBaseChainParams::MAIN).EquihashK();
    crypto_generichash_blake2b_state eh_state;
    EhInitialiseState(n, k, eh_state);
    crypto_generichash_blake2b_update(&eh_state, (unsigned char*)&ss[0], ss.size());

    uint256 nonce;
    randombytes_buf(nonce.begin(), 32);
    crypto_generichash_blake2b_update(&eh_state,
                                    nonce.begin(),
                                    nonce.size());

    struct timeval tv_start;
    timer_start(tv_start);
    std::set<std::vector<unsigned int>> solns;
    EhOptimisedSolveUncancellable(n, k, eh_state,
                                  [](std::vector<unsigned char> soln) { return false; });
    return timer_stop(tv_start);
}

std::vector<double> benchmark_solve_equihash_threaded(int nThreads)
{
    std::vector<double> ret;
    std::vector<std::future<double>> tasks;
    std::vector<std::thread> threads;
    for (int i = 0; i < nThreads; i++) {
        std::packaged_task<double(void)> task(&benchmark_solve_equihash);
        tasks.emplace_back(task.get_future());
        threads.emplace_back(std::move(task));
    }
    std::future_status status;
    for (auto it = tasks.begin(); it != tasks.end(); it++) {
        it->wait();
        ret.push_back(it->get());
    }
    for (auto it = threads.begin(); it != threads.end(); it++) {
        it->join();
    }
    return ret;
}

double benchmark_verify_equihash()
{
    CChainParams params = Params(CBaseChainParams::MAIN);
    CBlock genesis = Params(CBaseChainParams::MAIN).GenesisBlock();
    CBlockHeader genesis_header = genesis.GetBlockHeader();
    struct timeval tv_start;
    timer_start(tv_start);
    CheckEquihashSolution(&genesis_header, params);
    return timer_stop(tv_start);
}

double benchmark_large_tx()
{
    // Number of inputs in the spending transaction that we will simulate
    const size_t NUM_INPUTS = 555;

    // Create priv/pub key
    CKey priv;
    priv.MakeNewKey(false);
    auto pub = priv.GetPubKey();
    CBasicKeyStore tempKeystore;
    tempKeystore.AddKey(priv);

    // The "original" transaction that the spending transaction will spend
    // from.
    CMutableTransaction m_orig_tx;
    m_orig_tx.vout.resize(1);
    m_orig_tx.vout[0].nValue = 1000000;
    CScript prevPubKey = GetScriptForDestination(pub.GetID());
    m_orig_tx.vout[0].scriptPubKey = prevPubKey;

    auto orig_tx = CTransaction(m_orig_tx);

    CMutableTransaction spending_tx;
    auto input_hash = orig_tx.GetHash();
    // Add NUM_INPUTS inputs
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        spending_tx.vin.emplace_back(input_hash, 0);
    }

    // Sign for all the inputs
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        SignSignature(tempKeystore, prevPubKey, spending_tx, i, SIGHASH_ALL);
    }

    // Serialize:
    {
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << spending_tx;
        //std::cout << "SIZE OF SPENDING TX: " << ss.size() << std::endl;

        auto error = MAX_TX_SIZE / 20; // 5% error
        assert(ss.size() < MAX_TX_SIZE + error);
        assert(ss.size() > MAX_TX_SIZE - error);
    }

    // Spending tx has all its inputs signed and does not need to be mutated anymore
    CTransaction final_spending_tx(spending_tx);

    // Benchmark signature verification costs:
    struct timeval tv_start;
    timer_start(tv_start);
    for (size_t i = 0; i < NUM_INPUTS; i++) {
        ScriptError serror = SCRIPT_ERR_OK;
        assert(VerifyScript(final_spending_tx.vin[i].scriptSig,
                            prevPubKey,
                            STANDARD_SCRIPT_VERIFY_FLAGS,
                            TransactionSignatureChecker(&final_spending_tx, i),
                            &serror));
    }
    return timer_stop(tv_start);
}

double benchmark_try_decrypt_notes(size_t nAddrs)
{
    CWallet wallet;
    for (int i = 0; i < nAddrs; i++) {
        auto sk = libzcash::SpendingKey::random();
        wallet.AddSpendingKey(sk);
    }

    auto sk = libzcash::SpendingKey::random();
    auto tx = GetValidReceive(*pzcashParams, sk, 10, true);

    struct timeval tv_start;
    timer_start(tv_start);
    auto nd = wallet.FindMyNotes(tx);
    return timer_stop(tv_start);
}

double benchmark_increment_note_witnesses(size_t nTxs)
{
    CWallet wallet;
    ZCIncrementalMerkleTree tree;

    auto sk = libzcash::SpendingKey::random();
    wallet.AddSpendingKey(sk);

    // First block
    CBlock block1;
    for (int i = 0; i < nTxs; i++) {
        auto wtx = GetValidReceive(*pzcashParams, sk, 10, true);
        auto note = GetNote(*pzcashParams, sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        CNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;

        wtx.SetNoteData(noteData);
        wallet.AddToWallet(wtx, true, NULL);
        block1.vtx.push_back(wtx);
    }
    CBlockIndex index1(block1);
    index1.nHeight = 1;

    // Increment to get transactions witnessed
    wallet.ChainTip(&index1, &block1, tree, true);

    // Second block
    CBlock block2;
    block2.hashPrevBlock = block1.GetHash();
    {
        auto wtx = GetValidReceive(*pzcashParams, sk, 10, true);
        auto note = GetNote(*pzcashParams, sk, wtx, 0, 1);
        auto nullifier = note.nullifier(sk);

        mapNoteData_t noteData;
        JSOutPoint jsoutpt {wtx.GetHash(), 0, 1};
        CNoteData nd {sk.address(), nullifier};
        noteData[jsoutpt] = nd;

        wtx.SetNoteData(noteData);
        wallet.AddToWallet(wtx, true, NULL);
        block2.vtx.push_back(wtx);
    }
    CBlockIndex index2(block2);
    index2.nHeight = 2;

    struct timeval tv_start;
    timer_start(tv_start);
    wallet.ChainTip(&index2, &block2, tree, true);
    return timer_stop(tv_start);
}

