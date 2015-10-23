// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_LEVELDBWRAPPER_H
#define BITCOIN_LEVELDBWRAPPER_H

#include "clientversion.h"
#include "serialize.h"
#include "streams.h"
#include "util.h"
#include "version.h"

#include <boost/filesystem/path.hpp>

#include <leveldb/db.h>
#include <leveldb/write_batch.h>

class dbwrapper_error : public std::runtime_error
{
public:
    dbwrapper_error(const std::string& msg) : std::runtime_error(msg) {}
};

void HandleError(const leveldb::Status& status);

/** Batch of changes queued to be written to a CDBWrapper */
class CDBBatch
{
    friend class CDBWrapper;

private:
    leveldb::WriteBatch batch;

public:
    template <typename K, typename V>
    void Write(const K& key, const V& value)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(ssKey.GetSerializeSize(key));
        ssKey << key;
        leveldb::Slice slKey(&ssKey[0], ssKey.size());

        CDataStream ssValue(SER_DISK, CLIENT_VERSION);
        ssValue.reserve(ssValue.GetSerializeSize(value));
        ssValue << value;
        leveldb::Slice slValue(&ssValue[0], ssValue.size());

        batch.Put(slKey, slValue);
    }

    template <typename K>
    void Erase(const K& key)
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(ssKey.GetSerializeSize(key));
        ssKey << key;
        leveldb::Slice slKey(&ssKey[0], ssKey.size());

        batch.Delete(slKey);
    }
};

class CDBIterator
{
private:
    leveldb::Iterator *piter;

public:

    /**
     * @param[in] piterIn          The original leveldb iterator.
     */
    CDBIterator(leveldb::Iterator *piterIn) :
        piter(piterIn) { };
    ~CDBIterator();

    bool Valid();

    void SeekToFirst();

    template<typename K> void Seek(const K& key) {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(ssKey.GetSerializeSize(key));
        ssKey << key;
        leveldb::Slice slKey(&ssKey[0], ssKey.size());
        piter->Seek(slKey);
    }

    void Next();

    template<typename K> bool GetKey(K& key) {
        leveldb::Slice slKey = piter->key();
        try {
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            ssKey >> key;
        } catch(std::exception &e) {
            return false;
        }
        return true;
    }

    unsigned int GetKeySize() {
        return piter->key().size();
    }

    template<typename V> bool GetValue(V& value) {
        leveldb::Slice slValue = piter->value();
        try {
            CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch(std::exception &e) {
            return false;
        }
        return true;
    }

    unsigned int GetValueSize() {
        return piter->value().size();
    }

};

class CDBWrapper
{
private:
    //! custom environment this database is using (may be NULL in case of default environment)
    leveldb::Env* penv;

    //! database options used
    leveldb::Options options;

    //! options used when reading from the database
    leveldb::ReadOptions readoptions;

    //! options used when iterating over values of the database
    leveldb::ReadOptions iteroptions;

    //! options used when writing to the database
    leveldb::WriteOptions writeoptions;

    //! options used when sync writing to the database
    leveldb::WriteOptions syncoptions;

    //! the database itself
    leveldb::DB* pdb;

public:
    /**
     * @param[in] path        Location in the filesystem where leveldb data will be stored.
     * @param[in] nCacheSize  Configures various leveldb cache settings.
     * @param[in] fMemory     If true, use leveldb's memory environment.
     * @param[in] fWipe       If true, remove all existing data.
     */
    CDBWrapper(const boost::filesystem::path& path, size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    ~CDBWrapper();

    template <typename K, typename V>
    bool Read(const K& key, V& value) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(ssKey.GetSerializeSize(key));
        ssKey << key;
        leveldb::Slice slKey(&ssKey[0], ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            HandleError(status);
        }
        try {
            CDataStream ssValue(strValue.data(), strValue.data() + strValue.size(), SER_DISK, CLIENT_VERSION);
            ssValue >> value;
        } catch (const std::exception&) {
            return false;
        }
        return true;
    }

    template <typename K, typename V>
    bool Write(const K& key, const V& value, bool fSync = false)
    {
        CDBBatch batch;
        batch.Write(key, value);
        return WriteBatch(batch, fSync);
    }

    template <typename K>
    bool Exists(const K& key) const
    {
        CDataStream ssKey(SER_DISK, CLIENT_VERSION);
        ssKey.reserve(ssKey.GetSerializeSize(key));
        ssKey << key;
        leveldb::Slice slKey(&ssKey[0], ssKey.size());

        std::string strValue;
        leveldb::Status status = pdb->Get(readoptions, slKey, &strValue);
        if (!status.ok()) {
            if (status.IsNotFound())
                return false;
            LogPrintf("LevelDB read failure: %s\n", status.ToString());
            HandleError(status);
        }
        return true;
    }

    template <typename K>
    bool Erase(const K& key, bool fSync = false)
    {
        CDBBatch batch;
        batch.Erase(key);
        return WriteBatch(batch, fSync);
    }

    bool WriteBatch(CDBBatch& batch, bool fSync = false);

    // not available for LevelDB; provide for compatibility with BDB
    bool Flush()
    {
        return true;
    }

    bool Sync()
    {
        CDBBatch batch;
        return WriteBatch(batch, true);
    }

    CDBIterator *NewIterator()
    {
        return new CDBIterator(pdb->NewIterator(iteroptions));
    }

    /**
     * Return true if the database managed by this class contains no entries.
     */
    bool IsEmpty();

};

#endif // BITCOIN_LEVELDBWRAPPER_H

