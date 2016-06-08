// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "protocol.h"

#include "util.h"
#include "utilstrencodings.h"

#ifndef WIN32
# include <arpa/inet.h>
#endif

CMessageHeader::CMessageHeader(const MessageStartChars& pchMessageStartIn)
{
    memcpy(pchMessageStart, pchMessageStartIn, MESSAGE_START_SIZE);
    memset(pchCommand, 0, sizeof(pchCommand));
    nMessageSize = -1;
    memset(pchChecksum, 0, CHECKSUM_SIZE);
}

CMessageHeader::CMessageHeader(const MessageStartChars& pchMessageStartIn, const char* pszCommand, unsigned int nMessageSizeIn)
{
    memcpy(pchMessageStart, pchMessageStartIn, MESSAGE_START_SIZE);
    memset(pchCommand, 0, sizeof(pchCommand));
    strncpy(pchCommand, pszCommand, COMMAND_SIZE);
    nMessageSize = nMessageSizeIn;
    memset(pchChecksum, 0, CHECKSUM_SIZE);
}

std::string CMessageHeader::GetCommand() const
{
    return std::string(pchCommand, pchCommand + strnlen(pchCommand, COMMAND_SIZE));
}

bool CMessageHeader::IsValid(const MessageStartChars& pchMessageStartIn) const
{
    // Check start string
    if (memcmp(pchMessageStart, pchMessageStartIn, MESSAGE_START_SIZE) != 0)
        return false;

    // Check the command string for errors
    for (const char* p1 = pchCommand; p1 < pchCommand + COMMAND_SIZE; p1++)
    {
        if (*p1 == 0)
        {
            // Must be all zeros after the first zero
            for (; p1 < pchCommand + COMMAND_SIZE; p1++)
                if (*p1 != 0)
                    return false;
        }
        else if (*p1 < ' ' || *p1 > 0x7E)
            return false;
    }

    // Message size
    if (nMessageSize > MAX_SIZE)
    {
        LogPrintf("CMessageHeader::IsValid(): (%s, %u bytes) nMessageSize > MAX_SIZE\n", GetCommand(), nMessageSize);
        return false;
    }

    return true;
}



CAddress::CAddress() : CService()
{
    Init();
}

CAddress::CAddress(CService ipIn, ServiceFlags nServicesIn) : CService(ipIn)
{
    Init();
    nServices = nServicesIn;
}

void CAddress::Init()
{
    nServices = NODE_NETWORK;
    nTime = 100000000;
}

CInv::CInv()
{
    type = 0;
    hash.SetNull();
    hashAux.SetNull();
}

CInv::CInv(int typeIn, const uint256& hashIn)
{
    assert(typeIn != MSG_WTX);
    type = typeIn;
    hash = hashIn;
    if (typeIn == MSG_TX) {
        hashAux = LEGACY_TX_AUTH_DIGEST;
    } else {
        hashAux.SetNull();
    }
}

CInv::CInv(int typeIn, const uint256& hashIn, const uint256& hashAuxIn)
{
    type = typeIn;
    hash = hashIn;
    hashAux = hashAuxIn;
}

bool operator<(const CInv& a, const CInv& b)
{
    return (a.type < b.type ||
        (a.type == b.type && a.hash < b.hash) ||
        (a.type == b.type && a.hash == b.hash && a.hashAux < b.hashAux));
}

std::string CInv::GetCommand() const
{
    std::string cmd;
    switch (type)
    {
    case MSG_TX:             return cmd.append("tx");
    case MSG_BLOCK:          return cmd.append("block");
    // WTX is not a message type, just an inv type
    case MSG_WTX:            return cmd.append("wtx");
    case MSG_FILTERED_BLOCK: return cmd.append("merkleblock");
    default:
        throw std::out_of_range(strprintf("CInv::GetCommand(): type=%d unknown type", type));
    }
}

std::vector<unsigned char> CInv::GetWideHash() const
{
    assert(type != MSG_BLOCK);
    if (type == MSG_TX) {
        for (auto byte : hashAux) {
            assert(byte == 0xff);
        }
    };
    std::vector<unsigned char> vData(hash.begin(), hash.end());
    vData.insert(vData.end(), hashAux.begin(), hashAux.end());
    return vData;
}

std::string CInv::ToString() const
{
    switch (type)
    {
    case MSG_WTX:
        return strprintf("%s(%s, %s)", GetCommand(), hash.ToString(), hashAux.ToString());
    default:
        return strprintf("%s %s", GetCommand(), hash.ToString());
    }
}
