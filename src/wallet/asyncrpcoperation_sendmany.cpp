// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include "asyncrpcoperation_sendmany.h"

#include <iostream>
#include <chrono>
#include <thread>

AsyncRPCOperation_sendmany::AsyncRPCOperation_sendmany(std::string fromAddress, std::vector<SendManyRecipient> outputs, int minconf) : fromAddress(fromAddress), outputs(outputs), minconf(minconf)
{
}

AsyncRPCOperation_sendmany::AsyncRPCOperation_sendmany(const AsyncRPCOperation_sendmany& orig) {
}

AsyncRPCOperation_sendmany::~AsyncRPCOperation_sendmany() {
}

void AsyncRPCOperation_sendmany::main() {
    if (isCancelled())
        return;

    setState(OperationStatus::EXECUTING);
    startExecutionClock();

    /**
     *  Dummy run of a sendmany operation
     */
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::cout << std::endl << "z_sendmany: **************** DUMMY RUN *****************" << std::endl;
    std::cout << "z_sendmany: source of funds: " << fromAddress << std::endl;
    std::cout << "z_sendmany: minconf: " << minconf << std::endl;

    for (SendManyRecipient & t : outputs) {
        std::cout << "z_sendmany: send " << std::get<1>(t) << " to " << std::get<0>(t) << std::endl;
        std::string memo = std::get<2>(t);
        if (memo.size()>0) {
            std::cout << "          : memo = " << memo << std::endl;
        }
    }

    std::cout << "z_sendmany: checking balances and selecting coins and notes..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "z_sendmany: performing a joinsplit..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));

    std::cout << "z_sendmany: attempting to broadcast to network..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "z_sendmany: operation complete!" << std::endl;
    std::cout << "z_sendmany: ********************************************" << std::endl;

    stopExecutionClock();

    
    // dummy run will say that even number of outputs is success
    bool isEven = outputs.size() % 2 == 0;
    //std::cout<< "here is r: " << r << std::endl;
    if (isEven) {
        setState(OperationStatus::SUCCESS);
        Object obj;
        obj.push_back(Pair("dummy_txid", "4a1298544a1298544a1298544a1298544a129854"));
        obj.push_back(Pair("dummy_fee", 0.0001)); // dummy fee
        setResult(Value(obj));
    } else {
        setState(OperationStatus::FAILED);
        errorCode = std::rand();
        errorMessage = "Dummy run tests error handling by not liking an odd number number of outputs.";
    }

}

