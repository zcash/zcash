/** @file
 *****************************************************************************

 Declaration of interfaces for a timer to profile executions.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef TIMER_H_
#define TIMER_H_

#include <string>

namespace libzerocash {

void timer_start();
void timer_stop();

void timer_start(const std::string location);
void timer_stop(const std::string location);

} /* namespace libzerocash */

#endif /* TIMER_H_ */

