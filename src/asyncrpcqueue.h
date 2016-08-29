// Copyright (c) 2014 The Zcash developers
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
    void close();
    void closeAndWait();
    void cancelAllOperations();
    size_t getOperationCount() const;
    std::shared_ptr<AsyncRPCOperation> getOperationForId(AsyncRPCOperationId) const;
    std::shared_ptr<AsyncRPCOperation> popOperationForId(AsyncRPCOperationId);
    void addOperation(const std::shared_ptr<AsyncRPCOperation> &ptrOperation);
    std::vector<AsyncRPCOperationId> getAllOperationIds() const;

private:
    // addWorker() will spawn a new thread on this method
    void run(size_t workerId);

    // Why this is not a recursive lock: http://www.zaval.org/resources/library/butenhof1.html
    mutable std::mutex lock_;
    std::condition_variable condition_;
    bool closed_;
    AsyncRPCOperationMap operation_map_;
    std::queue <AsyncRPCOperationId> operation_id_queue_;
    std::vector<std::thread> workers_;
};

#endif


