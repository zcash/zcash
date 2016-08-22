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
    int getNumberOfWorkers();
    bool isClosed();
    void close();
    void cancelAllOperations();
    int getOperationCount();
    std::shared_ptr<AsyncRPCOperation> getOperationForId(AsyncRPCOperationId);
    std::shared_ptr<AsyncRPCOperation> popOperationForId(AsyncRPCOperationId);
    void addOperation(const std::shared_ptr<AsyncRPCOperation> &ptrOperation);
    std::vector<AsyncRPCOperationId> getAllOperationIds();

private:
    // addWorker() will spawn a new thread on this method
    void run(int workerId);

    std::mutex cs_lock;
    std::condition_variable cs_condition;
    bool closed;
    AsyncRPCOperationMap operationMap;
    std::queue <AsyncRPCOperationId> operationIdQueue;
    std::vector<std::thread> workers;
};

#endif


