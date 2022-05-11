// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "deprecation.h"

#include "alert.h"
#include "clientversion.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#include "chainparams.h"

// Flags that enable deprecated functionality.
#ifdef ENABLE_WALLET
bool fEnableGetNewAddress = true;
bool fEnableGetRawChangeAddress = true;
bool fEnableZGetNewAddress = true;
bool fEnableZGetBalance = true;
bool fEnableZGetTotalBalance = true;
bool fEnableZListAddresses = true;
bool fEnableLegacyPrivacyStrategy = true;
bool fEnableZCRawReceive = true;
bool fEnableZCRawJoinSplit = true;
bool fEnableZCRawKeygen = true;
bool fEnableAddrTypeField = true;
#endif

static const std::string CLIENT_VERSION_STR = FormatVersion(CLIENT_VERSION);

void EnforceNodeDeprecation(int nHeight, bool forceLogging, bool fThread) {

    // Do not enforce deprecation in regtest or on testnet
    std::string networkID = Params().NetworkIDString();
    if (networkID != "main") return;

    int blocksToDeprecation = DEPRECATION_HEIGHT - nHeight;
    if (blocksToDeprecation <= 0) {
        // In order to ensure we only log once per process when deprecation is
        // disabled (to avoid log spam), we only need to log in two cases:
        // - The deprecating block just arrived
        //   - This can be triggered more than once if a block chain reorg
        //     occurs, but that's an irregular event that won't cause spam.
        // - The node is starting
        if (blocksToDeprecation == 0 || forceLogging) {
            auto msg = strprintf(_("This version has been deprecated as of block height %d."),
                                 DEPRECATION_HEIGHT) + " " +
                       _("You should upgrade to the latest version of Zcash.");
            LogPrintf("*** %s\n", msg);
            CAlert::Notify(msg, fThread);
            uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_ERROR);
        }
        StartShutdown();
    } else if (blocksToDeprecation == DEPRECATION_WARN_LIMIT ||
               (blocksToDeprecation < DEPRECATION_WARN_LIMIT && forceLogging)) {
        std::string msg = strprintf(_("This version will be deprecated at block height %d, and will automatically shut down."),
                            DEPRECATION_HEIGHT) + " " +
                  _("You should upgrade to the latest version of Zcash.");
        LogPrintf("*** %s\n", msg);
        CAlert::Notify(msg, fThread);
        uiInterface.ThreadSafeMessageBox(msg, "", CClientUIInterface::MSG_WARNING);
    }
}

std::optional<std::string> SetAllowedDeprecatedFeaturesFromCLIArgs() {
    auto args = GetMultiArg("-allowdeprecated");
    std::set<std::string> allowdeprecated(args.begin(), args.end());

    if (allowdeprecated.count("none") > 0) {
        if (allowdeprecated.size() > 1)
            return "When using -allowdeprecated=none no other values may be provided for -allowdeprecated.";
        allowdeprecated = {};
    } else {
        allowdeprecated.insert(DEFAULT_ALLOW_DEPRECATED.begin(), DEFAULT_ALLOW_DEPRECATED.end());
    }

    std::set<std::string> unrecognized;
    for (const auto& flag : allowdeprecated) {
        if (DEFAULT_ALLOW_DEPRECATED.count(flag) == 0 && DEFAULT_DENY_DEPRECATED.count(flag) == 0)
            unrecognized.insert(flag);
    }

    if (unrecognized.size() > 0) {
        std::string unrecMsg;
        for (const auto& value : unrecognized) {
            if (unrecMsg.size() > 0) unrecMsg += ", ";
            unrecMsg += "\"" + value + "\"";
        }

        return strprintf(
                "Unrecognized argument(s) to -allowdeprecated: %s;\n"
                "Please select from the following values: %s",
                unrecMsg, GetAllowableDeprecatedFeatures());
    }

#ifdef ENABLE_WALLET
    fEnableLegacyPrivacyStrategy = allowdeprecated.count("legacy_privacy") > 0;
    fEnableGetNewAddress = allowdeprecated.count("getnewaddress") > 0;
    fEnableGetRawChangeAddress = allowdeprecated.count("getrawchangeaddress") > 0;
    fEnableZGetNewAddress = allowdeprecated.count("z_getnewaddress") > 0;
    fEnableZGetBalance = allowdeprecated.count("z_getbalance") > 0;
    fEnableZGetTotalBalance = allowdeprecated.count("z_gettotalbalance") > 0;
    fEnableZListAddresses = allowdeprecated.count("z_listaddresses") > 0;
    fEnableZCRawReceive = allowdeprecated.count("zcrawreceive") > 0;
    fEnableZCRawJoinSplit = allowdeprecated.count("zcrawjoinsplit") > 0;
    fEnableZCRawKeygen = allowdeprecated.count("zcrawkeygen") > 0;
    fEnableAddrTypeField = allowdeprecated.count("addrtype") > 0;
#endif

    return std::nullopt;
}

std::string GetAllowableDeprecatedFeatures() {
    std::string result = "\"none\"";
    for (const auto& value : DEFAULT_ALLOW_DEPRECATED) {
        result += ", \"" + value + "\"";
    }
    for (const auto& value : DEFAULT_DENY_DEPRECATED) {
        result += ", \"" + value + "\"";
    }
    return result;
}

