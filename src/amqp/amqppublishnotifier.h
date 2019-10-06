// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_AMQP_AMQPPUBLISHNOTIFIER_H
#define ZCASH_AMQP_AMQPPUBLISHNOTIFIER_H

#include "amqpabstractnotifier.h"
#include "amqpconfig.h"
#include "amqpsender.h"

#include <memory>
#include <thread>

class CBlockIndex;

class AMQPAbstractPublishNotifier : public AMQPAbstractNotifier
{
private:
    uint64_t sequence_;                         // memory only, per notifier instance: upcounting message sequence number

    std::shared_ptr<std::thread> thread_;       // proton container thread, may be shared between notifiers
    std::shared_ptr<AMQPSender> handler_;      // proton container message handler, may be shared between notifiers

public:
    bool SendMessage(const char *command, const void* data, size_t size);
    bool Initialize();
    void Shutdown();
    void SpawnProtonContainer();
};

class AMQPPublishHashBlockNotifier : public AMQPAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex);
};

class AMQPPublishHashTransactionNotifier : public AMQPAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction);
};

class AMQPPublishRawBlockNotifier : public AMQPAbstractPublishNotifier
{
public:
    bool NotifyBlock(const CBlockIndex *pindex);
};

class AMQPPublishRawTransactionNotifier : public AMQPAbstractPublishNotifier
{
public:
    bool NotifyTransaction(const CTransaction &transaction);
};

#endif // ZCASH_AMQP_AMQPPUBLISHNOTIFIER_H
