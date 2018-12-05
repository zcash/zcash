// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "paymentdisclosuredb.h"

#include "util.h"
#include "dbwrapper.h"

#include <boost/filesystem.hpp>

using namespace std;

static boost::filesystem::path emptyPath;

/**
 * Static method to return the shared/default payment disclosure database.
 */
shared_ptr<PaymentDisclosureDB> PaymentDisclosureDB::sharedInstance() {
    // Thread-safe in C++11 and gcc 4.3
    static shared_ptr<PaymentDisclosureDB> ptr = std::make_shared<PaymentDisclosureDB>();
    return ptr;
}

// C++11 delegated constructor
PaymentDisclosureDB::PaymentDisclosureDB() : PaymentDisclosureDB(emptyPath) {
}

PaymentDisclosureDB::PaymentDisclosureDB(const boost::filesystem::path& dbPath) {
    boost::filesystem::path path(dbPath);
    if (path.empty()) {
        path = GetDataDir() / "paymentdisclosure";
        LogPrintf("PaymentDisclosure: using default path for database: %s\n", path.string());
    } else {
        LogPrintf("PaymentDisclosure: using custom path for database: %s\n", path.string());
    }

    TryCreateDirectory(path);
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, path.string(), &db);
    dbwrapper_private::HandleError(status); // throws exception
    LogPrintf("PaymentDisclosure: Opened LevelDB successfully\n");
}

PaymentDisclosureDB::~PaymentDisclosureDB() {
    if (db != nullptr) {
        delete db;
    }
}

bool PaymentDisclosureDB::Put(const PaymentDisclosureKey& key, const PaymentDisclosureInfo& info)
{
    if (db == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);

    CDataStream ssValue(SER_DISK, CLIENT_VERSION);
    ssValue.reserve(GetSerializeSize(ssValue, info));
    ssValue << info;
    leveldb::Slice slice(&ssValue[0], ssValue.size());

    leveldb::Status status = db->Put(writeOptions, key.ToString(), slice);
    dbwrapper_private::HandleError(status);
    return true;
}

bool PaymentDisclosureDB::Get(const PaymentDisclosureKey& key, PaymentDisclosureInfo& info)
{
    if (db == nullptr) {
        return false;
    }

    std::lock_guard<std::mutex> guard(lock_);

    std::string strValue;
    leveldb::Status status = db->Get(readOptions, key.ToString(), &strValue);
    if (!status.ok()) {
        if (status.IsNotFound())
            return false;
        LogPrintf("PaymentDisclosure: LevelDB read failure: %s\n", status.ToString());
        dbwrapper_private::HandleError(status);
    }

    try {
        CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
        ssValue >> info;
    } catch (const std::exception&) {
        return false;
    }
    return true;
}
