// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "experimental_features.h"

#include "util.h"

bool fExperimentalDeveloperEncryptWallet = false;
bool fExperimentalDeveloperSetPoolSizeZero = false;
bool fExperimentalPaymentDisclosure = false;
bool fExperimentalInsightExplorer = false;
bool fExperimentalLightWalletd = false;

boost::optional<std::string> InitExperimentalMode()
{
    auto fExperimentalMode = GetBoolArg("-experimentalfeatures", false);
    fExperimentalDeveloperEncryptWallet = GetBoolArg("-developerencryptwallet", false);
    fExperimentalDeveloperSetPoolSizeZero = GetBoolArg("-developersetpoolsizezero", false);
    fExperimentalPaymentDisclosure = GetBoolArg("-paymentdisclosure", false);
    fExperimentalInsightExplorer = GetBoolArg("-insightexplorer", false);
    fExperimentalLightWalletd  = GetBoolArg("-lightwalletd", false);

    // Fail if user has set experimental options without the global flag
    if (!fExperimentalMode) {
        if (fExperimentalDeveloperEncryptWallet) {
            return _("Wallet encryption requires -experimentalfeatures.");
        } else if (fExperimentalDeveloperSetPoolSizeZero) {
            return _("Setting the size of shielded pools to zero requires -experimentalfeatures.");
        } else if (fExperimentalPaymentDisclosure) {
            return _("Payment disclosure requires -experimentalfeatures.");
        } else if (fExperimentalInsightExplorer) {
            return _("Insight explorer requires -experimentalfeatures.");
        } else if (fExperimentalLightWalletd) {
            return _("Light Walletd requires -experimentalfeatures.");
        }
    }
    return boost::none;
}

std::vector<std::string> GetExperimentalFeatures()
{
    std::vector<std::string> experimentalfeatures;
    if (fExperimentalDeveloperEncryptWallet)
        experimentalfeatures.push_back("developerencryptwallet");
    if (fExperimentalDeveloperSetPoolSizeZero)
        experimentalfeatures.push_back("developersetpoolsizezero");
    if (fExperimentalPaymentDisclosure)
        experimentalfeatures.push_back("paymentdisclosure");
    if (fExperimentalInsightExplorer)
        experimentalfeatures.push_back("insightexplorer");
    if (fExperimentalLightWalletd)
        experimentalfeatures.push_back("lightwalletd");

    return experimentalfeatures;
}
