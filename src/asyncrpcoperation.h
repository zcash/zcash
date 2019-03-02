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

#ifndef ASYNCRPCOPERATION_H
#define ASYNCRPCOPERATION_H

#include <string>
#include <atomic>
#include <map>
#include <chrono>
#include <memory>
#include <thread>
#include <utility>
#include <future>

#include <univalue.h>

using namespace std;

/**
 * AsyncRPCOperation objects are submitted to the AsyncRPCQueue for processing.
 * 
 * To subclass AsyncRPCOperation, implement the main() method.
 * Update the operation status as work is underway and completes.
 * If main() can be interrupted, implement the cancel() method.
 */

typedef std::string AsyncRPCOperationId;

typedef enum class operationStateEnum {
    READY = 0,
    EXECUTING,
    CANCELLED,
    FAILED,
    SUCCESS
} OperationStatus;

class AsyncRPCOperation {
public:
    AsyncRPCOperation();
    virtual ~AsyncRPCOperation();

    // You must implement this method in your subclass.
    virtual void main();

    // Override this method if you can interrupt execution of main() in your subclass.
    void cancel();
    
    // Getters and setters

    OperationStatus getState() const {
        return state_.load();
    }
        
    AsyncRPCOperationId getId() const {
        return id_;
    }
   
    int64_t getCreationTime() const {
        return creation_time_;
    }

    // Override this method to add data to the default status object.
    virtual UniValue getStatus() const;

    UniValue getError() const;
    
    UniValue getResult() const;

    std::string getStateAsString() const;
    
    int getErrorCode() const {
        std::lock_guard<std::mutex> guard(lock_);
        return error_code_;
    }

    std::string getErrorMessage() const {
        std::lock_guard<std::mutex> guard(lock_);
        return error_message_;
    }

    bool isCancelled() const {
        return OperationStatus::CANCELLED == getState();
    }

    bool isExecuting() const {
        return OperationStatus::EXECUTING == getState();
    }

    bool isReady() const {
        return OperationStatus::READY == getState();
    }

    bool isFailed() const {
        return OperationStatus::FAILED == getState();
    }
    
    bool isSuccess() const {
        return OperationStatus::SUCCESS == getState();
    }

protected:
    // The state_ is atomic because only it can be mutated externally.
    // For example, the user initiates a shut down of the application, which closes
    // the AsyncRPCQueue, which in turn invokes cancel() on all operations.
    // The member variables below are protected rather than private in order to
    // allow subclasses of AsyncRPCOperation the ability to access and update
    // internal state.  Currently, all operations are executed in a single-thread
    // by a single worker.
    mutable std::mutex lock_;   // lock on this when read/writing non-atomics
    UniValue result_;
    int error_code_;
    std::string error_message_;
    std::atomic<OperationStatus> state_;
    std::chrono::time_point<std::chrono::system_clock> start_time_, end_time_;  

    void start_execution_clock();
    void stop_execution_clock();

    void set_state(OperationStatus state) {
        this->state_.store(state);
    }

    void set_error_code(int errorCode) {
        std::lock_guard<std::mutex> guard(lock_);
        this->error_code_ = errorCode;
    }

    void set_error_message(std::string errorMessage) {
        std::lock_guard<std::mutex> guard(lock_);
        this->error_message_ = errorMessage;
    }
    
    void set_result(UniValue v) {
        std::lock_guard<std::mutex> guard(lock_);
        this->result_ = v;
    }
    
private:

    // Derived classes should write their own copy constructor and assignment operators
    AsyncRPCOperation(const AsyncRPCOperation& orig);
    AsyncRPCOperation& operator=( const AsyncRPCOperation& other );

    // Initialized in the operation constructor, never to be modified again.
    AsyncRPCOperationId id_;
    int64_t creation_time_;
};

#endif /* ASYNCRPCOPERATION_H */

