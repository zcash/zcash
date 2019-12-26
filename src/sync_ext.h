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

#ifndef __SYNC_EXT_H__
#define __SYNC_EXT_H__

#include "cc/CCinclude.h"
#include "sync.h"

// sync extension 

/* conditional lock */
template <typename Mutex>
class SCOPED_LOCKABLE CMutexLockConditional
{
private:
    boost::unique_lock<Mutex> lock;

    void Enter(const char* pszName, const char* pszFile, int nLine)
    {
        EnterCritical(pszName, pszFile, nLine, (void*)(lock.mutex()));
#ifdef DEBUG_LOCKCONTENTION
        if (!lock.try_lock()) {
            PrintLockContention(pszName, pszFile, nLine);
#endif
            lock.lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
    }

public:
    CMutexLockConditional(Mutex& mutexIn, const char* pszName, const char* pszFile, int nLine, bool doLock) EXCLUSIVE_LOCK_FUNCTION(mutexIn) : lock(mutexIn, boost::defer_lock)
    {
        if (doLock) {
            Enter(pszName, pszFile, nLine);
            if (lock.owns_lock())
                LOGSTREAMFN("lock-conditional", CCLOG_DEBUG1, stream << "cs=" << pszName << " file=" << pszFile << " nLine=" << nLine << " entered, locked" << std::endl);
        }
    }

    CMutexLockConditional(Mutex* pmutexIn, const char* pszName, const char* pszFile, int nLine, bool doLock) EXCLUSIVE_LOCK_FUNCTION(pmutexIn)
    {
        if (!pmutexIn) return;

        lock = boost::unique_lock<Mutex>(*pmutexIn, boost::defer_lock);
        if (doLock)
            Enter(pszName, pszFile, nLine);
    }

    ~CMutexLockConditional() UNLOCK_FUNCTION()
    {
        if (lock.owns_lock()) {
            LeaveCritical();
            LOGSTREAMFN("lock-conditional", CCLOG_DEBUG1, stream << "exited, was locked" << std::endl);
        }
    }

    operator bool()
    {
        return lock.owns_lock();
    }
};

typedef CMutexLockConditional<CCriticalSection> CCriticalBlockConditional;

#define CONDITIONAL_LOCK(cs, flag) CCriticalBlockConditional criticalblock(cs, #cs, __FILE__, __LINE__, flag)
#define CONDITIONAL_LOCK2(cs1, cs2, flag) CCriticalBlockConditional criticalblock1(cs1, #cs1, __FILE__, __LINE__, flag), criticalblock2(cs2, #cs2, __FILE__, __LINE__, flag)

#endif