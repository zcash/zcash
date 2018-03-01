// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ZCASH_AMQP_AMQPSENDER_H
#define ZCASH_AMQP_AMQPSENDER_H

#include "amqpconfig.h"

#include <deque>
#include <memory>
#include <future>
#include <iostream>

class AMQPSender : public proton::messaging_handler {
  private:
    std::deque<proton::message> messages_; 
    proton::url url_;
    proton::connection conn_;
    proton::sender sender_;
    std::mutex lock_;
    std::atomic<bool> terminated_ = {false};

  public:

    AMQPSender(const std::string& url) : url_(url) {}

    // Callback to initialize the container when run() is invoked
    void on_container_start(proton::container& c) override {
        proton::duration t(10000);   // milliseconds
        proton::connection_options opts = proton::connection_options().idle_timeout(t);
        conn_ = c.connect(url_, opts);
        sender_ = conn_.open_sender(url_.path());
    }

    // Remote end signals when the local end can send (i.e. has credit) 
    void on_sendable(proton::sender &s) override {
        dispatch();
    }

    // Publish message by adding to queue and trying to dispatch it
    void publish(const proton::message &m) {
        add_message(m);
        dispatch();
    }

    // Add message to queue
    void add_message(const proton::message &m) {
        std::lock_guard<std::mutex> guard(lock_);
        messages_.push_back(m);
    }

    // Send messages in queue
    void dispatch() {
        std::lock_guard<std::mutex> guard(lock_);

        if (isTerminated()) {
            throw std::runtime_error("amqp connection was terminated");
        }

        if (!conn_.active()) {
            throw std::runtime_error("amqp connection is not active");
        }

        while (messages_.size() > 0) {
            if (sender_.credit()) {
                const proton::message& m = messages_.front();
                sender_.send(m);
                messages_.pop_front();
            } else {
                break;
            }
        }
    }

    // Close connection to remote end.  Container event-loop, by default, will auto-stop.
    void terminate() {
        std::lock_guard<std::mutex> guard(lock_);
        conn_.close();
        terminated_.store(true);
    }

    bool isTerminated() const {
        return terminated_.load();
    }

    void on_transport_error(proton::transport &t) override {
        t.connection().close();
        throw t.error();
    }

    void on_connection_error(proton::connection &c) override {
        c.close();
        throw c.error();
    }

    void on_session_error(proton::session &s) override {
        s.connection().close();
        throw s.error();
    }

    void on_receiver_error(proton::receiver &r) override {
        r.connection().close();
        throw r.error();
    }

    void on_sender_error(proton::sender &s) override {
        s.connection().close();
        throw s.error();
    }

};


#endif //ZCASH_AMQP_AMQPSENDER_H
