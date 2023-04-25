// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_DEPRECATION_H
#define ZCASH_DEPRECATION_H

#include "chainparams.h"
#include "consensus/params.h"
#include "util/time.h"

// Deprecation policy:
// Per https://zips.z.cash/zip-0200
// Shut down nodes running this version of code, 16 weeks' worth of blocks after the estimated
// release block height. A warning is shown during the 14 days' worth of blocks prior to shut down.
static const int APPROX_RELEASE_HEIGHT = 2064276;
static const int RELEASE_TO_DEPRECATION_WEEKS = 16;
static const int EXPECTED_BLOCKS_PER_HOUR = 3600 / Consensus::POST_BLOSSOM_POW_TARGET_SPACING;
static_assert(EXPECTED_BLOCKS_PER_HOUR == 48, "The value of Consensus::POST_BLOSSOM_POW_TARGET_SPACING was chosen such that this assertion holds.");
static const int ACTIVATION_TO_DEPRECATION_BLOCKS = (RELEASE_TO_DEPRECATION_WEEKS * 7 * 24 * EXPECTED_BLOCKS_PER_HOUR);
static const int DEPRECATION_HEIGHT = APPROX_RELEASE_HEIGHT + ACTIVATION_TO_DEPRECATION_BLOCKS;

// Number of blocks before deprecation to warn users
static const int DEPRECATION_WARN_LIMIT = 14 * 24 * EXPECTED_BLOCKS_PER_HOUR;

//! Defaults for -allowdeprecated
static const std::set<std::string> DEFAULT_ALLOW_DEPRECATED{{
    // Node-level features
    "gbt_oldhashes",
    "deprecationinfo_deprecationheight",

    // Wallet-level features
#ifdef ENABLE_WALLET
    "z_getbalance",
    "z_gettotalbalance",
#endif
}};
static const std::set<std::string> DEFAULT_DENY_DEPRECATED{{
    // Node-level features

    // Wallet-level features
#ifdef ENABLE_WALLET
    "legacy_privacy",
    "getnewaddress",
    "getrawchangeaddress",
    "z_getnewaddress",
    "z_listaddresses",
    "addrtype",
    "wallettxvjoinsplit",
#endif
}};

// Flags that enable deprecated functionality.
extern bool fEnableGbtOldHashes;
extern bool fEnableDeprecationInfoDeprecationHeight;
extern bool fEnableAddrTypeField;
#ifdef ENABLE_WALLET
extern bool fEnableGetNewAddress;
extern bool fEnableGetRawChangeAddress;
extern bool fEnableZGetNewAddress;
extern bool fEnableZGetBalance;
extern bool fEnableZGetTotalBalance;
extern bool fEnableZListAddresses;
extern bool fEnableLegacyPrivacyStrategy;
extern bool fEnableWalletTxVJoinSplit;
#endif

/**
 * Returns the estimated time, in seconds since the epoch, at which deprecation
 * enforcement will take effect for this node.
 */
int64_t EstimatedNodeDeprecationTime(const CClock& clock, int nHeight);

/**
 * Checks whether the node is deprecated based on the current block height, and
 * shuts down the node with an error if so (and deprecation is not disabled for
 * the current client version). Warning and error messages are sent to the debug
 * log, the metrics UI, and (if configured) -alertnofity.
 *
 * fThread means run -alertnotify in a free-running thread.
 */
void EnforceNodeDeprecation(const CChainParams& params, int nHeight, bool forceLogging=false, bool fThread=true);

/**
 * Checks config options for enabling and/or disabling of deprecated
 * features and sets flags that enable deprecated features accordingly.
 *
 * @return std::nullopt if successful, or an error message indicating what
 * values are permitted for `-allowdeprecated`.
 */
std::optional<std::string> LoadAllowedDeprecatedFeatures();

/**
 * Returns a comma-separated list of the valid arguments to the -allowdeprecated
 * CLI option.
 */
std::string GetAllowableDeprecatedFeatures();

#endif // ZCASH_DEPRECATION_H
