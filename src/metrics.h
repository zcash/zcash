// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

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

    uint64_t get() const {
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
    double rate(const int64_t count);

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
"              [0;1;36;96;47m8[0;36;5;46;106mSSS%%%%tt[0;37;5;46;106m8[0;1;36;96;47m@[0m                                                      \n"
"         [0;1;36;96;47mX[0;1;36;96;46m@SX@@8[0;36;5;46;106m8@XS%%%%%%%%t[0;37;5;46;106m8[0m                                                  \n"
"       [0;1;30;90;46m@   [0;1;36;96;46m%SS8[0;37;5;46;106m8[0;1;36;96;47m8[0;1;37;97;47m88888[0;1;36;96;47m@[0;37;5;46;106m8[0;36;5;46;106m%%%%%%%t[0;1;36;96;47m@[0m                [0;1;31;91;41m:[0;31;5;41;101m@.    .X[0;1;31;91;41mt[0m       [0;1;31;91;41m;[0;31;5;41;101m@.    .X[0;1;31;91;41mt[0m    \n"
"     [0;1;30;90;46mS    :[0;36;47m8[0;1;37;97;47m8[0;37;5;47;107m:         .[0;37;5;46;106mS[0;36;5;46;106m%%%%%%%%%[0m             [0;31;5;41;101m8            X[0m   [0;31;5;41;101m@            @[0m  \n"
"   [0;1;30;90;46m8    t[0;1;37;97;47mX[0;37;5;47;107m:  :[0;1;37;97;47m8[0;1;36;96;47mX[0;37;46m@[0;1;36;96;46m@@@8[0;36;5;46;106m8@@XS%[0;37;5;47;107m%:;[0;1;36;96;47mS[0;36;5;46;106m%%%%t[0m          [0;31;5;41;101m:               8               .[0m \n"
"  [0;1;30;90;46m8    [0;36;47m8[0;37;5;47;107m:  [0;1;37;97;47m8[0;37;46m8[0;1;30;90;46m     [0;1;36;96;46mSSXX@@8[0;36;5;46;106m@[0;1;37;97;47m8[0;37;5;47;107m   .[0;36;5;46;106mt%%%%t[0m        [0;31;5;41;101m8                                 X[0m\n"
" [0;36;5;40;100mt[0;34;46m@XX[0;1;30;90;46m [0;1;37;97;47m;[0;37;5;47;107m  ;[0;36;47m8[0;1;30;90;46m         [0;1;36;96;46mSSXX@[0;1;36;96;47m@[0;37;5;47;107m:S[0;1;37;97;47m8[0;1;36;96;47mX[0;36;5;46;106m%%%%%%%t[0m       [0;31;5;41;101m;                                  [0m\n"
" [0;34;46m888@[0;1;30;90;47mS[0;37;5;47;107m  ;[0;1;30;90;46m8      [0;36;47m8[0;1;37;97;47m8[0;37;5;47;107mXSS[0;1;37;97;47m88[0;37;5;47;107mS:[0;1;36;96;47mS[0;1;36;96;46m@@8[0;36;5;46;106m8@S[0;37;5;46;106m8[0;36;5;46;106mt%%%%[0;37;5;46;106m8[0m      [0;31;5;41;101m%                                 :[0m\n"
"[0;36;5;40;100mX[0;36;44m888[0;34;46m8[0;37;5;47;107m%  [0;36;47m8[0;1;30;90;46m     @[0;37;5;47;107mS        t[0;1;36;96;46mSSX@@8[0;37;5;47;107m. .[0;36;5;46;106m%%%%%[0m      [0;1;31;91;41mt[0;31;5;41;101m                                 [0;1;31;91;41mX[0m\n"
"[0;1;30;90;44m8[0;36;44m@@8[0;36;5;40;100m8[0;37;5;47;107m  :[0;34;46m@@X[0;1;30;90;46m   [0;1;37;97;47m8[0;37;5;47;107m         .[0;1;30;90;46m:[0;1;36;96;46m%SSXX[0;37;5;47;107m%  [0;1;36;96;47m@[0;36;5;46;106mXS%%[0m       [0;31;5;41;101m;                               :[0m \n"
"[0;1;30;90;44m8[0;30;44m@@[0;36;44m@[0;36;5;40;100m8[0;37;5;47;107m  :[0;34;46m888@X[0;1;30;90;46m [0;1;37;97;47m8[0;37;5;47;107m         :[0;1;30;90;46m:  [0;1;36;96;46m%SS[0;37;5;47;107m%  [0;1;36;96;47m@[0;1;36;96;46m8[0;36;5;46;106m@XS[0m        [0;31;5;41;101mt                             ;[0m  \n"
"[0;36;5;40;100m@[0;30;44m@@@[0;1;30;90;44m8[0;37;5;47;107m%  [0;36;5;40;100m.[0;36;44m8[0;34;46m888@[0;1;30;90;46m8[0;37;5;47;107mX       :[0;36;47m8[0;1;30;90;46m     [0;1;36;96;46mX[0;37;5;47;107m.  [0;36;5;46;106m8[0;1;36;96;46m@@8[0;36;5;46;106m@[0m         [0;1;31;91;41mS[0;31;5;41;101m.                          [0;1;31;91;41m@[0m   \n"
" [0;30;44m@@@@[0;37;5;40;100m8[0;37;5;47;107m  t[0;36;5;40;100m8[0;36;44m88[0;34;46m888[0;1;30;90;46m:8[0;1;37;97;47mt8[0;37;5;47;107mX8[0;1;37;97;47m@[0;36;47m@[0;1;30;90;46mt     :[0;37;5;47;107mX  [0;1;37;97;47m8[0;1;36;96;46mSXX@[0;1;36;96;47m8[0m           [0;1;31;91;41m8[0;31;5;41;101m                       [0;1;31;91;41m8[0m     \n"
" [0;36;5;40;100m%[0;30;44m@@@@[0;1;30;90;47m:[0;37;5;47;107m  t[0;36;5;40;100m;[0;36;44m888[0;34;46m888@XX[0;1;30;90;46m        @[0;37;5;47;107mX  S[0;1;30;90;46m; [0;1;36;96;46mSS@[0m              [0;1;31;91;41mX[0;31;5;41;101m.                 .[0;1;31;91;41m@[0m       \n"
"  [0;36;5;40;100m8[0;30;44m@@@@[0;1;30;90;47m8[0;37;5;47;107m;  [0;1;37;97;47mS[0;36;5;40;100m%[0;36;44m888[0;34;46m888@@X[0;1;30;90;46m    8[0;1;37;97;47mS[0;37;5;47;107m. .[0;1;37;97;47m@[0;1;30;90;46m:   ;[0m                  [0;31;5;41;101m;             :[0;1;31;91;41m;[0m         \n"
"   [0;36;5;40;100mt[0;30;44m@@@@[0;34;5;40;100mX[0;1;30;90;47m;[0;37;5;47;107m;  %[0;1;37;97;47m:[0;1;30;90;47m8[0;36;5;40;100mX[0;1;30;90;46m@t[0;34;46m8[0;1;30;90;46m;S8[0;36;47m8[0;1;37;97;47mt[0;37;5;47;107mS  .[0;1;37;97;47m8[0;1;30;90;46m8    @[0m                     [0;31;5;41;101mt         t[0m            \n"
"     [0;34;5;40;100mX[0;30;44m@@@@[0;1;30;90;44m8[0;37;5;40;100m8[0;1;37;97;47mt[0;37;5;47;107mt           :[0;1;37;97;47m8[0;1;30;90;47m [0;1;30;90;46mX    .[0;36;47mS[0m                        [0;31;5;41;101m8     @[0m              \n"
"       [0;34;5;40;100mX[0;30;44m@@@@@@[0;36;5;40;100m8:[0;1;30;90;47m8t.[0;1;37;97;47m..[0;1;30;90;47m.t[0;37;5;40;100m8[0;1;30;90;46m8:[0;34;46m@X[0;1;30;90;46m   ;[0;36;47m@[0m                            [0;1;31;91;41m8[0;31;5;41;101m 8[0m                \n"
"         [0;37;5;40;100m8[0;34;5;40;100mX[0;30;44m@@@@@@@@@[0;36;44mX@888[0;34;46m888[0;1;30;90;46mt8[0m                                                  \n"
"              [0;36;5;40;100m X8[0;1;30;90;44m88[0;30;44m@@[0;1;30;90;44m8[0;36;5;40;100m88;[0;1;30;90;47mS[0m                                                      \n";
