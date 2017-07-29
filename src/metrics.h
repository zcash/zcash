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
int EstimateNetHeightInner(int height, int64_t tipmediantime,
                           int heightLastCheckpoint, int64_t timeLastCheckpoint,
                           int64_t genesisTime, int64_t targetSpacing);

void TriggerRefresh();

void ConnectMetricsScreen();
void ThreadShowMetricsScreen();

/**
 * Heart image: https://commons.wikimedia.org/wiki/File:Heart_coraz%C3%B3n.svg
 * License: CC BY-SA 3.0
 *
 * Rendering options:
 * Zcash: img2txt -W 40 -H 20 -f utf8 -d none -g 0.7 Z-yellow.orange-logo.png
 * Heart: img2txt -W 40 -H 20 -f utf8 -d none 2000px-Heart_corazón.svg.png
 */
const std::string METRICS_ART =
"              [0;34;40m            [0m                                                      \n"
"          [0;34;40m                    [0m                                                  \n"
"       [0;34;40m      [0;31;40m:8[0;33;5;40;100m8[0;1;30;90;43mSX@888@@X[0;31;5;40;100m8[0;31;40m:[0;34;40m      [0m              [0;31;5;41;101m8;     %[0;1;31;91;41mX[0m        [0;1;31;91;41mX[0;31;5;41;101m%     ;8[0m       \n"
"     [0;34;40m     [0;31;40m%[0;1;30;90;43m%X[0;1;33;93;43mt[0;33;5;43;103m%tt%[0;1;30;90;43mSSSS[0;33;5;43;103mS:[0;37;5;43;103mXXX[0;1;33;93;43mt[0;1;30;90;43m@[0;31;40m@[0;34;40m     [0m         [0;1;31;91;41mX[0;31;5;41;101m            :[0;1;31;91;41m:[0m  [0;1;31;91;41m:[0;31;5;41;101m:            [0;1;31;91;41mX[0m    \n"
"   [0;34;40m     [0;31;40m@[0;1;30;90;43mS[0;1;33;93;43m;;tt%%[0;33;5;43;103m%[0;1;33;93;43mt[0;34;40m    [0;1;33;93;43m;[0;33;5;43;103m;::[0;37;5;43;103mXXXX[0;37;43mS[0;31;5;40;100mX[0;34;40m     [0m      [0;31;5;41;101m%               SS               %[0m   \n"
"  [0;34;40m    [0;31;40m.[0;1;30;90;43mt[0;1;33;93;43m:::;;[0;1;30;90;43m%[0;31;40m8888[0;34;40m    [0;31;40m8[0;30;41m8888[0;1;33;93;43mt[0;37;5;43;103mXXXX[0;1;30;90;43m8[0;31;40m;[0;34;40m    [0m    [0;1;31;91;41mS[0;31;5;41;101m                                  [0;1;31;91;41mS[0m  \n"
" [0;34;40m    [0;31;40m.[0;1;30;90;43m%[0;1;33;93;43m...:::[0;31;40m8[0;34;40m             [0;1;30;90;43m8[0;33;5;43;103m::[0;37;5;43;103mXXX[0;1;33;93;43m%[0;31;40m;[0;34;40m    [0m   [0;31;5;41;101mX                                  X[0m  \n"
" [0;34;40m    [0;31;43m8[0;1;31;91;43m888[0;1;33;93;43m...:[0;1;30;90;43mt[0;1;30;90;41m888888[0;31;40mX[0;34;40m     [0;33;5;40;100m8[0;33;5;43;103mt;;::[0;37;5;43;103mXX[0;1;30;90;43m8[0;31;40m [0;34;40m   [0m   [0;31;5;41;101m8                                  8[0m  \n"
"[0;34;40m    [0;31;40m%[0;1;31;91;43m888888[0;1;33;93;43m...:::;:[0;1;30;90;41m8[0;31;40m [0;34;40m   [0;31;40m:[0;1;30;90;43mX[0;33;5;43;103mttt;;;::[0;37;5;43;103mX[0;31;5;40;100m@[0;34;40m    [0m   [0;31;5;41;101m                                  [0m   \n"
"[0;34;40m    [0;1;30;90;41m8[0;1;31;91;43m88888888[0;1;33;93;43m...:[0;1;30;90;43mS[0;31;40mt[0;34;40m    [0;1;30;90;41m8[0;1;33;93;43m:%[0;33;5;43;103m%tttt;;;:[0;1;30;90;43mX[0;34;40m    [0m   [0;31;5;41;101mX                                X[0m   \n"
"[0;34;40m    [0;1;30;90;41m8[0;1;31;91;43m8888888888[0;1;30;90;43mS[0;1;30;90;41m8[0;31;40m [0;34;40m   [0;31;40m:[0;1;30;90;43m%[0;1;33;93;43m;ttt%[0;33;5;43;103m%tttt;;[0;1;30;90;43mX[0;34;40m    [0m    [0;31;5;41;101m8                              8[0m    \n"
"[0;34;40m    [0;31;40m%[0;1;31;91;43m888888888[0;1;30;90;43m%[0;31;40mt[0;34;40m    [0;30;41m8[0;1;30;90;43mS[0;1;33;93;43m:;;;tt%%[0;33;5;43;103m%ttt;[0;1;30;90;41m8[0;34;40m    [0m      [0;31;5;41;101m:                          :[0m      \n"
" [0;34;40m    [0;31;43m8t[0;1;31;91;43m888888[0;33;41m8[0;31;40m [0;34;40m    [0;31;40mS[0;33;41m888[0;31;43m8888[0;1;30;90;43mS[0;1;33;93;43mtt%%[0;33;5;43;103m%t[0;1;30;90;43m@[0;31;40m [0;34;40m   [0m        [0;1;31;91;41m:[0;31;5;41;101m:                      :[0;1;31;91;41m:[0m       \n"
" [0;34;40m    [0;31;40m.[0;31;43m@tt[0;1;31;91;43m888[0;31;43m@[0;34;40m              [0;1;30;90;41m8[0;1;33;93;43m;;ttt[0;1;30;90;43m@[0;31;40m;[0;34;40m    [0m           [0;31;5;41;101mt                  t[0m          \n"
"  [0;34;40m    [0;31;40m.[0;31;43m8ttt[0;1;31;91;43m8[0;31;43m@[0;31;40mSSSSS[0;34;40m    [0;31;40mSXXXX[0;1;30;90;43m%[0;1;33;93;43m:;;;[0;1;30;90;43mX[0;31;40m;[0;34;40m    [0m              [0;31;5;41;101m8              8[0m            \n"
"   [0;34;40m     [0;31;40mX[0;31;43m8ttt[0;1;31;91;43m8888[0;1;30;90;43m%[0;34;40m    [0;1;30;90;43m%[0;1;31;91;43m88[0;1;33;93;43m...::[0;1;30;90;43mX[0;30;41m8[0;31;40m [0;34;40m    [0m                 [0;1;31;91;41mX[0;31;5;41;101m.        .[0;1;31;91;41mX[0m              \n"
"     [0;34;40m     [0;31;40m%[0;1;30;90;41m8[0;31;43m@tt[0;1;31;91;43m88[0;31;43m;[0;33;41m8888[0;1;30;90;43m%[0;1;31;91;43m8888[0;1;30;90;43m%[0;31;43m8[0;31;40mX[0;32;40m [0;34;40m    [0m                     [0;1;31;91;41m:[0;31;5;41;101m;    ;[0;1;31;91;41m:[0m                \n"
"       [0;34;40m      [0;31;40m:@[0;1;30;90;41m8[0;33;41m8[0;31;43m8@XXX@8[0;1;30;90;41m8[0;31;40m8:[0;34;40m      [0m                          [0;31;5;41;101mtt[0m                   \n"
"         [0;34;40m                      [0m                                                 \n"
"              [0;34;40m             [0m                                                     ";
