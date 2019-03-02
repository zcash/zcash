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

#include "deprecation.h"

#include "alert.h"
#include "clientversion.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#include "chainparams.h"

static const std::string CLIENT_VERSION_STR = FormatVersion(CLIENT_VERSION);
extern char ASSETCHAINS_SYMBOL[KOMODO_ASSETCHAIN_MAXLEN];

void EnforceNodeDeprecation(int nHeight, bool forceLogging, bool fThread) {

    // Do not enforce deprecation in regtest or on testnet
    std::string networkID = Params().NetworkIDString();
    std::string msg;

    if (networkID != "main" || ASSETCHAINS_SYMBOL[0] != 0 ) return;

    int blocksToDeprecation = DEPRECATION_HEIGHT - nHeight;
    if (blocksToDeprecation <= 0) {
        // In order to ensure we only log once per process when deprecation is
        // disabled (to avoid log spam), we only need to log in two cases:
        // - The deprecating block just arrived
        //   - This can be triggered more than once if a block chain reorg
        //     occurs, but that's an irregular event that won't cause spam.
        // - The node is starting
        if (blocksToDeprecation == 0 || forceLogging) {
            msg = strprintf(_("This version has been deprecated as of block height %d."),
                                 DEPRECATION_HEIGHT) + " " +
                       _("You should upgrade to the latest version of Komodo.");
            LogPrintf("*** %s\n", msg);
            CAlert::Notify(msg, fThread);
            uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_ERROR);
        }
        StartShutdown();
    } else if (blocksToDeprecation == DEPRECATION_WARN_LIMIT || (blocksToDeprecation < DEPRECATION_WARN_LIMIT && forceLogging)) {
        msg = strprintf(_("This version will be deprecated at block height %d, and will automatically shut down."),
                            DEPRECATION_HEIGHT) + " " +
                  _("You should upgrade to the latest version of Komodo.");
        LogPrintf("*** %s\n", msg);
        CAlert::Notify(msg, fThread);
        uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_WARNING);
    }
}
