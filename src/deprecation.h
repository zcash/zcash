// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_DEPRECATION_H
#define ZCASH_DEPRECATION_H

// Deprecation policy:
// * Shut down WEEKS_UNTIL_DEPRECATION weeks' worth of blocks after the estimated release block height.
// * A warning is shown during the WEEKS_UNTIL_DEPRECATION worth of blocks prior to shut down.
//static const int APPROX_RELEASE_HEIGHT = 800000;
//static const int WEEKS_UNTIL_DEPRECATION = 52;
static const int DEPRECATION_HEIGHT = 1400000; //APPROX_RELEASE_HEIGHT + (WEEKS_UNTIL_DEPRECATION * 7 * 24 * 60);

// Number of blocks before deprecation to warn users
static const int WEEKS_UNTIL_DEPRECATION = 60 * 24 * 60; // 2 months

/**
 * Checks whether the node is deprecated based on the current block height, and
 * shuts down the node with an error if so (and deprecation is not disabled for
 * the current client version).
 */
void EnforceNodeDeprecation(int nHeight, bool forceLogging=false);

#endif // ZCASH_DEPRECATION_H
