/** @file
 *****************************************************************************

 A test for Zerocash.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <sys/time.h>

#include "zerocash/Zerocash.h"
#include "zerocash/ZerocashParams.h"
#include "zerocash/Address.h"
#include "zerocash/CoinCommitment.h"
#include "zerocash/Coin.h"
#include "zerocash/IncrementalMerkleTree.h"
#include "zerocash/MintTransaction.h"
#include "zerocash/PourTransaction.h"
#include "zerocash/PourInput.h"
#include "zerocash/PourOutput.h"
#include "zerocash/utils/util.h"

using namespace std;
using namespace libsnark;

struct timeval tv_start;
struct timeval tv_end;

#define TEST_TREE_DEPTH 4
// TODO: use the actual proving and verifying keys
#define TEST_PK_PATH "./zerocashPerformance-proving-key"

void timer_start() {
    gettimeofday(&tv_start, 0);
}

double timer_stop() {
    double elapsed;
    gettimeofday(&tv_end, 0);
    elapsed = double(tv_end.tv_sec-tv_start.tv_sec) +
        (tv_end.tv_usec-tv_start.tv_usec)/double(1000000);
    return elapsed;
}

// PROBLEM: need to do setup in a DIFFERENT PROCESS before this one launches
// otherwise we're measuring the memory usage of all that crap. Tests need to
// *only* create the stuff they need.

double samplePerformanceOfJoinSplit()
{
}

double samplePerformanceOfVerify()
{

}

double samplePerformanceOfProvingKeyLoading()
{
    timer_start();
    string pk_path = TEST_PK_PATH;
    auto pk_loaded = libzerocash::ZerocashParams::LoadProvingKeyFromFile(pk_path, TEST_TREE_DEPTH);
    return timer_stop();
}

double samplePerformanceOfEquihash()
{

}

int main(int argc, char **argv) {

    const char *test_type = NULL;
    long samples = 0;

    if (argc != 3) {
        std::cout << "Bad command-line arguments." << std::endl;
        return 1;
    }

    test_type = argv[1];
    samples = strtol(argv[2], NULL, 10);

    double *sample_times = new double[samples];

    for (long run = 0; run < samples; ++run) {
        if (strcmp("joinsplit", test_type) == 0) {
            sample_times[run] = samplePerformanceOfJoinSplit();
        } else if (strcmp("verify", test_type) == 0) {
            sample_times[run] = samplePerformanceOfVerify();
        } else if (strcmp("provingkeyloading", test_type) == 0) {
            sample_times[run] = samplePerformanceOfProvingKeyLoading();
        } else if (strcmp("equihash", test_type) == 0) {
            sample_times[run] = samplePerformanceOfEquihash();
        } else {
            std::cout << "Unknown test type." << std::endl;
            return 1;
        }
    }

    for (long run = 0; run < samples; ++run) {
        std::cout << sample_times[run] << std::endl;
    }

    return 0;
}
