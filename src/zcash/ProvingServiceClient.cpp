// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "ProvingServiceClient.hpp"

#include "../util.h"
#include "streams.h"
#include "version.h"

#include <zmq.hpp>

using namespace libzcash;

ProvingServiceClient::ProvingServiceClient(std::string id, char* url, char* serverKey) : log_id(id), zmqUrl(url)
{
    assert(strlen(serverKey) == 40);
    std::memcpy(zmqServerKey, serverKey, 41);
    assert(zmq_curve_keypair(zmqPublicKey, zmqSecretKey) == 0);
}

boost::optional<std::vector<libzcash::ZCProof>> ProvingServiceClient::get_proofs(std::vector<ZCJSProofWitness> witnesses)
{
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_REQ);
    socket.setsockopt(ZMQ_LINGER, 0);
    socket.setsockopt(ZMQ_CURVE_SERVERKEY, zmqServerKey);
    socket.setsockopt(ZMQ_CURVE_PUBLICKEY, zmqPublicKey);
    socket.setsockopt(ZMQ_CURVE_SECRETKEY, zmqSecretKey);
    socket.connect(zmqUrl);

    uint32_t protoVersion = 0;
    std::vector<zmq::pollitem_t> items = {{socket, 0, ZMQ_POLLIN, 0}};
    uint32_t estimate;
    {
        CDataStream ssRequest(SER_NETWORK, PROTOCOL_VERSION);
        uint32_t numWitnesses = witnesses.size();
        ssRequest << protoVersion;
        ssRequest << (uint8_t)0;
        ssRequest << numWitnesses;
        std::vector<unsigned char> serialized(ssRequest.begin(), ssRequest.end());

        zmq::message_t request(serialized.size());
        memcpy(request.data(), serialized.data(), serialized.size());
        LogPrint("zrpc", "%s: Requesting estimate from proving service for %d proofs\n", log_id, numWitnesses);
        socket.send(request);

        zmq::message_t reply;
        LogPrint("zrpc", "%s: Waiting for time estimate…\n", log_id);
        // 10s timeout on initial response
        zmq::poll(items, 10 * 1000);
        if (items[0].revents & ZMQ_POLLIN) {
            socket.recv(&reply);
            const char* data = (const char*)reply.data();
            CDataStream ssConfirm(data, data + reply.size(), SER_NETWORK, PROTOCOL_VERSION);
            uint32_t confirmVersion;
            ssConfirm >> confirmVersion;
            assert(confirmVersion == protoVersion);
            ssConfirm >> estimate;
            LogPrint("zrpc", "%s: Time estimate: %d seconds\n", log_id, estimate);
        } else {
            LogPrint("zrpc", "%s: ERROR: Timeout waiting for time estimate");
            return boost::none;
        }
    }

    CDataStream ssWitnesses(SER_NETWORK, PROTOCOL_VERSION);
    ssWitnesses << protoVersion;
    ssWitnesses << (uint8_t)1;
    ssWitnesses << witnesses;
    std::vector<unsigned char> serialized(ssWitnesses.begin(), ssWitnesses.end());

    zmq::message_t request(serialized.size());
    memcpy(request.data(), serialized.data(), serialized.size());
    LogPrint("zrpc", "%s: Sending witnesses to proving service %s\n", log_id, zmqUrl);
    socket.send(request);

    zmq::message_t reply;
    LogPrint("zrpc", "%s: Waiting for proofs…\n", log_id);
    // Add 5% margin to timeout for overhead
    zmq::poll(items, estimate * 1050);
    if (items[0].revents & ZMQ_POLLIN) {
        socket.recv(&reply);
        LogPrint("zrpc", "%s: Proofs received\n", log_id);
        socket.close();
    } else {
        LogPrint("zrpc", "%s: ERROR: Timeout waiting for time estimate");
        return boost::none;
    }

    const char* data = (const char*)reply.data();
    CDataStream ssProofs(data, data + reply.size(), SER_NETWORK, PROTOCOL_VERSION);
    uint32_t proofsVersion;
    std::vector<ZCProof> proofs;
    ssProofs >> proofsVersion;
    assert(proofsVersion == protoVersion);
    ssProofs >> proofs;

    return proofs;
}
