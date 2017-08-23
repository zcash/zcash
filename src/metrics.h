// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"

#include <atomic>
#include <mutex>
#include <string>

struct AtomicCounter {
    std::atomic<uint64_t> value;

    AtomicCounter() : value {0} { }

    void increment(){
        ++value;
    }

    void decrement(){
        --value;
    }

    int get() const {
        return value.load();
    }
};

class AtomicTimer {
private:
    std::mutex mtx;
    uint64_t threads;
    int64_t start_time;
    int64_t total_time;

public:
    AtomicTimer() : threads(0), start_time(0), total_time(0) {}

    /**
     * Starts timing on first call, and counts the number of calls.
     */
    void start();

    /**
     * Counts number of calls, and stops timing after it has been called as
     * many times as start().
     */
    void stop();

    bool running();

    uint64_t threadCount();

    double rate(const AtomicCounter& count);
};

extern AtomicCounter transactionsValidated;
extern AtomicCounter ehSolverRuns;
extern AtomicCounter solutionTargetChecks;
extern AtomicTimer miningTimer;

void TrackMinedBlock(uint256 hash);

void MarkStartTime();
double GetLocalSolPS();

void TriggerRefresh();

void ConnectMetricsScreen();
void ThreadShowMetricsScreen();

/**
 * Heart image: https://commons.wikimedia.org/wiki/File:Heart_coraz%C3%B3n.svg
 * License: CC BY-SA 3.0
 *
 * Rendering options:
 * Logo: img2txt -W 90 -H 20 -f utf8 -d none -g design.png >> design.ansi
 */
const std::string METRICS_ART =
"                   [0;34;45m:::[0m    [0;34;45m:::[0m                                                             \n"
"                  [0;34;45m;[0;35;5;45;105m...[0;34;45m:::[0;1;35;95;45m.[0;35;5;45;105m...[0;34;45m%[0m                                                            \n"
"            [0;34;45mX[0;1;34;94;45m8[0;1;35;95;45m;[0;35;5;45;105m..................[0;1;35;95;45m;[0;34;45m:X[0m                                                      \n"
"         [0;1;35;95;45m.[0;35;5;45;105m.....;[0;1;34;94;45m8[0;34;45mt:[0;35;5;45;105m...[0m    [0;35;5;45;105m...[0;34;45m:;[0;1;34;94;45m8[0;35;5;45;105m;....;[0m           [0;1;31;91;41m;888888[0m              [0;1;31;91;41m%88888S[0m             \n"
"      [0;34;45mt[0;35;5;45;105m;....[0;34;45m:[0m      [0;35;5;45;105m...[0m    [0;35;5;45;105m...[0m      [0;34;45m:S[0m        [0;31;5;41;101m8             :[0;1;31;91;41m8[0m     [0;31;5;41;101m@             :[0m         \n"
"    [0;34;45mS[0;35;5;45;105m....[0;1;35;95;45m:[0m         [0;35;5;45;105m...[0m    [0;35;5;45;105m...[0m              [0;31;5;41;101mS                  S[0m [0;31;5;41;101m                  :[0m       \n"
"   [0;35;5;45;105mt...;[0m      [0;34;45m;[0;35;5;45;105m..................[0m         [0;31;5;41;101m%                                         [0m      \n"
"  [0;35;5;45;105m....t[0m       [0;34;45m%[0;35;5;45;105m.................;[0m         [0;31;5;41;101m                                          [0;1;31;91;41mX[0m     \n"
" [0;1;34;94;45m8[0;35;5;45;105m....[0m                 [0;34;45m:[0;35;5;45;105m......[0;1;35;95;45m:[0m           [0;31;5;41;101m                                          [0;1;31;91;41mX[0m     \n"
" [0;35;5;45;105m....[0;34;45m;[0m               [0;34;45m:[0;35;5;45;105m......[0;34;45m:[0m              [0;31;5;41;101m                                         [0m      \n"
" [0;35;5;45;105m....[0;34;45m;[0m             [0;35;5;45;105m;.....;[0;34;45mS[0m                [0;31;5;41;101m@                                      :[0m       \n"
" [0;1;35;95;45m.[0;35;5;45;105m...;[0m          [0;34;45m:[0;35;5;45;105m......[0;34;45m:[0m                     [0;31;5;41;101m                                    @[0m        \n"
"  [0;35;5;45;105m....[0;1;35;95;45m:[0m       [0;1;35;95;45m.[0;35;5;45;105m.......;[0;34;45m::::::::::[0m              [0;31;5;41;101m:                               8[0m          \n"
"   [0;35;5;45;105m....[0;1;35;95;45mt[0m      [0;35;5;45;105m...................[0;34;45m;[0m                [0;31;5;41;101m%                        :[0;1;31;91;41m@[0m             \n"
"    [0;34;45m;[0;35;5;45;105m....[0;34;45m:[0m        [0;34;45mX[0;35;5;45;105m...[0m   [0;34;45mX[0;35;5;45;105m...[0;34;45mX[0m                       [0;31;5;41;101mS                   [0;1;31;91;41m8[0m                \n"
"      [0;1;34;94;45m8[0;35;5;45;105m....;[0;34;45mS[0m      [0;35;5;45;105m...[0m    [0;35;5;45;105m...[0m      [0;34;45m%S[0m                   [0;31;5;41;101mS             [0;1;31;91;41m8[0m                   \n"
"        [0;34;45mS[0;1;35;95;45m;[0;35;5;45;105m.....[0;1;35;95;45m.[0;34;45m%[0m [0;34;45m%[0;35;5;45;105m...[0m    [0;35;5;45;105m...[0;34;45m%Xt[0;1;35;95;45m:[0;35;5;45;105m.....[0;34;45mS[0m                    [0;31;5;41;101mS       [0;1;31;91;41m8[0m                      \n"
"            [0;34;45m;[0;1;35;95;45m:[0;35;5;45;105m....................[0;1;35;95;45m:[0;34;45m;[0m                         [0;1;31;91;41m8[0;31;5;41;101m  S[0m                         \n"
"                  [0;1;34;94;45m8[0;35;5;45;105m...[0;1;35;95;45m:::;[0;35;5;45;105m...[0;34;45m:[0m                                                            \n"
"                   [0;34;45m...[0m    [0;34;45m...[0m                                                             \n";