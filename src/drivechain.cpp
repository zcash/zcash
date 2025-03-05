// Copyright (c) 2009-2025 Satoshi Nakamoto
// Copyright (c) 2009-2025 The Bitcoin Core developers
// Copyright (c) 2015-2025 The Zcash developers
// Copyright (c) 2025 L2L
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .
//
// Whenever possible any modifications to support Drivechain should be
// in this file to make rebasing as simple as possible. Some modifications
// (chainparams, mining etc) must modify existing functions but that should
// be avoided. Maybe it would be nice to seperate things into more additional
// files but the goal is maximizing rebase simplicity.
//
// This file should include:
// Constant values related to drivechain
// Client(s)? for talking to enforcer and bitcoin patched
// BMM validation and cache
// Deposit validation
// Bitcoin chain tracking, reorg detection etc
// Withdrawal cache and validation

#include <drivechain.h>

#include <chainparams.h>
#include <primitives/block.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <uint256.h>

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::asio::ip::tcp;

const std::string BITCOIN_RPC_USER = "user"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_PASS = "password"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_HOST = "127.0.0.1";
const int BITCOIN_RPC_PORT = 38332;


// Bitcoin-patched RPC client:


bool RPCBitcoinPatched(const std::string& json, boost::property_tree::ptree &ptree)
{
    // Format user:pass for authentication
    std::string auth = BITCOIN_RPC_USER + ":" + BITCOIN_RPC_PASS;
    if (auth == ":")
        return false;

    try {
        // Setup BOOST ASIO for a synchronus call to the mainchain
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::socket socket(io_service);
        boost::system::error_code error;
        socket.connect(boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::address::from_string(BITCOIN_RPC_HOST), BITCOIN_RPC_PORT), error);

        if (error) throw boost::system::system_error(error);

        // HTTP request (package the json for sending)
        boost::asio::streambuf output;
        std::ostream os(&output);
        os << "POST / HTTP/1.1\n";
        os << "Host: 127.0.0.1\n";
        os << "Content-Type: application/json\n";
        os << "Authorization: Basic " << EncodeBase64(auth) << std::endl;
        os << "Connection: close\n";
        os << "Content-Length: " << json.size() << "\n\n";
        os << json;

        // Send the request
        boost::asio::write(socket, output);

        // Read the reponse
        std::string data;
        for (;;)
        {
            boost::array<char, 256> buf;

            // Read until end of file (socket closed)
            boost::system::error_code e;
            size_t sz = socket.read_some(boost::asio::buffer(buf), e);

            data.insert(data.size(), buf.data(), sz);

            if (e == boost::asio::error::eof)
                break; // socket closed
            else if (e)
                throw boost::system::system_error(e);
        }

        std::stringstream ss;
        ss << data;

        // Get response code
        ss.ignore(std::numeric_limits<std::streamsize>::max(), ' ');
        int code;
        ss >> code;

        // Check response code
        if (code != 200)
            return false;

        // Skip the rest of the header
        for (size_t i = 0; i < 5; i++)
            ss.ignore(std::numeric_limits<std::streamsize>::max(), '\r');

        // Parse json response;
        std::string JSON;
        ss >> JSON;
        std::stringstream jss;
        jss << JSON;
        boost::property_tree::json_parser::read_json(jss, ptree);
    } catch (std::exception &exception) {
        LogPrintf("ERROR Sidechain client at %s:%d (sendRequestToMainchain): %s\n", BITCOIN_RPC_HOST, BITCOIN_RPC_PORT, exception.what());
        return false;
    }
    return true;
}

bool DrivechainRPCGetBTCBlockCount(int& nBlocks)
{
    // JSON for 'getblockcount' bitcoin HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"Drivechain\", ");
    json.append("\"method\": \"getblockcount\", \"params\": ");
    json.append("[] }");

    // Try to request mainchain block count
    boost::property_tree::ptree ptree;
    if (!RPCBitcoinPatched(json, ptree)) {
        LogPrintf("ERROR failed to request block count\n");
        return false;
    }

    // Process result
    nBlocks = ptree.get("result", 0);

    return nBlocks >= 0;
}

bool DrivechainRPCVerifyBMM(const uint256& hashMainBlock, const uint256& hashHStar, uint256& txid, int nTime)
{
    // TODO We cannot use http rpc to ask enforcer to verify BMM,
    // so this will return true until we have grpc support here or
    // the enforcer adds http rpc support.
    return true;
}


// BMM validation & cache:


bool VerifyBMM(const CBlock& block)
{
    // Skip genesis block
    if (block.GetHash() == Params().GetConsensus().hashGenesisBlock)
        return true;

    // TODO
    // Have we already verified BMM for this block?
    //if (bmmCache.HaveVerifiedBMM(block.GetHash()))
    //    return true;

    // h*
    const uint256 hashMerkleRoot = block.hashMerkleRoot;

    // Mainchain block hash
    const uint256 hashMainBlock = uint256(); // TODO block.hashMerkleRoot;

    // Verify BMM with local mainchain node
    uint256 txid;
    uint32_t nTime;
    if (!DrivechainRPCVerifyBMM(hashMainBlock, hashMerkleRoot, txid, nTime)) {
        LogPrintf("%s: Did not find BMM h*: %s in mainchain block: %s!\n", __func__, hashMerkleRoot.ToString(), hashMainBlock.ToString());
        return false;
    }

    // TODO
    // Cache that we have verified BMM for this block
    // bmmCache.CacheVerifiedBMM(block.GetHash());

    return true;
}

