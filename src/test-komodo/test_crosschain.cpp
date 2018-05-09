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
#include "main.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "script/cc.h"
#include "script/interpreter.h"
#include "script/serverchecker.h"
#include "txmempool.h"
#include "crosschain.h"

#include "testutils.h"


extern uint256 komodo_calcMoM(int32_t height,int32_t MoMdepth);


/*
 * Tests for the whole process of creating and validating notary proofs
 * using proof roots (MoMoMs). This is to support coin imports.
 */

namespace TestCrossChainProof {


class TestCrossChain : public ::testing::Test, public Eval {
public:
    bool CheckNotaryInputs(const CTransaction &tx, uint32_t height, uint32_t timestamp) const
    {
        NotarisationData data;
        return ParseNotarisationOpReturn(tx, data);
    }
protected:
    static void SetUpTestCase() { }
    virtual void SetUp() {
        ASSETCHAINS_CC = 1;
        EVAL_TEST = this;
    }
};



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
    setupChain(); 
    std::vector<CBlock> blocks;
    blocks.resize(10);
    NotarisationData a2kmd, kmd2a(true);


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
        NotarisationData back(1);
        strcpy(ASSETCHAINS_SYMBOL, "symbolA");
        strcpy(a2kmd.symbol, "symbolA");
        a2kmd.ccId = 2;
        
        /*
         * Notarisation 1
         */
        generateBlock(&blocks[1]);
        generateBlock(&blocks[2]);
        a2kmd.blockHash = blocks[2].GetHash();
        a2kmd.MoM = komodo_calcMoM(a2kmd.height = chainActive.Height(), a2kmd.MoMDepth = 2);
        SendIPC(E_MARSHAL(ss << a2kmd));
        E_UNMARSHAL(RecvIPC(), ss >> back);
        RecordNotarisation(blocks[1].vtx[0], back);

        /*
         * Notarisation 2
         */
        generateBlock(&blocks[3]);
        generateBlock(&blocks[4]);
        a2kmd.blockHash = blocks[4].GetHash();
        a2kmd.MoM = komodo_calcMoM(a2kmd.height = chainActive.Height(), a2kmd.MoMDepth = 2);
        SendIPC(E_MARSHAL(ss << a2kmd));
        E_UNMARSHAL(RecvIPC(), ss >> back);
        RecordNotarisation(blocks[3].vtx[0], back);

        /*
         * Generate proof
         */
        generateBlock(&blocks[5]);
        uint256 txid = blocks[3].vtx[0].GetHash();
        std::pair<uint256,MerkleBranch> assetChainProof = GetAssetchainProof(txid);
        SendIPC(E_MARSHAL(ss << txid; ss << assetChainProof));
    };

    auto RunTestKmd = [&] ()
    {
        NotarisationData n;

        /*
         * Notarisation 1
         */
        E_UNMARSHAL(RecvIPC(), ss >> n);
        // Grab a coinbase input to fund notarisation
        generateBlock(&blocks[1]);
        n.txHash = RecordNotarisation(blocks[1].vtx[0], a2kmd);
        n.height = chainActive.Height();
        SendIPC(E_MARSHAL(ss << n));

        /*
         * Notarisation 2
         */
        E_UNMARSHAL(RecvIPC(), ss >> n);
        // Grab a coinbase input to fund notarisation
        generateBlock(&blocks[2]);
        n.txHash = RecordNotarisation(blocks[2].vtx[0], a2kmd);
        n.height = chainActive.Height();
        SendIPC(E_MARSHAL(ss << n));

        /*
         * Extend proof
         */
        std::pair<uint256,MerkleBranch> assetChainProof;
        uint256 txid;
        // Extend proof to MoMoM
        assert(E_UNMARSHAL(RecvIPC(), ss >> txid; ss >> kmd2a));
        std::pair<uint256,MerkleBranch> ccProof = GetCrossChainProof(txid, (char*)"symbolA",
            2, assetChainProof.first, assetChainProof.second);
    };

    const char endpoint[] = "ipc://tmpKomodoTestCrossChainSock";

    if (!childPid) {
        assert(0 == zmq_connect(socket, endpoint));
        usleep(20000);
        RunTestAssetchain();
        exit(0);
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

    *
     * Assetchain notarisation 2
     *

    ON_ASSETCHAIN {
        a2kmd.blockHash = blocks[4].GetHash();
        a2kmd.MoM = komodo_calcMoM(a2kmd.height = chainActive.Height(), a2kmd.MoMDepth = 2);
        SendIPC(E_MARSHAL(ss << a2kmd));
    }

    ON_KMD {
        assert(E_UNMARSHAL(RecvIPC(), ss >> a2kmd));
        // Grab a coinbase input to fund notarisation
        RecordNotarisation(blocks[2].vtx[0], a2kmd);
    }

    generateBlock(&blocks[5]);
    generateBlock(&blocks[6]);

    *
     * Backnotarisation
     * 
     * This is what will contain the MoMoM which allows us to prove across chains
     *
    std::vector<uint256> moms;
    int assetChainHeight;

    ON_KMD {
        memset(kmd2a.txHash.begin(), 1, 32);  // Garbage but non-null
        kmd2a.symbol[0] = 0;  // KMD
        kmd2a.MoMoM = GetProofRoot((char*)"symbolA", 2, chainActive.Height(), moms, &assetChainHeight);
        kmd2a.MoMoMDepth = 0; // Needed?
        SendIPC(E_MARSHAL(ss << kmd2a));
    }

    ON_ASSETCHAIN {
        assert(E_UNMARSHAL(RecvIPC(), ss >> kmd2a));
        RecordNotarisation(blocks[1].vtx[0], kmd2a);
    }


    *
     * We can now prove a tx from A on A, via a merkle root backpropagated from KMD.
     * 
     * The transaction that we'll try to prove is the coinbase from the 3rd block.
     * We should be able to start with only that transaction ID, and generate a merkle
     * proof.
     *

    std::pair<uint256,MerkleBranch> assetChainProof;
    uint256 txid;

    ON_ASSETCHAIN {
        txid = blocks[2].vtx[0].GetHash();

        // First thing to do is get the proof from the assetchain
        assetChainProof = GetAssetchainProof(txid);
        SendIPC(E_MARSHAL(ss << txid; ss << assetChainProof));
    }

    ON_KMD {
        // Extend proof to MoMoM
        assert(E_UNMARSHAL(RecvIPC(), ss >> txid; ss >> kmd2a));
        std::pair<uint256,MerkleBranch> ccProof = GetCrossChainProof(txid, (char*)"symbolA",
            2, assetChainProof.first, assetChainProof.second);
    }

    */
}


} /* namespace TestCrossChainProof */
