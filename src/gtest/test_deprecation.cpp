#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chainparams.h"
#include "clientversion.h"
#include "deprecation.h"
#include "fs.h"
#include "gtest/utils.h"
#include "init.h"
#include "ui_interface.h"
#include "util/system.h"
#include "util/strencodings.h"

#include <fstream>

using namespace boost::placeholders;
using ::testing::StrictMock;

static const std::string CLIENT_VERSION_STR = FormatVersion(CLIENT_VERSION);

class DeprecationTest : public ::testing::Test {
protected:
    void SetUp() override {
        ConnectMockUIInterface(mock_);
        SelectParams(CBaseChainParams::MAIN);

    }

    void TearDown() override {
        DisconnectMockUIInterface();
        mapArgs.clear();
    }

    StrictMock<MockUIInterface> mock_;

    static std::vector<std::string> read_lines(fs::path filepath) {
        std::vector<std::string> result;

        std::ifstream f(filepath.string().c_str());
        std::string line;
        while (std::getline(f,line)) {
            result.push_back(line);
        }

        return result;
    }
};

TEST_F(DeprecationTest, NonDeprecatedNodeKeepsRunning) {
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT - 1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationIsWarned) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationWarningIsNotDuplicated) {
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT + 1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationWarningIsRepeatedOnStartup) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT + 1, true);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeShutsDown) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_ERROR));
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeErrorIsNotDuplicated) {
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT + 1);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeErrorIsRepeatedOnStartup) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_ERROR));
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT + 1, true);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeIgnoredOnRegtest) {
    SelectParams(CBaseChainParams::REGTEST);
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT+1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeIgnoredOnTestnet) {
    SelectParams(CBaseChainParams::TESTNET);
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT+1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, AlertNotify) {
    fs::path temp = fs::temp_directory_path() /
        fs::unique_path("alertnotify-%%%%.txt");

    mapArgs["-alertnotify"] = std::string("echo %s >> ") + temp.string();

    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(Params(), DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT, false, false);

    std::vector<std::string> r = read_lines(temp);
    EXPECT_EQ(r.size(), 1u);

    // -alertnotify restricts the message to safe characters.
    auto expectedMsg = strprintf(
        "This version will be deprecated at block height %d, and will automatically shut down. You should upgrade to the latest version of Zcash.",
        DEPRECATION_HEIGHT);

    // Windows built-in echo semantics are different than posixy shells. Quotes and
    // whitespace are printed literally.
#ifndef WIN32
    EXPECT_EQ(r[0], expectedMsg);
#else
    EXPECT_EQ(r[0], strprintf("'%s' ", expectedMsg));
#endif
    fs::remove(temp);
}
