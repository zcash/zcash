// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
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

#ifndef ZCASH_DEPRECATION_H
#define ZCASH_DEPRECATION_H

// Deprecation policy:
// * Shut down WEEKS_UNTIL_DEPRECATION weeks' worth of blocks after the estimated release block height.
// * A warning is shown during the DEPRECATION_WARN_LIMIT worth of blocks prior to shut down.
static const int WEEKS_UNTIL_DEPRECATION = 52;
static const int DEPRECATION_HEIGHT = 2900000; //TODO: use [last_season_array_item - 1] + 650.000 for automagic update
static const int APPROX_RELEASE_HEIGHT = DEPRECATION_HEIGHT - (WEEKS_UNTIL_DEPRECATION * 7 * 24 * 60);

// Number of blocks before deprecation to warn users
static const int DEPRECATION_WARN_LIMIT = 60 * 24 * 60; // 2 months

/**
 * Checks whether the node is deprecated based on the current block height, and
 * shuts down the node with an error if so (and deprecation is not disabled for
 * the current client version).
 */
void EnforceNodeDeprecation(int nHeight, bool forceLogging=false, bool fThread=true);

#endif // ZCASH_DEPRECATION_H
