// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amqppublishnotifier.h"
#include "chainparams.h"
#include "main.h"
#include "util.h"

#include "amqpsender.h"

#include <memory>
#include <thread>

static std::multimap<std::string, AMQPAbstractPublishNotifier*> mapPublishNotifiers;

static const char *MSG_HASHBLOCK = "hashblock";
static const char *MSG_HASHTX    = "hashtx";
static const char *MSG_RAWBLOCK  = "rawblock";
static const char *MSG_RAWTX     = "rawtx";

// Invoke this method from a new thread to run the proton container event loop.
void AMQPAbstractPublishNotifier::SpawnProtonContainer()
{
    try {
        proton::default_container(*handler_).run();
    }
    catch (const proton::error_condition &e) {
        LogPrint("amqp", "amqp: container error: %s\n", e.what());
    }
    catch (const std::runtime_error &e) {
        LogPrint("amqp", "amqp: runtime error: %s\n", e.what());
    }
    catch (const std::exception &e) {
        LogPrint("amqp", "amqp: exception: %s\n", e.what());
    }
    catch (...) {
        LogPrint("amqp", "amqp: unknown error\n");
    }
    handler_->terminate();
}

bool AMQPAbstractPublishNotifier::Initialize()
{
    std::multimap<std::string, AMQPAbstractPublishNotifier*>::iterator i = mapPublishNotifiers.find(address);

    if (i == mapPublishNotifiers.end()) {
        try {
            handler_ = std::make_shared<AMQPSender>(address);
            thread_ = std::make_shared<std::thread>(&AMQPAbstractPublishNotifier::SpawnProtonContainer, this);
        }
        catch (std::exception &e) {
            LogPrint("amqp", "amqp: initialization error: %s\n", e.what());
            return false;
        }
        mapPublishNotifiers.insert(std::make_pair(address, this));
    } else {
        // copy the shared ptrs to the message handler and the thread where the proton container is running
        handler_ = i->second->handler_;
        thread_ = i->second->thread_;
        mapPublishNotifiers.insert(std::make_pair(address, this));
    }

    return true;
}


void AMQPAbstractPublishNotifier::Shutdown()
{
    LogPrint("amqp", "amqp: Shutdown notifier %s at %s\n", GetType(), GetAddress());

    int count = mapPublishNotifiers.count(address);

    // remove this notifier from the list of publishers using this address
    typedef std::multimap<std::string, AMQPAbstractPublishNotifier*>::iterator iterator;
    std::pair<iterator, iterator> iterpair = mapPublishNotifiers.equal_range(address);

    for (iterator it = iterpair.first; it != iterpair.second; ++it) {
        if (it->second == this) {
            mapPublishNotifiers.erase(it);
            break;
        }
    }

    // terminate the connection if this is the last publisher using this address
    if (count == 1) {
        handler_->terminate();
        if (thread_.get() != nullptr) {
            if (thread_->joinable()) {
                thread_->join();
            }
        }
    }
}


bool AMQPAbstractPublishNotifier::SendMessage(const char *command, const void* data, size_t size)
{
    try { 
        proton::binary content;
        const char *p = (const char *)data;
        content.assign(p, p + size);

        proton::message message(content);
        message.subject(std::string(command));
        proton::message::property_map & props = message.properties();
        props.put("x-opt-sequence-number", sequence_);
        handler_->publish(message);

    } catch (proton::error_condition &e) {
        LogPrint("amqp", "amqp: error : %s\n", e.what());
        return false;
    }
    catch (const std::runtime_error &e) {
        LogPrint("amqp", "amqp: runtime error: %s\n", e.what());
        return false;
    }
    catch (const std::exception &e) {
        LogPrint("amqp", "amqp: exception: %s\n", e.what());
        return false;
    }
    catch (...) {
        LogPrint("amqp", "amqp: unknown error\n");
        return false;
    }

    sequence_++;

    return true;
}

bool AMQPPublishHashBlockNotifier::NotifyBlock(const CBlockIndex *pindex)
{
    uint256 hash = pindex->GetBlockHash();
    LogPrint("amqp", "amqp: Publish hashblock %s\n", hash.GetHex());
    char data[32];
    for (unsigned int i = 0; i < 32; i++)
        data[31 - i] = hash.begin()[i];
    return SendMessage(MSG_HASHBLOCK, data, 32);
}

bool AMQPPublishHashTransactionNotifier::NotifyTransaction(const CTransaction &transaction)
{
    uint256 hash = transaction.GetHash();
    LogPrint("amqp", "amqp: Publish hashtx %s\n", hash.GetHex());
    char data[32];
    for (unsigned int i = 0; i < 32; i++)
        data[31 - i] = hash.begin()[i];
    return SendMessage(MSG_HASHTX, data, 32);
}

bool AMQPPublishRawBlockNotifier::NotifyBlock(const CBlockIndex *pindex)
{
    LogPrint("amqp", "amqp: Publish rawblock %s\n", pindex->GetBlockHash().GetHex());

    const Consensus::Params& consensusParams = Params().GetConsensus();
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    {
        LOCK(cs_main);
        CBlock block;
        if(!ReadBlockFromDisk(block, pindex, consensusParams)) {
            LogPrint("amqp", "amqp: Can't read block from disk");
            return false;
        }

        ss << block;
    }

    return SendMessage(MSG_RAWBLOCK, &(*ss.begin()), ss.size());
}

bool AMQPPublishRawTransactionNotifier::NotifyTransaction(const CTransaction &transaction)
{
    uint256 hash = transaction.GetHash();
    LogPrint("amqp", "amqp: Publish rawtx %s\n", hash.GetHex());
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << transaction;
    return SendMessage(MSG_RAWTX, &(*ss.begin()), ss.size());
}
