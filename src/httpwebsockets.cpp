#include "httpwebsockets.h"

#include "util.h"

#include <boost/chrono.hpp>
#include <boost/thread.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

// Threads
boost::thread threadWebsocketListener;
boost::thread threadWebsocketHandler;

// Websockets
std::vector<websocket::stream<tcp::socket>*> vWebsockets;

// Buffer
boost::beast::multi_buffer bufferWebsockets;
boost::mutex mutexBufferWebsockets;
bool fBufferWebsocketsEmpty = true;

// Shutdown
bool fShutdownWebsockets = false;
boost::mutex mutexShutdownWebsockets;

void WriteBufferWebsockets(std::string message) {
    mutexBufferWebsockets.lock();

    if (fBufferWebsocketsEmpty == false) {
        message = "," + message;
    }

    size_t n = boost::asio::buffer_copy(
            bufferWebsockets.prepare(message.size()),
            boost::asio::buffer(message));
    bufferWebsockets.commit(n);

    fBufferWebsocketsEmpty = false;

    mutexBufferWebsockets.unlock();
}

void WriteBufferToWebsocket(websocket::stream<tcp::socket>* ws) {
    mutexBufferWebsockets.lock();

    ws->text(true);
    ws->write(bufferWebsockets.data());

    mutexBufferWebsockets.unlock();
}

void OpenBufferWebsockets() {
    mutexBufferWebsockets.lock();

    std::string message = "[";
    size_t n = boost::asio::buffer_copy(
            bufferWebsockets.prepare(message.size()),
            boost::asio::buffer(message));
    bufferWebsockets.commit(n);

    mutexBufferWebsockets.unlock();
}

void CloseBufferWebsockets() {
    mutexBufferWebsockets.lock();

    std::string message = "]";
    size_t n = boost::asio::buffer_copy(
            bufferWebsockets.prepare(message.size()),
            boost::asio::buffer(message));
    bufferWebsockets.commit(n);

    mutexBufferWebsockets.unlock();
}

void ClearBufferWebsockets() {
    mutexBufferWebsockets.lock();

    bufferWebsockets.clear();
    fBufferWebsocketsEmpty = true;

    mutexBufferWebsockets.unlock();
}

bool fGetBufferWebsocketsEmpty() {
    bool ret;

    mutexBufferWebsockets.lock();

    ret = fBufferWebsocketsEmpty;

    mutexBufferWebsockets.unlock();

    return ret;
}

bool fGetShutdownWebsockets() {
    bool ret;

    mutexShutdownWebsockets.lock();

    ret = fShutdownWebsockets;

    mutexShutdownWebsockets.unlock();

    return ret;
}

void fSetShutdownWebsockets(bool fShutdown) {
    mutexShutdownWebsockets.lock();
    
    fShutdownWebsockets = fShutdown;

    mutexShutdownWebsockets.unlock();
}

/*
 * Thread to listen for incoming websocket connections
 * */
void ThreadWebsocketListener() {
    RenameThread("zcash-websocket-listener");

    auto const address = boost::asio::ip::make_address("0.0.0.0");
    auto const port = static_cast<unsigned short>(std::atoi("8334"));

    boost::asio::io_context context{1};

    tcp::acceptor acceptor{context, {address, port}};

    for (;;) {
        // Shutdown request
        if (fGetShutdownWebsockets()) {
            return;
        }

        try {
            // Wait for a connection
            tcp::socket socket{context};
            acceptor.accept(socket);
            
            // Upgrade connection
            websocket::stream<tcp::socket>* ws =
                new websocket::stream<tcp::socket>(std::move(socket));
            ws->accept();

            // Add connection to queue
            vWebsockets.push_back(ws);
        } catch (const std::exception& e) {
            LogPrint("websockets", "ERROR: cannot accept websocket connection\n");
        }
    }
}

/*
 * Thread to write buffered data to all websockets
 * */
void ThreadWebsocketHandler() {
    RenameThread("zcash-websocket-handler");

    ClearBufferWebsockets();
    OpenBufferWebsockets();

    for (;;) {
        // Shutdown request
        if (fGetShutdownWebsockets()) {
            return;
        }

        // Wait for buffer to write
        boost::this_thread::sleep_for(boost::chrono::milliseconds(500));

        if (fGetBufferWebsocketsEmpty()) {
            continue;
        }

        // Close JSON set
        CloseBufferWebsockets();

        int n = vWebsockets.size();
        for (int i_w = 0; i_w < n; i_w++) {
            try {

                WriteBufferToWebsocket(vWebsockets[i_w]);

            } catch (boost::system::system_error const& se) {
                // session has closed
                LogPrint("websockets", "Session closed\n");
                
                // remove websocket
                delete vWebsockets[i_w];
                vWebsockets.erase(vWebsockets.begin() + i_w);

            } catch (std::exception const& e) {
                LogPrint("websockets", "ERROR: websocket session closed unexpectedly\n");

                // remove websocket
                delete vWebsockets[i_w];
                vWebsockets.erase(vWebsockets.begin() + i_w);

            }
        }
        
        ClearBufferWebsockets();
        OpenBufferWebsockets();
    }
}

bool StartWebsockets(boost::thread_group& threadGroup) {
    LogPrint("websockets", "Starting websocket server\n");

    threadWebsocketListener = boost::thread(&ThreadWebsocketListener);
    threadWebsocketHandler = boost::thread(&ThreadWebsocketHandler);

    return true;
}

void InterruptWebsockets() {
    LogPrint("websockets", "Interrupting websocket server\n");
}

void StopWebsockets() {
    LogPrint("websockets", "Stopping websocket server\n");

    fSetShutdownWebsockets(true);
}


