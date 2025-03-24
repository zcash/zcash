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
#include <clientversion.h>
#include <fs.h>
#include <hash.h>
#include <primitives/block.h>
#include <serialize.h>
#include <streams.h>
#include <util/system.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <univalue.h>

#include <deque>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <vector>

#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/filesystem.hpp>
#include <boost/foreach.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

using boost::asio::ip::tcp;

const std::string BITCOIN_RPC_USER = "user"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_PASS = "password"; // TODO currently hardcoded by enforcer
const std::string BITCOIN_RPC_HOST = "127.0.0.1";
const int BITCOIN_RPC_PORT = 38332;

const unsigned int THIS_SIDECHAIN = 255;

struct MainBlockCacheIndex
{
    size_t index;
    uint256 hash;
};

// Index of mainchain block hash in vMainBlockHash
std::map<uint256 /* hashMainchainBlock */, MainBlockCacheIndex> mapMainBlock;

// List of all known mainchain block hashes in order
std::vector<uint256> vMainBlockHash;

// Sidechain block hashes we already verified with the enforcer
std::set<uint256 /* hashBlock */> setBMMVerified;

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

bool DrivechainRPCGetBTCBlockHash(const int& nHeight, uint256& hash)
{
    // JSON for 'getblockhash' mainchain HTTP-RPC
    std::string json;
    json.append("{\"jsonrpc\": \"1.0\", \"id\":\"Drivechain\", ");
    json.append("\"method\": \"getblockhash\", \"params\": ");
    json.append("[");
    json.append(UniValue(nHeight).write());
    json.append("] }");

    // Try to request mainchain block hash
    boost::property_tree::ptree ptree;
    if (!RPCBitcoinPatched(json, ptree)) {
        LogPrintf("ERROR Sidechain client failed to request block hash!\n");
        return false;
    }

    std::string strHash = ptree.get("result", "");
    hash = uint256S(strHash);

    return (!hash.IsNull());
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
    // TODO
    // The enforcer doesn't give a list of sidechain deposits like the
    // deprecated L1. Instead we will use GetTwoWayPegData or GetBlockInfo
    // for every L1 block to collect valid deposits and store them.
    //
    // Then we will need a function that the miner can use to include 
    // any unpaid deposits when creating new blocks
    //
    // TODO how are they ordered? 
    //
    // TODO can we verify a specific deposit with the enforcer?
    //
    // TODO 
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

bool ReadBMMCache()
{
    fs::path path = GetDataDir() / "bmm.dat";
    CAutoFile filein(fsbridge::fopen(path, "rb"), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return false;
    }

    try {
        int nVersionRequired, nVersionThatWrote;
        filein >> nVersionRequired;
        filein >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION) {
            return false;
        }

        int nBMM = 0;
        filein >> nBMM;
        for (int i = 0; i < nBMM; i++) {
            uint256 hash;
            filein >> hash;
            CacheVerifiedBMM(hash);
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error reading BMM cache: %s", __func__, e.what());
        return false;
    }

    return true;
}

bool WriteBMMCache()
{
    int nBMM = setBMMVerified.size();

    fs::path path = GetDataDir() / "bmm.dat.new";
    CAutoFile fileout(fsbridge::fopen(path, "wb"), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        return;
    }

    try {
        fileout << 160000; // version required to read: 0.16.00 or later
        fileout << CLIENT_VERSION; // version that wrote the file

        // Verified BMM hash cache
        fileout << nBMM; // Number of Withdrawal Bundle hashes in file
        for (const uint256& u : setBMMVerified) {
            fileout << u;
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error writing BMM cache: %s", __func__, e.what());
        return false;
    }

    FileCommit(fileout.Get());
    fileout.fclose();
    RenameOver(GetDataDir() / "bmm.dat.new", GetDataDir() / "bmm.dat");

    LogPrintf("%s: Wrote BMM cache.\n", __func__);

    return true;
}

bool HaveVerifiedBMM(const uint256& hashBlock)
{
    if (hashBlock.IsNull())
        return false;

    return (setBMMVerified.count(hashBlock));
}

void CacheVerifiedBMM(const uint256& hashBlock)
{
    if (hashBlock.IsNull())
        return;

    setBMMVerified.insert(hashBlock);
}

bool VerifyBMM(const CBlock& block)
{
    // Skip genesis block
    if (block.GetHash() == Params().GetConsensus().hashGenesisBlock)
        return true;

    // Have we already verified BMM for this block?
    if (HaveVerifiedBMM(block.GetHash()))
        return true;

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

    // Cache that we have verified BMM for this block
    CacheVerifiedBMM(block.GetHash());

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
    vDeposit = vDepositNew; 

    return true;
}

std::string GenerateDepositAddress(const std::string& strDestIn)
{
    // TODO this needs to be updated to match current enforcer version
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

bool ReadMainBlockCache()
{
    fs::path path = GetDataDir() / "mainblockhash.dat";
    CAutoFile filein(fsbridge::fopen(path, "rb"), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull()) {
        return false;
    }

    std::vector<uint256> vHash;
    try {
        int nVersionRequired, nVersionThatWrote;
        filein >> nVersionRequired;
        filein >> nVersionThatWrote;
        if (nVersionRequired > CLIENT_VERSION) {
            return false;
        }

        int count = 0;
        filein >> count;
        for (int i = 0; i < count; i++) {
            uint256 hash;
            filein >> hash;
            vHash.push_back(hash);
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error reading main block cache: %s", __func__, e.what());
        return false;
    }

    for (const uint256& u : vHash)
        CacheMainBlockHash(u);

    return true;
}

bool WriteMainBlockCache()
{
    if (vMainBlockHash.empty())
        return false;

    int count = vMainBlockHash.size();

    fs::path path = GetDataDir() / "mainblockhash.dat.new";
    CAutoFile fileout(fsbridge::fopen(path, "wb"), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull()) {
        return false;
    }

    try {
        fileout << 160000; // version required to read: 0.16.00 or later
        fileout << CLIENT_VERSION; // version that wrote the file
        fileout << count; // Number of Withdrawal Bundle hashes in file

        for (const uint256& u : vMainBlockHash) {
            fileout << u;
        }
    }
    catch (const std::exception& e) {
        LogPrintf("%s: Error writing main block cache: %s", __func__, e.what());
        return false;
    }

    FileCommit(fileout.Get());
    fileout.fclose();
    RenameOver(GetDataDir() / "mainblockhash.dat.new", GetDataDir() / "mainblockhash.dat");

    LogPrintf("%s: Wrote %u\n", __func__, count);

    return true;
}

void CacheMainBlockHash(const uint256& hash)
{
    // Don't re-cache the genesis block
    if (vMainBlockHash.size() == 1 && hash == vMainBlockHash.front())
        return;

    // Add to ordered vector of main block hashes
    vMainBlockHash.push_back(hash);

    // Add to index of hashes
    MainBlockCacheIndex index;
    index.hash = hash;
    index.index = vMainBlockHash.size() - 1;

    mapMainBlock[hash] = index;
}

bool HaveMainBlock(const uint256& hash)
{
    return mapMainBlock.count(hash);
}

bool UpdateMainBlockHashCache(bool& fReorg, std::vector<uint256>& vDisconnected)
{
    // TODO 
    // This function will detect mainchain reorgs based on the mainchain block
    // hash cache, but we shouldn't do anything if we detect a reorg until the
    // enforcer is updated to handle them.
    //
    // Note: bitcoin core does not count genesis block towards block count but
    // we will cache it.
    //

    // Get the current mainchain block height
    int nMainBlocks = 0;
    if (DrivechainRPCGetBTCBlockCount(nMainBlocks)) {
        LogPrintf("%s: Failed to update - cannot get block count from mainchain. (connection issue?)\n", __func__);
        return false;
    }

    uint256 hashMainTip;
    if (DrivechainRPCGetBTCBlockHash(nMainBlocks, hashMainTip)) {
        LogPrintf("%s: Failed to get to mainchain tip block hash!\n", __func__);
        return false;
    }

    // If the block height hasn't changed, check that if cached chain tip is the
    // same as the current mainchain tip. If it is we don't need to do anything
    // else. If it isn't we will continue to update / reorg handling.
    int nCachedBlocks = vMainBlockHash.size();
    if (nMainBlocks + 1 == nCachedBlocks && vMainBlockHash.size() && vMainBlockHash.back() == hashMainTip) {
        return true;
    }

    // Otherwise;
    // From the new mainchain tip, start looping back through mainchain blocks
    // while keeping track of them in order until we find one that connects to
    // one of our cached blocks by prevblock.
    uint256 hashPrevBlock;
    std::deque<uint256> deqHashNew;
    for (int i = nMainBlocks; i > 0; i--) {
        // Get the prevblockhash
        if (!DrivechainRPCGetBTCBlockHash(i - 1, hashPrevBlock)) {
            LogPrintf("%s: Failed to get to mainchain block: %u\n", __func__, i - 1);
            return false;
        }

        // Check if the prevblock is in our cache. Once we find a prevblock in
        // our cache we can update our cache from that block up to the new
        // mainchain tip.
        if (HaveMainBlock(hashPrevBlock)) {
            deqHashNew.push_front(hashPrevBlock);
            break;
        }

        deqHashNew.push_front(hashPrevBlock);
    }
    // Also add the new mainchain tip
    deqHashNew.push_back(hashMainTip);

    if (deqHashNew.empty()) {
        LogPrintf("%s: Error - called with empty list of new block hashes!\n", __func__);
        return false;
    }

    // If the main block cache doesn't have the genesis block yet, add it first
    if (vMainBlockHash.empty())
        CacheMainBlockHash(deqHashNew.front());

    // Figure out the block in our cache that we will append the new blocks to
    MainBlockCacheIndex index;
    if (mapMainBlock.count(deqHashNew.front())) {
        index = mapMainBlock[deqHashNew.front()];
    } else {
        LogPrintf("%s: Error - New blocks do not connect to cached chain!\n", __func__);
        return false;
    }

    // If there were any blocks in our cache after the block we will be building
    // on, remove them, add them to vOrphan as they were disconnected, set
    // fReorg true.
    if (index.index != vMainBlockHash.size() - 1) {
        LogPrintf("%s: Mainchain reorg detected!\n", __func__);
        fReorg = true;
    }

    std::vector<uint256> vOrphan;

    for (size_t i = vMainBlockHash.size() - 1; i > index.index; i--) {
        vOrphan.push_back(vMainBlockHash[i]);
        vMainBlockHash.pop_back();
    }

    // Remove disconnected blocks from index map
    for (const uint256& u : vOrphan)
        mapMainBlock.erase(u);

    // Check if we already know the first block in the deque and remove it if
    // we do.
    if (HaveMainBlock(deqHashNew.front()))
        deqHashNew.pop_front();

    // Append new blocks
    for (const uint256& u : deqHashNew)
        CacheMainBlockHash(u);

    LogPrintf("%s: Updated cached mainchain tip to: %s.\n", __func__, deqHashNew.back().ToString());

    return true;
}

bool VerifyMainBlockCache(std::string& strError)
{
    if (!vMainBlockHash.size()) {
        strError = "No mainchain blocks in cache!";
        return false;
    }

    // Compare cached hash at height with mainchain block hash at height
    for (size_t i = 0; i < vMainBlockHash.size(); i++) {
        uint256 hashBlock;

        if (!DrivechainRPCGetBTCBlockHash(i, hashBlock)) {
            strError = "Failed to request mainchain block hash!";
            return false;
        }

        if (hashBlock != vMainBlockHash[i]) {
            strError = "Invalid hash cached: ";
            strError += vMainBlockHash[i].ToString();
            strError += " height: ";
            strError += std::to_string(i);

            return false;
        }
    }

    return true;
}

void HandleMainchainReorg(const std::vector<uint256>& vOrphan)
{
    // TODO unused, waiting for enforcer to handle reorgs 
    /* This might need to move to main so that we can handle best
     * chain activation... ???
     *

    // For mainchain blocks that were orphaned - invalidate bmm blocks with
    // commitments from them.
    //
    // vOrphan contains a list of mainchain block hashes that were orphaned

    // Before invalidating any blocks, check the sanity of the mainchain block
    // cache and then verify that the blocks to be orphaned actually are missing
    // from the mainchain.

    // Check the mainchain block cache
    std::string strError = "";
    if (!VerifyMainBlockCache(strError)) {
        LogPrintf("%s: Main block cache invalid: %s. Resyncing...\n",
                __func__, strError);
        // Reset the mainchain block cache and then re-sync it
        bmmCache.ResetMainBlockCache();

        // TODO
        // If during this call a reorg is detected and we have more orphans then
        // something bad happened and needs to be handled. Since we just reset
        // the mainchain block cache, have a mutex lock, and are updating the
        // cache from scratch now, it should be impossible.
        bool fReorg = false;
        std::vector<uint256> vOrphanIgnore;
        if (!UpdateMainBlockHashCache(fReorg, vOrphanIgnore)) {
            // TODO
            // If we make it to this point there might be a connection issue or
            // something going on. Maybe the mainchain node went down during the
            // function? There might be something better to do than just logging
            // the error here.
            LogPrintf("%s: Failed to re-update main block cache after reset!",
                    __func__);
            return;
        }
    }

    // Check that the alleged orphans actually don't exist on the mainchain
    std::vector<uint256> vOrphanFinal;
    for (const uint256& u : vOrphan) {
        if (!bmmCache.HaveMainBlock(u))
            vOrphanFinal.push_back(u);
    }

    // Check if any BMM blocks were created from commitments in this
    // orphaned mainchain block
    for (const uint256& u : vOrphanFinal) {
        CValidationState state;
        {
            LOCK(cs_main);
            // Check our map of blocks based on their mainchain BMM commit block
            if (!mapBlockMainHashIndex.count(u))
                continue;

            CBlockIndex* pindex = mapBlockMainHashIndex[u];
            if (!chainActive.Contains(pindex))
                continue;

            InvalidateBlock(state, Params(), pindex);

            LogPrintf("%s: Invalidated block: %s because mainchain block: %s was orphaned!\n",
                    __func__, pindex->GetBlockHash().ToString(), u.ToString());

            if (!state.IsValid()) {
                LogPrintf("%s: Error while invalidating blocks: %s\n",
                        __func__, FormatStateMessage(state));
                return;
            }
        }

        ActivateBestChain(state, Params());
        if (!state.IsValid()) {
            LogPrintf("%s: Error activating best chain: %s\n",
                    __func__, FormatStateMessage(state));
            return;
        }
    }
*/
}

