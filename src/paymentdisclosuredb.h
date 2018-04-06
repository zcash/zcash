// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_PAYMENTDISCLOSUREDB_H
#define ZCASH_PAYMENTDISCLOSUREDB_H

#include "paymentdisclosure.h"

#include <cstdint>
#include <string>
#include <mutex>
#include <future>
#include <memory>

#include <boost/optional.hpp>

#include <leveldb/db.h>


class PaymentDisclosureDB
{
protected:
    leveldb::DB* db = nullptr;
    leveldb::Options options;
    leveldb::ReadOptions readOptions;
    leveldb::WriteOptions writeOptions;
    mutable std::mutex lock_;

public:
    static std::shared_ptr<PaymentDisclosureDB> sharedInstance();

    PaymentDisclosureDB();
    PaymentDisclosureDB(const boost::filesystem::path& dbPath);
    ~PaymentDisclosureDB();

    bool Put(const PaymentDisclosureKey& key, const PaymentDisclosureInfo& info);
    bool Get(const PaymentDisclosureKey& key, PaymentDisclosureInfo& info);
};


#endif // ZCASH_PAYMENTDISCLOSUREDB_H
