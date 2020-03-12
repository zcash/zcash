// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include <string>
#include <vector>
#include <boost/optional.hpp>

extern bool fExperimentalDeveloperEncryptWallet;
extern bool fExperimentalDeveloperSetPoolSizeZero;
extern bool fExperimentalPaymentDisclosure;
extern bool fExperimentalInsightExplorer;

boost::optional<std::string> InitExperimentalMode();
std::vector<std::string> GetExperimentalFeatures();
