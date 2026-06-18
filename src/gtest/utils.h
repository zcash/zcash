#ifndef BITCOIN_GTEST_UTILS_H
#define BITCOIN_GTEST_UTILS_H

#include "random.h"
#include "util/system.h"
#include "key_io.h"

#include <gmock/gmock.h>

int GenZero(int n);
int GenMax(int n);
void LoadProofParameters();
void LoadGlobalWallet();
void UnloadGlobalWallet();

// A mock for uiInterface.ThreadSafeMessageBox, for tests of code that notifies the
// user via that signal (e.g. a divergence-triggered shutdown or end-of-service
// halt). The gtest harness connects no slot, so without one the signals2 combiner
// throws on zero slots; ConnectMockUIInterface connects this mock.
class MockUIInterface {
public:
    MOCK_METHOD3(ThreadSafeMessageBox, bool(const std::string& message,
                                            const std::string& caption,
                                            unsigned int style));
};

// Replace any slots on uiInterface.ThreadSafeMessageBox with one forwarding to `mock`.
// `mock` must outlive the connection; pair with DisconnectMockUIInterface, which must
// be called before `mock` is destroyed.
void ConnectMockUIInterface(MockUIInterface& mock);

// Teardown counterpart to ConnectMockUIInterface for tests of code that may request a
// shutdown: disconnect all slots from uiInterface.ThreadSafeMessageBox and clear any
// shutdown the test requested (StartShutdown sets fRequestShutdown, which has no
// production reset). Clearing the flag needs no synchronization: in the gtest process
// nothing consumes it (the node main loop and wallet notification thread are not
// running), so there is no concurrent observer.
void DisconnectMockUIInterface();

template<typename Tree> void AppendRandomLeaf(Tree &tree);

#endif
