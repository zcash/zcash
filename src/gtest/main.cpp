#include "gmock/gmock.h"
#include "init.h"
#include "key.h"
#include "pubkey.h"
#include "random.h"
#include "util.h"
#include "utiltest.h"

#include "librustzcash.h"
#include <sodium.h>
#include <tracing.h>

#include <boost/filesystem.hpp>

const std::function<std::string(const char*)> G_TRANSLATION_FUN = nullptr;

uint256 insecure_rand_seed = GetRandHash();
FastRandomContext insecure_rand_ctx(insecure_rand_seed);

struct ECCryptoClosure
{
    ECCVerifyHandle handle;
};

ECCryptoClosure instance_of_eccryptoclosure;

class LogGrabber : public ::testing::EmptyTestEventListener {
    fs::path logPath;

public:
    LogGrabber(fs::path logPathIn) : logPath(logPathIn) {}

    virtual void OnTestStart(const ::testing::TestInfo& test_info) {
        // Test logs are written synchronously, so we can clear the log file to
        // ensure that at the end of the test, the log lines are all related to
        // the test itself.
        std::ofstream logFile;
        logFile.open(logPath.string(), std::ofstream::out | std::ofstream::trunc);
        logFile.close();
    }

    virtual void OnTestEnd(const ::testing::TestInfo& test_info) {
        // If the test failed, print the test logs.
        auto result = test_info.result();
        if (result && result->Failed()) {
            std::cout << "\n--- Logs:";

            std::ifstream logFile;
            logFile.open(logPath.string(), std::ios::in | std::ios::ate);
            ASSERT_TRUE(logFile.is_open());
            logFile.seekg(0, logFile.beg);
            std::string line;
            while (logFile.good()) {
                std::getline(logFile, line);
                if (!line.empty()) {
                    std::cout << "\n  " << line;
                }
            }

            std::cout << "\n---" << std::endl;
        }
    }
};

int main(int argc, char **argv) {
  assert(sodium_init() != -1);
  ECC_Start();

    // Log all errors to a common test file.
    fs::path tmpPath = fs::temp_directory_path();
    fs::path tmpFilename = fs::unique_path("%%%%%%%%");
    fs::path logPath = tmpPath / tmpFilename;
    const fs::path::string_type& logPathStr = logPath.native();
    static_assert(sizeof(fs::path::value_type) == sizeof(codeunit),
                    "native path has unexpected code unit size");
    const codeunit* logPathCStr = reinterpret_cast<const codeunit*>(logPathStr.c_str());
    size_t logPathLen = logPathStr.length();

    std::string initialFilter = "error";
    pTracingHandle = tracing_init_test(
        logPathCStr, logPathLen,
        initialFilter.c_str());

  testing::InitGoogleMock(&argc, argv);

  // The "threadsafe" style is necessary for correct operation of death/exit
  // tests on macOS (https://github.com/zcash/zcash/issues/4802).
  testing::FLAGS_gtest_death_test_style = "threadsafe";

    ::testing::TestEventListeners& listeners =
        ::testing::UnitTest::GetInstance()->listeners();
    listeners.Append(new LogGrabber(logPath));

  auto ret = RUN_ALL_TESTS();

  ECC_Stop();
  return ret;
}
