/** @file
 *****************************************************************************

 Implementation of interfaces for a timer to profile executions.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include <stdio.h>
#include <sys/time.h>

#include "tests/timer.h"

namespace libzerocash {

struct timeval tv_start;
struct timeval tv_end;

void timer_start() {
    printf("%s\n", "Starting Timer");
    gettimeofday(&tv_start, 0);
}

void timer_stop() {
    float elapsed;
    gettimeofday(&tv_end, 0);

    elapsed = float(tv_end.tv_sec-tv_start.tv_sec) + (tv_end.tv_usec-tv_start.tv_usec)/float(1000000);
    printf("%s [%fs]\n\n", "Stopping Timer", elapsed);
}

void timer_start(const std::string location) {
    printf("%s %s\n", "(enter)", location.c_str());
    gettimeofday(&tv_start, 0);
}

void timer_stop(const std::string location) {
    float elapsed;
    gettimeofday(&tv_end, 0);

    elapsed = float(tv_end.tv_sec-tv_start.tv_sec) + (tv_end.tv_usec-tv_start.tv_usec)/float(1000000);
    printf("%s %s [%fs]\n\n", "(leave)", location.c_str(), elapsed);
}

} /* namespace libzerocash */
