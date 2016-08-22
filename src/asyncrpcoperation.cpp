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

AsyncRPCOperation::AsyncRPCOperation() : errorCode(0), errorMessage() {
    // Set a unique reference for each operation
    boost::uuids::uuid uuid = uuidgen();
    std::string s = "opid-" + boost::uuids::to_string(uuid);
    setId(s);
    
    setState(OperationStatus::READY);
    creationTime = (int64_t)time(NULL);
}

AsyncRPCOperation::AsyncRPCOperation(const AsyncRPCOperation& o) : id(o.id), creationTime(o.creationTime), state(o.state.load())
{
}


AsyncRPCOperation& AsyncRPCOperation::operator=( const AsyncRPCOperation& other ) {
    this->id = other.getId();
    this->creationTime = other.creationTime;
    this->state.store(other.state.load());
    return *this;
}


AsyncRPCOperation::~AsyncRPCOperation() {
}

void AsyncRPCOperation::cancel() {
    if (isReady())
        setState(OperationStatus::CANCELLED);
}


void AsyncRPCOperation::startExecutionClock() {
    startTime = std::chrono::system_clock::now();
}

void AsyncRPCOperation::stopExecutionClock() {
    endTime = std::chrono::system_clock::now();
}


// Implement this method in any subclass.
// This is just an example implementation.


void AsyncRPCOperation::main() {
    if (isCancelled())
        return;
    
    setState(OperationStatus::EXECUTING);

    //
    // Do some work here...
    //

    startExecutionClock();

    //std::this_thread::sleep_for(std::chrono::milliseconds(10000));

    stopExecutionClock();


    // If there was an error...
//    setErrorCode(123);
//    setErrorMessage("Murphy's law");
//    setState(OperationStatus::FAILED);

    
    // Otherwise
    Value v("We have a result!");
    setResult(v);
    setState(OperationStatus::SUCCESS);
}

Value AsyncRPCOperation::getError() const {
    if (!isFailed())
        return Value::null;

    Object error;
    error.push_back(Pair("code", this->errorCode));
    error.push_back(Pair("message", this->errorMessage));
    return Value(error);
}

Value AsyncRPCOperation::getResult() const {
     if (!isSuccess())
        return Value::null;

     return this->resultValue;
}


/*
 * Returns a status Value object.
 * If the operation has failed, it will include an error object.
 * If the operation has succeeded, it will include the result value.
 */
Value AsyncRPCOperation::getStatus() const {
    OperationStatus status = this->getState();
    Object obj;
    obj.push_back(Pair("id", this->getId()));
    obj.push_back(Pair("status", OperationStatusMap[status]));
    obj.push_back(Pair("creation_time", this->creationTime));
    // creation, exec time, duration, exec end, etc.
    Value err = this->getError();
    if (!err.is_null()) {
        obj.push_back(Pair("error", err.get_obj()));
    }
    Value result = this->getResult();
    if (!result.is_null()) {
        obj.push_back(Pair("result", result));

        // Include execution time for successful operation
        std::chrono::duration<double> elapsed_seconds = endTime - startTime;
        obj.push_back(Pair("execution_secs", elapsed_seconds.count()));

    }
    return Value(obj);
}


std::string AsyncRPCOperation::getStateAsString() const {
    OperationStatus status = this->getState();
    return OperationStatusMap[status];
}
