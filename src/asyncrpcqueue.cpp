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

#include "asyncrpcqueue.h"

static std::atomic<size_t> workerCounter(0);

/**
 * Static method to return the shared/default queue.
 */
shared_ptr<AsyncRPCQueue> AsyncRPCQueue::sharedInstance() {
    // Thread-safe in C+11 and gcc 4.3
    static shared_ptr<AsyncRPCQueue> q = std::make_shared<AsyncRPCQueue>();
    return q;
}

AsyncRPCQueue::AsyncRPCQueue() : closed_(false), finish_(false) {
}

AsyncRPCQueue::~AsyncRPCQueue() {
    closeAndWait();     // join on all worker threads
}

/**
 * A worker will execute this method on a new thread
 */
void AsyncRPCQueue::run(size_t workerId) {

    while (true) {
        AsyncRPCOperationId key;
        std::shared_ptr<AsyncRPCOperation> operation;
        {
            std::unique_lock<std::mutex> guard(lock_);
            while (operation_id_queue_.empty() && !isClosed() && !isFinishing()) {
                this->condition_.wait(guard);
            }

            // Exit if the queue is empty and we are finishing up
            if (isFinishing() && operation_id_queue_.empty()) {
                break;
            }

            // Exit if the queue is closing.
            if (isClosed()) {
                while (!operation_id_queue_.empty()) {
                    operation_id_queue_.pop();
                }
                break;
            }

            // Get operation id
            key = operation_id_queue_.front();
            operation_id_queue_.pop();

            // Search operation map
            AsyncRPCOperationMap::const_iterator iter = operation_map_.find(key);
            if (iter != operation_map_.end()) {
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
}


/**
 * Add shared_ptr to operation.
 *
 * To retain polymorphic behaviour, i.e. main() method of derived classes is invoked,
 * caller should create the shared_ptr like this:
 *
 * std::shared_ptr<AsyncRPCOperation> ptr(new MyCustomAsyncRPCOperation(params));
 *
 * Don't use std::make_shared<AsyncRPCOperation>().
 */
void AsyncRPCQueue::addOperation(const std::shared_ptr<AsyncRPCOperation> &ptrOperation) {
    std::lock_guard<std::mutex> guard(lock_);

    // Don't add if queue is closed or finishing
    if (isClosed() || isFinishing()) {
        return;
    }

    AsyncRPCOperationId id = ptrOperation->getId();
    operation_map_.emplace(id, ptrOperation);
    operation_id_queue_.push(id);
    this->condition_.notify_one();
}

/**
 * Return the operation for a given operation id.
 */
std::shared_ptr<AsyncRPCOperation> AsyncRPCQueue::getOperationForId(AsyncRPCOperationId id) const {
    std::shared_ptr<AsyncRPCOperation> ptr;

    std::lock_guard<std::mutex> guard(lock_);
    AsyncRPCOperationMap::const_iterator iter = operation_map_.find(id);
    if (iter != operation_map_.end()) {
        ptr = iter->second;
    }
    return ptr;
}

/**
 * Return the operation for a given operation id and then remove the operation from internal storage.
 */
std::shared_ptr<AsyncRPCOperation> AsyncRPCQueue::popOperationForId(AsyncRPCOperationId id) {
    std::shared_ptr<AsyncRPCOperation> ptr = getOperationForId(id);
    if (ptr) {
        std::lock_guard<std::mutex> guard(lock_);
        // Note: if the id still exists in the operationIdQueue, when it gets processed by a worker
        // there will no operation in the map to execute, so nothing will happen.
        operation_map_.erase(id);
    }
    return ptr;
}

/**
 * Return true if the queue is closed to new operations.
 */
bool AsyncRPCQueue::isClosed() const {
    return closed_.load();
}

/**
 * Close the queue and cancel all existing operations
 */
void AsyncRPCQueue::close() {
    closed_.store(true);
    cancelAllOperations();
}

/**
 * Return true if the queue is finishing up
 */
bool AsyncRPCQueue::isFinishing() const {
    return finish_.load();
}

/**
 * Close the queue but finish existing operations. Do not accept new operations.
 */
void AsyncRPCQueue::finish() {
    finish_.store(true);
}

/**
 *  Call cancel() on all operations
 */
void AsyncRPCQueue::cancelAllOperations() {
    std::lock_guard<std::mutex> guard(lock_);
    for (auto key : operation_map_) {
        key.second->cancel();
    }
    this->condition_.notify_all();
}

/**
 * Return the number of operations in the queue
 */
size_t AsyncRPCQueue::getOperationCount() const {
    std::lock_guard<std::mutex> guard(lock_);
    return operation_id_queue_.size();
}

/**
 * Spawn a worker thread
 */
void AsyncRPCQueue::addWorker() {
    std::lock_guard<std::mutex> guard(lock_);
    workers_.emplace_back( std::thread(&AsyncRPCQueue::run, this, ++workerCounter) );
}

/**
 * Return the number of worker threads spawned by the queue
 */
size_t AsyncRPCQueue::getNumberOfWorkers() const {
    std::lock_guard<std::mutex> guard(lock_);
    return workers_.size();
}

/**
 * Return a list of all known operation ids found in internal storage.
 */
std::vector<AsyncRPCOperationId> AsyncRPCQueue::getAllOperationIds() const {
    std::lock_guard<std::mutex> guard(lock_);
    std::vector<AsyncRPCOperationId> v;
    for(auto & entry: operation_map_) {
        v.push_back(entry.first);
    }
    return v;
}

/**
 * Calling thread will close and wait for worker threads to join.
 */
void AsyncRPCQueue::closeAndWait() {
    close();
    wait_for_worker_threads();
}

/**
 * Block current thread until all workers have finished their tasks.
 */
void AsyncRPCQueue::finishAndWait() {
    finish();
    wait_for_worker_threads();
}

/**
 * Block current thread until all operations are finished or the queue has closed.
 */
void AsyncRPCQueue::wait_for_worker_threads() {
    // Notify any workers who are waiting, so they see the updated queue state
    {
        std::lock_guard<std::mutex> guard(lock_);
        this->condition_.notify_all();
    }
        
    for (std::thread & t : this->workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
}
