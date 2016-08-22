// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcqueue.h"

static std::atomic<int> workerCounter(0);

AsyncRPCQueue::AsyncRPCQueue() : closed(false) {
}

/*
 * Calling thread will join on all the worker threads
 */
AsyncRPCQueue::~AsyncRPCQueue() {
    this->closed = true; // set this in case close() was not invoked
    for (std::thread & t : this->workers) {
        t.join();
    }
}

/*
 * A worker will execute this method on a new thread
 */
void AsyncRPCQueue::run(int workerId) {
//    std::cout << "Launched queue worker " << workerId << std::endl;

    while (!isClosed()) {
        AsyncRPCOperationId key;
        std::shared_ptr<AsyncRPCOperation> operation;
        {
            std::unique_lock< std::mutex > guard(cs_lock);
            while (operationIdQueue.empty() && !isClosed()) {
                this->cs_condition.wait(guard);
            }

            // Exit if the queue is closing.
            if (isClosed())
                break;

            // Get operation id
            key = operationIdQueue.front();
            operationIdQueue.pop();

            // Search operation map
            AsyncRPCOperationMap::const_iterator iter = operationMap.find(key);
            if (iter != operationMap.end()) {
                operation = iter->second;
            }
        }

        if (!operation) {
            // cannot find operation in map, may have been removed
        } else if (operation->isCancelled()) {
            // skip cancelled operation
        } else {
            operation->main();
        }
    }
//    std::cout << "Terminating queue worker " << workerId << std::endl;
}


/*
 * Add shared_ptr to operation.
 *
 * To retain polymorphic behaviour, i.e. main() method of derived classes is invoked,
 * caller should create the shared_ptr like thi:
 *
 * std::shared_ptr<AsyncRPCOperation> ptr(new MyCustomAsyncRPCOperation(params));
 *
 * Don't use std::make_shared<AsyncRPCOperation>().
 */
void AsyncRPCQueue::addOperation(const std::shared_ptr<AsyncRPCOperation> &ptrOperation) {

    // Don't add if queue is closed
    if (isClosed())
        return;

    AsyncRPCOperationId id = ptrOperation->getId();
    {
        std::lock_guard< std::mutex > guard(cs_lock);
        operationMap.emplace(id, ptrOperation);
        operationIdQueue.push(id);
        this->cs_condition.notify_one();
    }
}


std::shared_ptr<AsyncRPCOperation> AsyncRPCQueue::getOperationForId(AsyncRPCOperationId id) {
    std::shared_ptr<AsyncRPCOperation> ptr;

    std::lock_guard< std::mutex > guard(cs_lock);
    AsyncRPCOperationMap::const_iterator iter = operationMap.find(id);
    if (iter != operationMap.end()) {
        ptr = iter->second;
    }
    return ptr;
}

std::shared_ptr<AsyncRPCOperation> AsyncRPCQueue::popOperationForId(AsyncRPCOperationId id) {
    std::shared_ptr<AsyncRPCOperation> ptr = getOperationForId(id);
    if (ptr) {
        std::lock_guard< std::mutex > guard(cs_lock);
        // Note: if the id still exists in the operationIdQueue, when it gets processed by a worker
        // there will no operation in the map to execute, so nothing will happen.
        operationMap.erase(id);
    }
    return ptr;
}

bool AsyncRPCQueue::isClosed() {
    return closed;
}

void AsyncRPCQueue::close() {
    this->closed = true;
    cancelAllOperations();
}

/*
 *  Call cancel() on all operations
 */
void AsyncRPCQueue::cancelAllOperations() {
    std::unique_lock< std::mutex > guard(cs_lock);
    for (auto key : operationMap) {
        key.second->cancel();
    }
    this->cs_condition.notify_all();
}

int AsyncRPCQueue::getOperationCount() {
    std::unique_lock< std::mutex > guard(cs_lock);
    return operationIdQueue.size();
}

/*
 * Spawn a worker thread
 */
void AsyncRPCQueue::addWorker() {
    std::unique_lock< std::mutex > guard(cs_lock); // Todo: could just have a lock on the vector
    workers.emplace_back( std::thread(&AsyncRPCQueue::run, this, ++workerCounter) );
}

int AsyncRPCQueue::getNumberOfWorkers() {
    return workers.size();
}


std::vector<AsyncRPCOperationId> AsyncRPCQueue::getAllOperationIds() {
    std::unique_lock< std::mutex > guard(cs_lock);
    std::vector<AsyncRPCOperationId> v;
    for(auto & entry: operationMap)
        v.push_back(entry.first);
    return v;
}

