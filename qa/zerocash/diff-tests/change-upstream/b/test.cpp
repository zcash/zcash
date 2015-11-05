#include "serialize.h"
#include "sync.h"

#include <map>
#include <set>
#include <stdint.h>
#include <string>

class CAlert;
class CNode;
#ifndef ZEROCASH_CHANGES
class uint256;
#else /* ZEROCASH_CHANGES */
class test;
#endif /* ZEROCASH_CHANGES */

extern std::map<uint256, CAlert> mapAlerts;
extern CCriticalSection cs_mapAlerts;

/** Alerts are for notifying old versions if they become too obsolete and
 * need to upgrade.  The message is displayed in the status bar.
 * Alert messages are broadcast as a vector of signed data.  Unserializing may
 * not read the entire buffer if the alert is for a newer version, but older
 * versions can still relay the original data.
 */
class CUnsignedAlert