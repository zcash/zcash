// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"
#include "streams.h"
#include "util.h"
#include "version.h"
#include "zcash/JoinSplit.hpp"
#include "zcash/Proof.hpp"

#if ENABLE_ZMQ
#include <azmq/socket.hpp>
#endif
#include <boost/asio.hpp>
#include <vector>

#include "libsnark/common/profiling.hpp"

using namespace libzcash;

std::string DEFAULT_ZMQ_BIND_ADDRESS = "tcp://*:8234";

uint32_t estimate_proof_time(ZCJoinSplit* p)
{
    std::cout << "Measuring time to compute one proof…" << std::flush;
    struct timeval tv_start;
    gettimeofday(&tv_start, 0);

    // Compute a dummy proof
    uint256 anchor = ZCIncrementalMerkleTree().root();
    uint256 pubKeyHash;
    JSDescription jsdesc(*p,
                         pubKeyHash,
                         anchor,
                         {JSInput(), JSInput()},
                         {JSOutput(), JSOutput()},
                         0,
                         0);

    struct timeval tv_end;
    gettimeofday(&tv_end, 0);
    uint32_t elapsed = tv_end.tv_sec - tv_start.tv_sec;
    std::cout << " " << elapsed << " seconds" << std::endl;
    return elapsed;
}

CDataStream get_estimate(CDataStream ssRequest, uint32_t oneProof)
{
    uint32_t numProofs;
    ssRequest >> numProofs;
    std::cout << "Received request for " << numProofs << " proofs!" << std::endl;

    // Estimate callback
    uint32_t estimate = oneProof * numProofs;
    std::cout << "- Estimated proving time: " << estimate << " seconds" << std::endl;

    CDataStream ssConfirm(SER_NETWORK, PROTOCOL_VERSION);
    ssConfirm << estimate;
    return ssConfirm;
}

CDataStream get_proofs(ZCJoinSplit* p, CDataStream ssWitnesses)
{
    std::vector<ZCJSProofWitness> witnesses;
    ssWitnesses >> witnesses;
    std::cout << "Received " << witnesses.size() << " witnesses!" << std::endl;

    std::cout << "- Running prover…" << std::flush;
    std::vector<ZCProof> proofs;
    for (auto witness : witnesses) {
        proofs.push_back(p->prove(witness));
        std::cout << "…" << std::flush;
    }
    std::cout << " Done!" << std::endl;

    CDataStream ssProofs(SER_NETWORK, PROTOCOL_VERSION);
    ssProofs << proofs;
    return ssProofs;
}

CDataStream handle_request(ZCJoinSplit* p, CDataStream ssRequest, uint32_t oneProof)
{
    uint32_t protoVersion;
    ssRequest >> protoVersion;
    assert(protoVersion == 0);

    CDataStream ssResponse(SER_NETWORK, PROTOCOL_VERSION);
    ssResponse << protoVersion;

    uint8_t msgType;
    ssRequest >> msgType;
    if (msgType == 0) {
        ssResponse << get_estimate(ssRequest, oneProof);
    } else if (msgType == 1) {
        ssResponse << get_proofs(p, ssRequest);
    } else {
        // Error
    }

    return ssResponse;
}

#if ENABLE_ZMQ
void zmq_receive_request(ZCJoinSplit* p, azmq::rep_socket& socket, uint32_t oneProof)
{
    socket.async_receive([p, &socket, oneProof](boost::system::error_code const& ec, azmq::message& request, size_t bytes_transferred) {
        const char* data = (const char*)request.data();
        CDataStream ssRequest(data, data + request.size(), SER_NETWORK, PROTOCOL_VERSION);

        auto ssResponse = handle_request(p, ssRequest, oneProof);

        std::vector<unsigned char> serialized(ssResponse.begin(), ssResponse.end());
        socket.send(boost::asio::buffer(serialized));

        zmq_receive_request(p, socket, oneProof);
    });
}
#endif

int main(int argc, char* argv[])
{
    SetupEnvironment();

    libsnark::inhibit_profiling_info = true;
    libsnark::inhibit_profiling_counters = true;

    if (argc > 1 && argv[1][0] == '-' &&
        (argv[1][1] == '?' || argv[1][1] == 'h' || (argv[1][1] == '-' && argv[1][2] == 'h' && argv[1][3] == 'e' &&
                                                    argv[1][4] == 'l' && argv[1][5] == 'p'))) {
        std::cerr << "Usage: zcash-proving-service (bindAddress)" << std::endl;
        std::cerr << "bindAddress is a ZMQ bind address, default " << DEFAULT_ZMQ_BIND_ADDRESS << std::endl;
        return 1;
    }

    auto p = ZCJoinSplit::Prepared((ZC_GetParamsDir() / "sprout-verifying.key").string(),
                                   (ZC_GetParamsDir() / "sprout-proving.key").string());

    boost::asio::io_service ios;
    auto oneProof = estimate_proof_time(p);

#if ENABLE_ZMQ
    char publicKey[41];
    char secretKey[41];
    assert(zmq_curve_keypair(publicKey, secretKey) == 0);

    azmq::rep_socket zmq_socket(ios);
    zmq_socket.set_option(azmq::socket::curve_server(1));
    zmq_socket.set_option(azmq::socket::curve_privatekey(secretKey));
    if (argc >= 2) {
        zmq_socket.bind(argv[1]);
        std::cout << "Listening with ZMQ on " << argv[1] << std::endl;
    } else {
        zmq_socket.bind(DEFAULT_ZMQ_BIND_ADDRESS);
        std::cout << "Listening with ZMQ on " << DEFAULT_ZMQ_BIND_ADDRESS << std::endl;
    }
    std::cout << "Proving service ZMQ public key: " << publicKey << std::endl;

    zmq_receive_request(p, zmq_socket, oneProof);
#endif

    ios.run();
}
