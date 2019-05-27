// Copyright (c) 2015 The Bitcoin Core developers
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

#include "scheduler.h"

#include "reverselock.h"

#include <assert.h>
#include <boost/bind.hpp>
#include <utility>

CScheduler::CScheduler() : nThreadsServicingQueue(0), stopRequested(false), stopWhenEmpty(false)
{
}

CScheduler::~CScheduler()
{
    /*int32_t i;
    if ( nThreadsServicingQueue != 0 )
    {
        for (i=0; i<10; i++)
        {
            sleep(1);
            fprintf(stderr,"CScheduler nThreadsServicingQueue.%d\n",(int32_t)nThreadsServicingQueue);
            if ( nThreadsServicingQueue == 0 )
                break;
        }
    }*/
    assert(nThreadsServicingQueue == 0);
}


void CScheduler::serviceQueue()
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    ++nThreadsServicingQueue;

    // newTaskMutex is locked throughout this loop EXCEPT
    // when the thread is waiting or when the user's function
    // is called.
    while (!shouldStop()) {
        try {
            while (!shouldStop() && taskQueue.empty()) {
                // Wait until there is something to do.
                newTaskScheduled.wait(lock);
            }

            // Wait until either there is a new task, or until
            // the time of the first item on the queue:

            // Some boost versions have a conflicting overload of wait_until that returns void.
            // Explicitly use a template here to avoid hitting that overload.
            while (!shouldStop() && !taskQueue.empty() &&
                   newTaskScheduled.wait_until<>(lock, taskQueue.begin()->first) != boost::cv_status::timeout) {
                // Keep waiting until timeout
            }

            // If there are multiple threads, the queue can empty while we're waiting (another
            // thread may service the task we were waiting on).
            if (shouldStop() || taskQueue.empty())
                continue;

            Function f = taskQueue.begin()->second;
            taskQueue.erase(taskQueue.begin());

            {
                // Unlock before calling f, so it can reschedule itself or another task
                // without deadlocking:
                reverse_lock<boost::unique_lock<boost::mutex> > rlock(lock);
                f();
            }
        } catch (...) {
            --nThreadsServicingQueue;
            throw;
        }
    }
    --nThreadsServicingQueue;
}

void CScheduler::stop(bool drain)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        if (drain)
            stopWhenEmpty = true;
        else
            stopRequested = true;
    }
    newTaskScheduled.notify_all();
}

void CScheduler::schedule(CScheduler::Function f, boost::chrono::system_clock::time_point t)
{
    {
        boost::unique_lock<boost::mutex> lock(newTaskMutex);
        taskQueue.insert(std::make_pair(t, f));
    }
    newTaskScheduled.notify_one();
}

void CScheduler::scheduleFromNow(CScheduler::Function f, int64_t deltaSeconds)
{
    schedule(f, boost::chrono::system_clock::now() + boost::chrono::seconds(deltaSeconds));
}

static void Repeat(CScheduler* s, CScheduler::Function f, int64_t deltaSeconds)
{
    f();
    s->scheduleFromNow(boost::bind(&Repeat, s, f, deltaSeconds), deltaSeconds);
}

void CScheduler::scheduleEvery(CScheduler::Function f, int64_t deltaSeconds)
{
    scheduleFromNow(boost::bind(&Repeat, this, f, deltaSeconds), deltaSeconds);
}

size_t CScheduler::getQueueInfo(boost::chrono::system_clock::time_point &first,
                             boost::chrono::system_clock::time_point &last) const
{
    boost::unique_lock<boost::mutex> lock(newTaskMutex);
    size_t result = taskQueue.size();
    if (!taskQueue.empty()) {
        first = taskQueue.begin()->first;
        last = taskQueue.rbegin()->first;
    }
    return result;
}
