// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ASYNCRPCQUEUE_H
#define ASYNCRPCQUEUE_H

#include "asyncrpcoperation.h"

#include <iostream>
#include <string>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <vector>
#include <future>
#include <thread>
#include <utility>
#include <memory>


typedef std::unordered_map<AsyncRPCOperationId, std::shared_ptr<AsyncRPCOperation> > AsyncRPCOperationMap; 


class AsyncRPCQueue {
public:
    static shared_ptr<AsyncRPCQueue> sharedInstance();

    AsyncRPCQueue();
    virtual ~AsyncRPCQueue();

    // We don't want queue to be copied or moved around
    AsyncRPCQueue(AsyncRPCQueue const&) = delete;             // Copy construct
    AsyncRPCQueue(AsyncRPCQueue&&) = delete;                  // Move construct
    AsyncRPCQueue& operator=(AsyncRPCQueue const&) = delete;  // Copy assign
    AsyncRPCQueue& operator=(AsyncRPCQueue &&) = delete;      // Move assign

    void addWorker();
    size_t getNumberOfWorkers() const;
    bool isClosed() const;
    bool isFinishing() const;
    void close(); // close queue and cancel all operations
    void finish(); // close queue but finishing existing operations
    void closeAndWait(); // block thread until all threads have terminated.
    void finishAndWait(); // block thread until existing operations have finished, threads terminated
    void cancelAllOperations(); // mark all operations in the queue as cancelled
    size_t getOperationCount() const;
    std::shared_ptr<AsyncRPCOperation> getOperationForId(AsyncRPCOperationId) const;
    std::shared_ptr<AsyncRPCOperation> popOperationForId(AsyncRPCOperationId);
    void addOperation(const std::shared_ptr<AsyncRPCOperation> &ptrOperation);
    std::vector<AsyncRPCOperationId> getAllOperationIds() const;

private:
    // addWorker() will spawn a new thread on run())
    void run(size_t workerId);
    void wait_for_worker_threads();

    // Why this is not a recursive lock: http://www.zaval.org/resources/library/butenhof1.html
    mutable std::mutex lock_;
    std::condition_variable condition_;
    std::atomic<bool> closed_;
    std::atomic<bool> finish_;
    AsyncRPCOperationMap operation_map_;
    std::queue <AsyncRPCOperationId> operation_id_queue_;
    std::vector<std::thread> workers_;
};

#endif


