#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "chainparams.h"
#include "clientversion.h"
#include "deprecation.h"
#include "init.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"

#include <boost/filesystem/operations.hpp>
#include <fstream>

using ::testing::StrictMock;

static const std::string CLIENT_VERSION_STR = FormatVersion(CLIENT_VERSION);
extern std::atomic<bool> fRequestShutdown;

class MockUIInterface {
public:
    MOCK_METHOD3(ThreadSafeMessageBox, bool(const std::string& message,
                                      const std::string& caption,
                                      unsigned int style));
};

static bool ThreadSafeMessageBox(MockUIInterface *mock,
                                 const std::string& message,
                                 const std::string& caption,
                                 unsigned int style)
{
    return mock->ThreadSafeMessageBox(message, caption, style);
}

class DeprecationTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        uiInterface.ThreadSafeMessageBox.disconnect_all_slots();
        uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, &mock_, _1, _2, _3));
        SelectParams(CBaseChainParams::MAIN);
        
    }

    virtual void TearDown() {
        fRequestShutdown = false;
        mapArgs.clear();
    }

    StrictMock<MockUIInterface> mock_;

    static std::vector<std::string> read_lines(boost::filesystem::path filepath) {
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
    EnforceNodeDeprecation(DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT - 1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationIsWarned) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationWarningIsNotDuplicated) {
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT + 1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, NodeNearDeprecationWarningIsRepeatedOnStartup) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT + 1, true);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeShutsDown) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_ERROR));
    EnforceNodeDeprecation(DEPRECATION_HEIGHT);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeErrorIsNotDuplicated) {
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(DEPRECATION_HEIGHT + 1);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeErrorIsRepeatedOnStartup) {
    EXPECT_FALSE(ShutdownRequested());
    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_ERROR));
    EnforceNodeDeprecation(DEPRECATION_HEIGHT + 1, true);
    EXPECT_TRUE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeIgnoredOnRegtest) {
    SelectParams(CBaseChainParams::REGTEST);
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(DEPRECATION_HEIGHT+1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, DeprecatedNodeIgnoredOnTestnet) {
    SelectParams(CBaseChainParams::TESTNET);
    EXPECT_FALSE(ShutdownRequested());
    EnforceNodeDeprecation(DEPRECATION_HEIGHT+1);
    EXPECT_FALSE(ShutdownRequested());
}

TEST_F(DeprecationTest, AlertNotify) {
    boost::filesystem::path temp = GetTempPath() /
        boost::filesystem::unique_path("alertnotify-%%%%.txt");

    mapArgs["-alertnotify"] = std::string("echo %s >> ") + temp.string();

    EXPECT_CALL(mock_, ThreadSafeMessageBox(::testing::_, "", CClientUIInterface::MSG_WARNING));
    EnforceNodeDeprecation(DEPRECATION_HEIGHT - DEPRECATION_WARN_LIMIT, false, false);

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
    boost::filesystem::remove(temp);
}
