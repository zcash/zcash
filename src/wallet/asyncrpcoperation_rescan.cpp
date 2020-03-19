// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "asyncrpcoperation_rescan.h"

#include "asyncrpcoperation_common.h"
#include "asyncrpcqueue.h"

AsyncRPCOperation_rescan::AsyncRPCOperation_rescan(
        int nRescanHeight,
        std::string caller, 
        UniValue result) :
        height_(nRescanHeight), caller_(caller), result_(result)
{
    LogPrint("zrpc", "%s: Async rescan initialized\n", getId());
}

AsyncRPCOperation_rescan::~AsyncRPCOperation_rescan() {
}

void AsyncRPCOperation_rescan::main() {
    if (isCancelled()) {
        return;
    }

    set_state(OperationStatus::EXECUTING);
    start_execution_clock();

    bool success = false;

    try {
        success = main_impl();
    } catch (const UniValue& objError) {
        int code = find_value(objError, "code").get_int();
        std::string message = find_value(objError, "message").get_str();
        set_error_code(code);
        set_error_message(message);
    } catch (const runtime_error& e) {
        set_error_code(-1);
        set_error_message("runtime error: " + string(e.what()));
    } catch (const logic_error& e) {
        set_error_code(-1);
        set_error_message("logic error: " + string(e.what()));
    } catch (const exception& e) {
        set_error_code(-1);
        set_error_message("general exception: " + string(e.what()));
    } catch (...) {
        set_error_code(-2);
        set_error_message("unknown error");
    }

    stop_execution_clock();

    if (success) {
        set_state(OperationStatus::SUCCESS);
        set_result(result_);
    } else {
        set_state(OperationStatus::FAILED);
    }

    std::string s = strprintf("%s: rescan finished (status=%s", getId(), getStateAsString());
    if (success) {
        s += strprintf(", type=%s, address=%s)\n", find_value(result_, "type").get_str(), find_value(result_, "address").get_str());
    } else {
        s += strprintf(", error=%s)\n", getErrorMessage());
    }
    LogPrintf("%s",s);
}

bool AsyncRPCOperation_rescan::main_impl() {
    pwalletMain->ScanForWalletTransactions(chainActive[height_], true);
    return true;
}

UniValue AsyncRPCOperation_rescan::getStatus() const {
    UniValue v = AsyncRPCOperation::getStatus();
    UniValue obj = v.get_obj();
    obj.push_back(Pair("method", caller_));
    return obj;
}
