// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#ifndef ASYNCRPCOPERATION_H
#define ASYNCRPCOPERATION_H

#include <string>
#include <atomic>
#include <map>
#include <chrono>

#include "json/json_spirit_value.h"
#include "json/json_spirit_utils.h"
#include "json/json_spirit_reader_template.h"
#include "json/json_spirit_writer_template.h"

using namespace std;
using namespace json_spirit;

/**
 * AsyncRPCOperations are given to the AsyncRPCQueue for processing.
 * 
 * How to subclass:
 * Implement the main() method, this is where work is performed.
 * Update the operation status as work is underway and completes.
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
    
    // Todo: keep or delete copy constructors and assignment?
    AsyncRPCOperation(const AsyncRPCOperation& orig);
    AsyncRPCOperation& operator=( const AsyncRPCOperation& other );

    virtual ~AsyncRPCOperation();

    // Implement this method in your subclass.
    virtual void main();

    void cancel();
    
    // Getters and setters

    OperationStatus getState() const {
        return state.load();
    }
        
    AsyncRPCOperationId getId() const {
        return id;
    }
   
    int64_t getCreationTime() const {
        return creationTime;
    }

    Value getStatus() const;

    Value getError() const;
    
    Value getResult() const;

    std::string getStateAsString() const;
    
    int getErrorCode() const {
        return errorCode;
    }

    std::string getErrorMessage() const {
        return errorMessage;
    }

    bool isCancelled() const {
        return OperationStatus::CANCELLED==getState();
    }

    bool isExecuting() const {
        return OperationStatus::EXECUTING==getState();
    }

    bool isReady() const {
        return OperationStatus::READY==getState();
    }

    bool isFailed() const {
        return OperationStatus::FAILED==getState();
    }
    
    bool isSuccess() const {
        return OperationStatus::SUCCESS==getState();
    }

protected:

    Value resultValue;
    int errorCode;
    std::string errorMessage;
    std::atomic<OperationStatus> state;
    std::chrono::time_point<std::chrono::system_clock> startTime, endTime;  

    void startExecutionClock();
    void stopExecutionClock();

    void setState(OperationStatus state) {
        this->state.store(state);
    }

    void setErrorCode(int errorCode) {
        this->errorCode = errorCode;
    }

    void setErrorMessage(std::string errorMessage) {
        this->errorMessage = errorMessage;
    }
    
    void setResult(Value v) {
        this->resultValue = v;
    }
    
private:
    
    // Todo: Private for now.  If copying an operation is possible, should it
    // receive a new id and a new creation time?
    void setId(AsyncRPCOperationId id) {
        this->id = id;
    }
    
    // Todo: Ditto above.
    void setCreationTime(int64_t creationTime) {
        this->creationTime = creationTime;
    }

    AsyncRPCOperationId id;
    int64_t creationTime;
};

#endif /* ASYNCRPCOPERATION_H */

