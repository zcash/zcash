// (C) 2018 The Verus Developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
This provides the PoW hash function for Verus, a CPU-optimized hash 
function with a Haraka V2 core. Unlike Haraka, which is made for short 
inputs only, Verus Hash takes any length of input and produces a 256 
bit output.
*/
#include <string.h>
#include "crypto/common.h"
#include "crypto/verus_hash.h"

void (*CVerusHash::haraka512Function)(unsigned char *out, const unsigned char *in);

void CVerusHash::Hash(void *result, const void *data, size_t _len)
{
    unsigned char buf[128];
    unsigned char *bufPtr = buf;
    int nextOffset = 64;
    uint32_t pos = 0, len = _len;
    unsigned char *bufPtr2 = bufPtr + nextOffset;
    unsigned char *ptr = (unsigned char *)data;

    // put our last result or zero at beginning of buffer each time
    memset(bufPtr, 0, 32);

    // digest up to 32 bytes at a time
    for ( ; pos < len; pos += 32)
    {
        if (len - pos >= 32)
        {
            memcpy(bufPtr + 32, ptr + pos, 32);
        }
        else
        {
            int i = (int)(len - pos);
            memcpy(bufPtr + 32, ptr + pos, i);
            memset(bufPtr + 32 + i, 0, 32 - i);
        }
        (*haraka512Function)(bufPtr2, bufPtr);
        bufPtr2 = bufPtr;
        bufPtr += nextOffset;
        nextOffset *= -1;
    }
    memcpy(result, bufPtr, 32);
};

void CVerusHash::init()
{
    if (IsCPUVerusOptimized())
    {
        haraka512Function = &haraka512_zero;
    }
    else
    {
        haraka512Function = &haraka512_port_zero;
    }
}

CVerusHash &CVerusHash::Write(const unsigned char *data, size_t _len)
{
    unsigned char *tmp;
    uint32_t pos, len = _len;

    // digest up to 32 bytes at a time
    for ( pos = 0; pos < len; )
    {
        uint32_t room = 32 - curPos;

        if (len - pos >= room)
        {
            memcpy(curBuf + 32 + curPos, data + pos, room);
            (*haraka512Function)(result, curBuf);
            tmp = curBuf;
            curBuf = result;
            result = tmp;
            pos += room;
            curPos = 0;
        }
        else
        {
            memcpy(curBuf + 32 + curPos, data + pos, len - pos);
            curPos += len - pos;
            pos = len;
        }
    }
    return *this;
}

// to be declared and accessed from C
void verus_hash(void *result, const void *data, size_t len)
{
    return CVerusHash::Hash(result, data, len);
}

void (*CVerusHashV2::haraka512Function)(unsigned char *out, const unsigned char *in);

void CVerusHashV2::init()
{
    if (IsCPUVerusOptimized())
    {
        load_constants();
        haraka512Function = &haraka512;
    }
    else
    {
        // load and tweak the haraka constants
        load_constants_port();
        haraka512Function = &haraka512_port;
    }
}

void CVerusHashV2::Hash(void *result, const void *data, size_t len)
{
    unsigned char buf[128];
    unsigned char *bufPtr = buf;
    int pos = 0, nextOffset = 64;
    unsigned char *bufPtr2 = bufPtr + nextOffset;
    unsigned char *ptr = (unsigned char *)data;

    // put our last result or zero at beginning of buffer each time
    memset(bufPtr, 0, 32);

    // digest up to 32 bytes at a time
    for ( ; pos < len; pos += 32)
    {
        if (len - pos >= 32)
        {
            memcpy(bufPtr + 32, ptr + pos, 32);
        }
        else
        {
            int i = (int)(len - pos);
            memcpy(bufPtr + 32, ptr + pos, i);
            memset(bufPtr + 32 + i, 0, 32 - i);
        }
        (*haraka512Function)(bufPtr2, bufPtr);
        bufPtr2 = bufPtr;
        bufPtr += nextOffset;
        nextOffset *= -1;
    }
    memcpy(result, bufPtr, 32);
};

CVerusHashV2 &CVerusHashV2::Write(const unsigned char *data, size_t len)
{
    unsigned char *tmp;

    // digest up to 32 bytes at a time
    for ( int pos = 0; pos < len; )
    {
        int room = 32 - curPos;

        if (len - pos >= room)
        {
            memcpy(curBuf + 32 + curPos, data + pos, room);
            (*haraka512Function)(result, curBuf);
            tmp = curBuf;
            curBuf = result;
            result = tmp;
            pos += room;
            curPos = 0;
        }
        else
        {
            memcpy(curBuf + 32 + curPos, data + pos, len - pos);
            curPos += len - pos;
            pos = len;
        }
    }
    return *this;
}

// to be declared and accessed from C
void verus_hash_v2(void *result, const void *data, size_t len)
{
    return CVerusHashV2::Hash(result, data, len);
}
