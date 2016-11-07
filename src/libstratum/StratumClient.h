// Copyright (c) 2016 Genoil <jw@meneer.net>
// Copyright (c) 2016 Jack Grigg <jack@z.cash>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientversion.h"
#include "libstratum/ZcashStratum.h"

#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <thread>

#include "json/json_spirit_value.h"

using namespace std;
using namespace boost::asio;
using boost::asio::ip::tcp;
using namespace json_spirit;


typedef struct {
        string host;
        string port;
        string user;
        string pass;
} cred_t;

template <typename Miner, typename Job, typename Solution>
class StratumClient
{
public:
    StratumClient(Miner * m,
                  string const & host, string const & port,
                  string const & user, string const & pass,
                  int const & retries, int const & worktimeout);
    ~StratumClient() { }

    void setFailover(string const & host, string const & port);
    void setFailover(string const & host, string const & port,
                     string const & user, string const & pass);

    bool isRunning() { return m_running; }
    bool isConnected() { return m_connected && m_authorized; }
    bool current() { return p_current; }
    bool submit(const Solution* solution);
    void reconnect();
    void disconnect();

private:
    void startWorking();
    void workLoop();
    void connect();

    void work_timeout_handler(const boost::system::error_code& ec);

    void processReponse(const Object& responseObject);

    cred_t * p_active;
    cred_t m_primary;
    cred_t m_failover;

    bool m_authorized;
    bool m_connected;
    bool m_running = true;

    int    m_retries = 0;
    int    m_maxRetries;
    int m_worktimeout = 60;

    string m_response;

    Miner * p_miner;
    boost::mutex x_current;
    Job * p_current;
    Job * p_previous;

    bool m_stale = false;

    std::unique_ptr<std::thread> m_work;

    boost::asio::io_service m_io_service;
    tcp::socket m_socket;

    boost::asio::streambuf m_requestBuffer;
    boost::asio::streambuf m_responseBuffer;

    boost::asio::deadline_timer * p_worktimer;

    string m_nextJobTarget;
};

typedef StratumClient<ZcashMiner, ZcashJob, EquihashSolution> ZcashStratumClient;
