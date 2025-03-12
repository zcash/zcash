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

#include <amount.h>
#include <chainparams.h>
#include <hash.h>
#include <primitives/block.h>
#include <util/strencodings.h>
#include <util/system.h>

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

const unsigned int THIS_SIDECHAIN = 255;


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

bool DrivechainRPCGetDeposits(std::vector<DrivechainDeposit>& vDeposit)
{
    // TODO how do we ask the enforcer only for new deposits we didn't 
    // receive yet?
    //
    // TODO how do we verify deposits with the enforcer?
    //
    // TODO We cannot use http rpc to ask enforcer for a list of deposits,
    // so this will return true and a vector of test deposits  until we 
    // have grpc support here or the enforcer adds http rpc support.
    

    // Vector of test deposits

    DrivechainDeposit d1;
    d1.strDest = "1Test";
    d1.amount = CAmount(1 * COIN);
    d1.mtx = CMutableTransaction();
    d1.nBurnIndex = 1;
    d1.nTx = 1;
    d1.hashMainchainBlock = uint256();

    DrivechainDeposit d2;
    d2.strDest = "2Test";
    d2.amount = CAmount(1 * COIN);
    d2.mtx = CMutableTransaction();
    d2.nBurnIndex = 1;
    d2.nTx = 1;
    d2.hashMainchainBlock = uint256();

    vDeposit = {d1, d2};
    
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


// Deposit validation & DB

// The miner will use this function to ask for a list of new deposits
// that should be added to the next block. It should return a list of
// valid deposits that haven't been paid out yet.
bool GetUnpaidDrivechainDeposits(std::vector<DrivechainDeposit>& vDeposit) 
{
    std::vector<DrivechainDeposit> vDepositNew;
    if (!DrivechainRPCGetDeposits(vDepositNew)) {
        LogPrintf("%s: Failed to request deposits from enforcer!\n", __func__);
        return false;
    }

    // TODO filter / sort deposits ?
    vDeposit= vDepositNew; 

    return true;
}

std::string GenerateDepositAddress(const std::string& strDestIn)
{
    std::string strDepositAddress = "";

    // Append sidechain number
    strDepositAddress += "s";
    strDepositAddress += std::to_string(THIS_SIDECHAIN);
    strDepositAddress += "_";

    // Append destination
    strDepositAddress += strDestIn;
    strDepositAddress += "_";

    // Generate checksum (first 6 bytes of SHA-256 hash)
    std::vector<unsigned char> vch;
    vch.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&strDepositAddress[0], strDepositAddress.size()).Finalize(&vch[0]);
    std::string strHash = HexStr(vch.begin(), vch.end());

    // Append checksum bits
    strDepositAddress += strHash.substr(0, 6);

    return strDepositAddress;
}

bool ParseDepositAddress(const std::string& strAddressIn, std::string& strAddressOut, unsigned int& nSidechainOut)
{
    if (strAddressIn.empty())
        return false;

    // First character should be 's'
    if (strAddressIn.front() != 's')
        return false;

    /* TODO this needs updating to parse new enforcer version

    unsigned int delim1 = strAddressIn.find_first_of("_") + 1;
    unsigned int delim2 = strAddressIn.find_last_of("_");

    if (delim1 == std::string::npos || delim2 == std::string::npos)
        return false;
    if (delim1 >= strAddressIn.size() || delim2 + 1 >= strAddressIn.size())
        return false;

    std::string strSidechain = strAddressIn.substr(1, delim1);
    if (strSidechain.empty())
        return false;

    // Get sidechain number
    try {
        nSidechainOut = std::stoul(strSidechain);
    } catch (...) {
        return false;
    }

    // Check sidechain number is within range
    if (nSidechainOut > 255)
        return false;

    // Get substring without prefix or suffix
    strAddressOut = strAddressIn.substr(delim1, delim2 - delim1);
    if (strAddressOut.empty())
        return false;

    // Get substring without checksum (for generating our checksum)
    std::string strNoCheck = strAddressIn.substr(0, delim2 + 1);
    if (strNoCheck.empty())
        return false;

    // Generate checksum (first 6 bytes of SHA-256 hash)
    std::vector<unsigned char> vch;
    vch.resize(CSHA256::OUTPUT_SIZE);
    CSHA256().Write((unsigned char*)&strNoCheck[0], strNoCheck.size()).Finalize(&vch[0]);
    std::string strHash = HexStr(vch.begin(), vch.end());

    if (strHash.size() != 64)
        return false;

    // Get checksum from address string
    std::string strCheck = strAddressIn.substr(delim2 + 1, strAddressIn.size());
    if (strCheck.size() != 6)
        return false;

    // Compare address checksum with our checksum
    if (strCheck != strHash.substr(0, 6))
        return false;
    */

    return true;
}

bool IsDrivechainDepositScript(const CScript& script)
{
    if (script.size() < 1)
        return false;

    if (script[0] != 0xDC)
        return false;

    return true;
}

bool VerifyDrivechainDeposit(const CTxOut& out)
{
    return true;
}

