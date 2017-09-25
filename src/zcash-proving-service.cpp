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
#if ENABLE_WEBSOCKET
#include <sstream>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#endif
#include <vector>

#include "libsnark/common/profiling.hpp"

using namespace libzcash;

#if ENABLE_ZMQ
std::string DEFAULT_ZMQ_BIND_ADDRESS = "tcp://*:8234";
#endif

#if ENABLE_WEBSOCKET
uint16_t DEFAULT_WS_BIND_PORT = 9002;
typedef websocketpp::server<websocketpp::config::asio> ws_server_t;
#endif

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

#if ENABLE_WEBSOCKET
void on_websocket_message(ZCJoinSplit* p, ws_server_t* s, websocketpp::connection_hdl hdl, ws_server_t::message_ptr msg)
{
    if (msg->get_opcode() == websocketpp::frame::opcode::text) {
        std::cout << "on_websocket message called with hdl: " << hdl.lock().get()
                  << " and text message: " << msg->get_payload()
                  << std::endl;
        return;
    }

    auto payload = msg->get_raw_payload();
    CDataStream ssWitnesses(payload.data(), payload.data() + payload.length(), SER_NETWORK, PROTOCOL_VERSION);

    auto ssProofs = get_proofs(p, ssWitnesses);
    std::vector<unsigned char> serialized(ssProofs.begin(), ssProofs.end());

    try {
        s->send(hdl, serialized.data(), serialized.size(), websocketpp::frame::opcode::binary);
    } catch (const websocketpp::lib::error_code& e) {
        std::cout << "Response failed because: " << e
                  << "(" << e.message() << ")" << std::endl;
    }
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
#if (ENABLE_ZMQ) && (ENABLE_WEBSOCKET)
        std::cerr << "Usage: zcash-proving-service (zmqBindAddress) (wsBindPort)" << std::endl;
        std::cerr << "zmqBindAddress is a ZMQ bind address, default " << DEFAULT_ZMQ_BIND_ADDRESS << std::endl;
        std::cerr << "wsBindPort is a WebSocket bind address, default " << DEFAULT_WS_BIND_PORT << std::endl;
#elif (ENABLE_ZMQ)
        std::cerr << "Usage: zcash-proving-service (bindAddress)" << std::endl;
        std::cerr << "bindAddress is a ZMQ bind address, default " << DEFAULT_ZMQ_BIND_ADDRESS << std::endl;
#else
        std::cerr << "Usage: zcash-proving-service (bindPort)" << std::endl;
        std::cerr << "bindPort is a WebSocket bind address, default " << DEFAULT_WS_BIND_PORT << std::endl;
#endif
        return 1;
    }

    auto p = ZCJoinSplit::Prepared((ZC_GetParamsDir() / "sprout-verifying.key").string(),
                                   (ZC_GetParamsDir() / "sprout-proving.key").string());

    boost::asio::io_service ios;
    auto oneProof = estimate_proof_time(p);

#if (ENABLE_ZMQ) && (ENABLE_WEBSOCKET)
    size_t zmq_addr = 0;
    size_t ws_port = 0;
    if (argc >= 2) {
        zmq_addr = 1;
    }
    if (argc >= 3) {
        ws_port = 2;
    }
#elif (ENABLE_ZMQ)
    size_t zmq_addr = 0;
    if (argc >= 2) {
        zmq_addr = 1;
    }
#else
    size_t ws_port = 0;
    if (argc >= 2) {
        ws_port = 1;
    }
#endif

#if ENABLE_ZMQ
    char publicKey[41];
    char secretKey[41];
    assert(zmq_curve_keypair(publicKey, secretKey) == 0);

    azmq::rep_socket zmq_socket(ios);
    zmq_socket.set_option(azmq::socket::curve_server(1));
    zmq_socket.set_option(azmq::socket::curve_privatekey(secretKey));
    if (zmq_addr > 0) {
        zmq_socket.bind(argv[zmq_addr]);
        std::cout << "Listening with ZMQ on " << argv[zmq_addr] << std::endl;
    } else {
        zmq_socket.bind(DEFAULT_ZMQ_BIND_ADDRESS);
        std::cout << "Listening with ZMQ on " << DEFAULT_ZMQ_BIND_ADDRESS << std::endl;
    }
    std::cout << "Proving service ZMQ public key: " << publicKey << std::endl;

    zmq_receive_request(p, zmq_socket, oneProof);
#endif

#if ENABLE_WEBSOCKET
    ws_server_t ws_server;
    ws_server.init_asio(&ios);
    ws_server.set_message_handler(std::bind(
        &on_websocket_message, p, &ws_server,
        std::placeholders::_1, std::placeholders::_2));
    if (ws_port > 0) {
        std::istringstream ss(argv[ws_port]);
        uint16_t port;
        if (!(ss >> port)) {
            std::cerr << "Invalid port " << argv[ws_port] << '\n';
            return 1;
        }
        ws_server.listen(port);
        std::cout << "Listening with WebSocket on port " << port << std::endl;
    } else {
        ws_server.listen(DEFAULT_WS_BIND_PORT);
        std::cout << "Listening with WebSocket on port " << DEFAULT_WS_BIND_PORT << std::endl;
    }

    ws_server.start_accept();
#endif

    ios.run();
}
