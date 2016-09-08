// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "asyncrpcoperation.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <string>
#include <ctime>
#include <chrono>

using namespace std;
using namespace json_spirit;

static boost::uuids::random_generator uuidgen;

std::map<OperationStatus, std::string> OperationStatusMap = {
    {OperationStatus::READY, "queued"},
    {OperationStatus::EXECUTING, "executing"},
    {OperationStatus::CANCELLED, "cancelled"},
    {OperationStatus::FAILED, "failed"},
    {OperationStatus::SUCCESS, "success"}
};

/**
 * Every operation instance should have a globally unique id
 */
AsyncRPCOperation::AsyncRPCOperation() : error_code_(0), error_message_() {
    // Set a unique reference for each operation
    boost::uuids::uuid uuid = uuidgen();
    id_ = "opid-" + boost::uuids::to_string(uuid);
    creation_time_ = (int64_t)time(NULL);
    set_state(OperationStatus::READY);
}

AsyncRPCOperation::AsyncRPCOperation(const AsyncRPCOperation& o) :
        id_(o.id_), creation_time_(o.creation_time_), state_(o.state_.load()),
        start_time_(o.start_time_), end_time_(o.end_time_),
        error_code_(o.error_code_), error_message_(o.error_message_),
        result_(o.result_)
{
}

AsyncRPCOperation& AsyncRPCOperation::operator=( const AsyncRPCOperation& other ) {
    this->id_ = other.id_;
    this->creation_time_ = other.creation_time_;
    this->state_.store(other.state_.load());
    this->start_time_ = other.start_time_;
    this->end_time_ = other.end_time_;
    this->error_code_ = other.error_code_;
    this->error_message_ = other.error_message_;
    this->result_ = other.result_;
    return *this;
}


AsyncRPCOperation::~AsyncRPCOperation() {
}

/**
 * Override this cancel() method if you can interrupt main() when executing.
 */
void AsyncRPCOperation::cancel() {
    if (isReady()) {
        set_state(OperationStatus::CANCELLED);
    }
}

/**
 * Start timing the execution run of the code you're interested in
 */
void AsyncRPCOperation::start_execution_clock() {
    std::lock_guard<std::mutex> guard(lock_);
    start_time_ = std::chrono::system_clock::now();
}

/**
 * Stop timing the execution run
 */
void AsyncRPCOperation::stop_execution_clock() {
    std::lock_guard<std::mutex> guard(lock_);
    end_time_ = std::chrono::system_clock::now();
}

/**
 * Implement this virtual method in any subclass.  This is just an example implementation.
 */
void AsyncRPCOperation::main() {
    if (isCancelled()) {
        return;
    }
    
    set_state(OperationStatus::EXECUTING);

    start_execution_clock();

    // Do some work here..

    stop_execution_clock();

    // If there was an error, you might set it like this:
    /*
    setErrorCode(123);
    setErrorMessage("Murphy's law");
    setState(OperationStatus::FAILED);
    */

    // Otherwise, if the operation was a success:
    Value v("We have a result!");
    set_result(v);
    set_state(OperationStatus::SUCCESS);
}

/**
 * Return the error of the completed operation as a Value object.
 * If there is no error, return null Value.
 */
Value AsyncRPCOperation::getError() const {
    if (!isFailed()) {
        return Value::null;
    }

    std::lock_guard<std::mutex> guard(lock_);
    Object error;
    error.push_back(Pair("code", this->error_code_));
    error.push_back(Pair("message", this->error_message_));
    return Value(error);
}

/**
 * Return the result of the completed operation as a Value object.
 * If the operation did not succeed, return null Value.
 */
Value AsyncRPCOperation::getResult() const {
    if (!isSuccess()) {
        return Value::null;
    }

    std::lock_guard<std::mutex> guard(lock_);
    return this->result_;
}


/**
 * Returns a status Value object.
 * If the operation has failed, it will include an error object.
 * If the operation has succeeded, it will include the result value.
 * If the operation was cancelled, there will be no error object or result value.
 */
Value AsyncRPCOperation::getStatus() const {
    OperationStatus status = this->getState();
    Object obj;
    obj.push_back(Pair("id", this->id_));
    obj.push_back(Pair("status", OperationStatusMap[status]));
    obj.push_back(Pair("creation_time", this->creation_time_));
    // TODO: Issue #1354: There may be other useful metadata to return to the user.
    Value err = this->getError();
    if (!err.is_null()) {
        obj.push_back(Pair("error", err.get_obj()));
    }
    Value result = this->getResult();
    if (!result.is_null()) {
        obj.push_back(Pair("result", result));

        // Include execution time for successful operation
        std::chrono::duration<double> elapsed_seconds = end_time_ - start_time_;
        obj.push_back(Pair("execution_secs", elapsed_seconds.count()));

    }
    return Value(obj);
}

/**
 * Return the operation state in human readable form.
 */
std::string AsyncRPCOperation::getStateAsString() const {
    OperationStatus status = this->getState();
    return OperationStatusMap[status];
}
