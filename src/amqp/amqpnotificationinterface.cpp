// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amqpnotificationinterface.h"
#include "amqppublishnotifier.h"

#include "version.h"
#include "main.h"
#include "streams.h"
#include "util.h"

// AMQP 1.0 Support
//
// The boost::signals2 signals and slot system is thread safe, so CValidationInterface listeners
// can be invoked from any thread.
//
// Currently signals are fired from main.cpp so the callbacks should be invoked on the same thread.
// It should be safe to share objects responsible for sending, as they should not be run concurrently
// across different threads.
//
// Developers should be mindful of where notifications are fired to avoid potential race conditions.
// For example, different signals targeting the same address could be fired from different threads
// in different parts of the system around the same time.
//
// Like the ZMQ notification interface, if a notifier fails to send a message, the notifier is shut down.
//

AMQPNotificationInterface::AMQPNotificationInterface()
{
}

AMQPNotificationInterface::~AMQPNotificationInterface()
{
    Shutdown();

    for (std::list<AMQPAbstractNotifier*>::iterator i = notifiers.begin(); i != notifiers.end(); ++i) {
        delete *i;
    }
}

AMQPNotificationInterface* AMQPNotificationInterface::CreateWithArguments(const std::map<std::string, std::string> &args)
{
    AMQPNotificationInterface* notificationInterface = nullptr;
    std::map<std::string, AMQPNotifierFactory> factories;
    std::list<AMQPAbstractNotifier*> notifiers;

    factories["pubhashblock"] = AMQPAbstractNotifier::Create<AMQPPublishHashBlockNotifier>;
    factories["pubhashtx"] = AMQPAbstractNotifier::Create<AMQPPublishHashTransactionNotifier>;
    factories["pubrawblock"] = AMQPAbstractNotifier::Create<AMQPPublishRawBlockNotifier>;
    factories["pubrawtx"] = AMQPAbstractNotifier::Create<AMQPPublishRawTransactionNotifier>;

    for (std::map<std::string, AMQPNotifierFactory>::const_iterator i=factories.begin(); i!=factories.end(); ++i) {
        std::map<std::string, std::string>::const_iterator j = args.find("-amqp" + i->first);
        if (j!=args.end()) {
            AMQPNotifierFactory factory = i->second;
            std::string address = j->second;
            AMQPAbstractNotifier *notifier = factory();
            notifier->SetType(i->first);
            notifier->SetAddress(address);
            notifiers.push_back(notifier);
        }
    }

    if (!notifiers.empty()) {
        notificationInterface = new AMQPNotificationInterface();
        notificationInterface->notifiers = notifiers;

        if (!notificationInterface->Initialize()) {
            delete notificationInterface;
            notificationInterface = nullptr;
        }
    }

    return notificationInterface;
}

// Called at startup to conditionally set up
bool AMQPNotificationInterface::Initialize()
{
    LogPrint("amqp", "amqp: Initialize notification interface\n");

    std::list<AMQPAbstractNotifier*>::iterator i = notifiers.begin();
    for (; i != notifiers.end(); ++i) {
        AMQPAbstractNotifier *notifier = *i;
        if (notifier->Initialize()) {
            LogPrint("amqp", "amqp: Notifier %s ready (address = %s)\n", notifier->GetType(), notifier->GetAddress());
        } else {
            LogPrint("amqp", "amqp: Notifier %s failed (address = %s)\n", notifier->GetType(), notifier->GetAddress());
            break;
        }
    }

    if (i != notifiers.end()) {
        return false;
    }

    return true;
}

// Called during shutdown sequence
void AMQPNotificationInterface::Shutdown()
{
    LogPrint("amqp", "amqp: Shutdown notification interface\n");

    for (std::list<AMQPAbstractNotifier*>::iterator i = notifiers.begin(); i != notifiers.end(); ++i) {
        AMQPAbstractNotifier *notifier = *i;
        notifier->Shutdown();
    }
}

void AMQPNotificationInterface::UpdatedBlockTip(const CBlockIndex *pindex)
{
    for (std::list<AMQPAbstractNotifier*>::iterator i = notifiers.begin(); i != notifiers.end(); ) {
        AMQPAbstractNotifier *notifier = *i;
        if (notifier->NotifyBlock(pindex)) {
            i++;
        } else {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}

void AMQPNotificationInterface::SyncTransaction(const CTransaction &tx, const CBlock *pblock)
{
    for (std::list<AMQPAbstractNotifier*>::iterator i = notifiers.begin(); i != notifiers.end(); ) {
        AMQPAbstractNotifier *notifier = *i;
        if (notifier->NotifyTransaction(tx)) {
            i++;
        } else {
            notifier->Shutdown();
            i = notifiers.erase(i);
        }
    }
}
