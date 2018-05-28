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
extern bool KOMODO_TEST_ASSETCHAIN_SKIP_POW;


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
        KOMODO_TEST_ASSETCHAIN_SKIP_POW = 1;
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
         * Test a proof
         */
        uint256 txid = blocks[7].vtx[0].GetHash();
        TxProof proof = GetAssetchainProof(txid);
        SendIPC(E_MARSHAL(ss << txid; ss << proof));
        E_UNMARSHAL(RecvIPC(), ss >> proof);

        std::pair<uint256,NotarisationData> bn;
        if (!GetNextBacknotarisation(proof.first, bn)) {
            printf("GetNextBackNotarisation failed\n");
            return 1;
        }
        if (proof.second.Exec(txid) != bn.second.MoMoM) {
            printf("MoMom incorrect\n");
            return 1;
        }
        return 0;
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
                uint256 destNotarisationTxid;
                n.MoMoM = CalculateProofRoot(n.symbol, 2, height, moms, destNotarisationTxid);
            }
            n.IsBackNotarisation = 1;
            SendIPC(E_MARSHAL(ss << n));
        }

        /*
         * Extend proof
         */
        TxProof proof;
        uint256 txid;
        // Extend proof to MoMoM
        assert(E_UNMARSHAL(RecvIPC(), ss >> txid; ss >> proof));
        proof = GetCrossChainProof(txid, (char*)"PIZZA", 2, proof);
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

}


} /* namespace TestCrossChainProof */
