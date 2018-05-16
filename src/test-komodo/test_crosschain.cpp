#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <cryptoconditions.h>
#include <gtest/gtest.h>

#include "cc/eval.h"
#include "importcoin.h"
#include "base58.h"
#include "core_io.h"
#include "crosschain.h"
#include "key.h"
#include "komodo_structs.h"
#include "main.h"
#include "notarisationdb.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"
#include "txmempool.h"
#include "crosschain.h"

#include "testutils.h"


extern uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);
extern struct notarized_checkpoint *komodo_npptr_at(int idx);


/*
 * Tests for the whole process of creating and validating notary proofs
 * using proof roots (MoMoMs). This is to support coin imports.
 */

namespace TestCrossChainProof {


class TestCrossChain : public ::testing::Test, public Eval {
public:
    bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const
    {
        NotarisationData data(2);
        return ParseNotarisationOpReturn(tx, data);  // If it parses it's valid
    }
protected:
    static void SetUpTestCase() { }
    virtual void SetUp() {
        ASSETCHAINS_CC = 1;
        EVAL_TEST = this;
    }
};


uint256 endianHash(uint256 h)
{
    uint256 out;
    for (int i=0; i<32; i++) {
        out.begin()[31-i] = h.begin()[i];
    }
    return out;
}


TEST_F(TestCrossChain, testCreateAndValidateImportProof)
{
    /*
     * This tests the full process of creation of a cross chain proof.
     * For the purposes of the test we will use one assetchain and a KMD chain.
     *
     * In order to do this test, we need 2 blockchains, so we'll fork and make a socket
     * for IPC.
     */

    int childPid = fork();
    void *ctx = zmq_ctx_new();
    void *socket = zmq_socket(ctx, ZMQ_PAIR);
    if (!childPid)
        strcpy(ASSETCHAINS_SYMBOL, "PIZZA");
    setupChain(); 
    std::vector<CBlock> blocks;
    blocks.resize(1000);
    NotarisationData a2kmd(0), kmd2a(1);
    int numTestNotarisations = 10;


    auto SendIPC = [&] (std::vector<uint8_t> v) {
        assert(v.size() == zmq_send(socket, v.data(), v.size(), 0));
    };

    auto RecvIPC = [&] () {
        std::vector<uint8_t> out;
        out.resize(100000);
        int len = zmq_recv(socket, out.data(), out.size(), 0);
        assert(len != -1);
        out.resize(len);
        return out;
    };

    auto RecordNotarisation = [&] (CTransaction inputTx, NotarisationData data) {
        CMutableTransaction mtx = spendTx(inputTx);
        mtx.vout.resize(2);
        mtx.vout[0].scriptPubKey << VCH(notaryKey.GetPubKey().begin(), 33) << OP_CHECKSIG;
        mtx.vout[1].scriptPubKey << OP_RETURN << E_MARSHAL(ss << data);
        mtx.vout[1].nValue = 0;
        mtx.vin[0].scriptSig << getSig(mtx, inputTx.vout[0].scriptPubKey);
        
        acceptTxFail(CTransaction(mtx));
        printf("accept %snotarisation: %s\n", data.IsBackNotarisation ? "back" : "",
                mtx.GetHash().GetHex().data());
        return mtx.GetHash();
    };

    auto RunTestAssetchain = [&] ()
    {
        NotarisationData n(0), back(1);
        strcpy(n.symbol, "PIZZA");
        n.ccId = 2;
        int height = 0;

        /*
         * Send notarisations and write backnotarisations
         */
        for (int ni=0; ni<numTestNotarisations; ni++)
        {
            generateBlock(&blocks[++height]);
            generateBlock(&blocks[++height]);
            n.blockHash = blocks[height].GetHash();
            n.MoM = endianHash(komodo_calcMoM(n.height=height, n.MoMDepth=2));
            SendIPC(E_MARSHAL(ss << n));
            assert(E_UNMARSHAL(RecvIPC(), ss >> back));
            RecordNotarisation(blocks[height].vtx[0], back);
        }

        /*
         * Generate proof
         */
        uint256 txid = blocks[7].vtx[0].GetHash();
        int npIdx;
        std::pair<uint256,MerkleBranch> proof = GetAssetchainProof(txid, npIdx);
        SendIPC(E_MARSHAL(ss << txid; ss << proof));

        /*
         * Test proof
         */
        std::pair<uint256,MerkleBranch> ccProof;
        E_UNMARSHAL(RecvIPC(), ss >> ccProof);

        // Now we have the branch with the hash of the notarisation on KMD
        // What we'd like is the notarised height on PIZZA so we can go forward
        // to the next backnotarisation, and then to the next, to get the M3.
        uint256 result = ccProof.second.Exec(txid);
        printf("result m3: %s\n", result.GetHex().data());
        struct notarized_checkpoint* np = komodo_npptr_at(npIdx+1);
        std::pair<uint256,NotarisationData> b;
        pnotarisations->Read(np->notarized_desttxid, b);
        printf("m3@1: %s\n", b.second.MoMoM.GetHex().data());

        {
            printf("RunTestAssetChain.test {\n  txid: %s\n  momom: %s\n", txid.GetHex().data(), b.second.MoMoM.GetHex().data());
            printf("  idx: %i\n", ccProof.second.nIndex);
            for (int i=0; i<ccProof.second.branch.size(); i++) printf("  %s", ccProof.second.branch[i].GetHex().data());
            printf("\n}\n");
        }

        return b.second.MoMoM == result ? 0 : 1;
    };

    auto RunTestKmd = [&] ()
    {
        NotarisationData n(0);
        int height = 0;

        /*
         * Write notarisations and send backnotarisations
         */
        for (int ni=0; ni<numTestNotarisations; ni++)
        {
            n.IsBackNotarisation = 0;
            E_UNMARSHAL(RecvIPC(), ss >> n);
            // Grab a coinbase input to fund notarisation
            generateBlock(&blocks[++height]);
            n.txHash = RecordNotarisation(blocks[height].vtx[0], n);
            {
                std::vector<uint256> moms;
                int assetChainHeight;
                n.MoMoM = GetProofRoot(n.symbol, 2, height, moms, &assetChainHeight);
            }
            printf("RunTestKmd {\n  kmdnotid:%s\n  momom:%s\n}\n", n.txHash.GetHex().data(), n.MoMoM.GetHex().data());
            n.IsBackNotarisation = 1;
            SendIPC(E_MARSHAL(ss << n));
        }

        /*
         * Extend proof
         */
        std::pair<uint256,MerkleBranch> proof;
        uint256 txid;
        // Extend proof to MoMoM
        assert(E_UNMARSHAL(RecvIPC(), ss >> txid; ss >> proof));
        proof.second = GetCrossChainProof(txid, (char*)"PIZZA", 2, proof.first, proof.second);
        SendIPC(E_MARSHAL(ss << proof));
    };

    const char endpoint[] = "ipc://tmpKomodoTestCrossChainSock";

    if (!childPid) {
        assert(0 == zmq_connect(socket, endpoint));
        usleep(20000);
        int out = RunTestAssetchain();
        if (!out) printf("Assetchain success\n");
        exit(out);
    }
    else {
        assert(0 == zmq_bind(socket, endpoint));
        RunTestKmd();
        int returnStatus;    
        waitpid(childPid, &returnStatus, 0);
        unlink("tmpKomodoTestCrossChainSock");
        ASSERT_EQ(0, returnStatus);
    }


    /*
     * We can now prove a tx from A on A, via a merkle root backpropagated from KMD.
     *
     * The transaction that we'll try to prove is the coinbase from the 3rd block.
     * We should be able to start with only that transaction ID, and generate a merkle
     * proof.
     */
}


} /* namespace TestCrossChainProof */
