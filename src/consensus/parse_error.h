// Copyright (c) 2026 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CONSENSUS_PARSE_ERROR_H
#define BITCOIN_CONSENSUS_PARSE_ERROR_H

#include "consensus/validation.h"

#include <rust/cxx.h>

#include <cstring>
#include <ios>

/**
 * Maps a Rust bundle-parse error (tagged with a kind prefix by
 * `bundle_parse_error::format_bundle_parse_error`) to the appropriate C++
 * exception type for P2P handlers.
 */
inline void ThrowBundleParseError(const rust::Error& e)
{
    const char* what = e.what();
    constexpr const char* io_prefix = "zcash_parse_io: ";
    constexpr const char* consensus_prefix = "zcash_parse_consensus: ";
    const size_t io_prefix_len = std::strlen(io_prefix);
    const size_t consensus_prefix_len = std::strlen(consensus_prefix);

    if (std::strncmp(what, io_prefix, io_prefix_len) == 0) {
        throw std::ios_base::failure(what + io_prefix_len);
    }
    if (std::strncmp(what, consensus_prefix, consensus_prefix_len) == 0) {
        throw consensus_rule_failure(what + consensus_prefix_len);
    }
    throw std::ios_base::failure(what);
}

#endif // BITCOIN_CONSENSUS_PARSE_ERROR_H
