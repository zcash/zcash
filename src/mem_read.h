#pragma once
/******************************************************************************
 * Copyright © 2021 Komodo Core Deveelopers                                   *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "komodo_structs.h" // for parse_error

#include <cstdint>
#include <string>

/***
 * These read a chunk of memory with some built-in type safety
 */

template<class T>
std::size_t mem_read(T& dest, uint8_t *filedata, long &fpos, long datalen)
{
    if (fpos + sizeof(T) <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sizeof(T) );
        fpos += sizeof(T);
        return sizeof(T);
    }
    throw komodo::parse_error("Invalid size: " + std::to_string(sizeof(T)) );
}

template<class T, std::size_t N>
std::size_t mem_read(T(&dest)[N], uint8_t *filedata, long& fpos, long datalen)
{
    std::size_t sz = sizeof(T) * N;
    if (fpos + sz <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sz );
        fpos += sz;
        return sz;
    }
    throw komodo::parse_error("Invalid size: "  + std::to_string( sz ) );
}

/****
 * Read a size that is less than the array length
 */
template<class T, std::size_t N>
std::size_t mem_nread(T(&dest)[N], size_t num_elements, uint8_t *filedata, long& fpos, long datalen)
{
    std::size_t sz = sizeof(T) * num_elements;
    if (fpos + sz <= datalen)
    {
        memcpy( &dest, &filedata[fpos], sz );
        fpos += sz;
        return sz;
    }
    throw komodo::parse_error("Invalid size: " + std::to_string(sz));
}