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

//-- Websocket Server --//
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

bool fRequestedShutdown = false;

// Websocket write buffer
boost::mutex websocketMutex;
boost::beast::multi_buffer websocketBuffer;

boost::asio::io_context ioContext{1};
std::vector<websocket::stream<tcp::socket>*> websockets;

void ThreadWebsocketHandler() {
    RenameThread("zcash-websocket-handler");

    ClearWebsockets();

    for (;;) {
        if (fRequestedShutdown) {
            break;
        }

        boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));

        if (websocketBuffer.size() < 2) {
            continue;
        }

        WriteWebsockets("]");

        int n = websockets.size();
        for (int i = 0; i < n; i ++) {
            try {
                websocket::stream<tcp::socket>* ws = websockets[i];

                ws->text(true);
                ws->write(websocketBuffer.data());
            } catch(boost::system::system_error const& se) {
                // This indicates that the session was closed
                if(se.code() != websocket::error::closed)
                    std::cerr << "Error: " << se.code().message() << std::endl;
            } catch(std::exception const& e) {
                std::cerr << "Error: " << e.what() << std::endl;
            }
        }

        ClearWebsockets();
    }

    // clean up
    int n = websockets.size();
    for (int i = 0; i < n; i ++) {
        delete websockets[i];
    }
    websockets.clear();
    websocketBuffer.clear();
}

void ThreadWebsocketListener() {
    RenameThread("zcash-websocket-listener");

    try {
        auto const address = boost::asio::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(std::atoi("8334"));

         // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioContext, {address, port}};

        for (;;) {
            if (fRequestedShutdown) {
                break;
            }
            boost::this_thread::sleep_for(boost::chrono::milliseconds(1000));
            // This will receive the new connection
            tcp::socket socket{ioContext};

            // Block until we get a connection
            acceptor.accept(socket);

            // Construct the stream by moving in the socket
            websocket::stream<tcp::socket>* ws = new websocket::stream<tcp::socket>(std::move(socket));

            // Accept the websocket handshake
            ws->accept();

            websockets.push_back(ws);
        }
    }
    catch(const std::exception& e) {
        // TODO better error catching
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

bool StartWebsockets(boost::thread_group& threadGroup) {
    LogPrint("websockets", "Starting websocket server\n");

    threadGroup.create_thread(&ThreadWebsocketListener);
    threadGroup.create_thread(&ThreadWebsocketHandler);

    return true;
}

void InterruptWebsockets() {
    LogPrint("websockets", "Interrupting websocket server\n");
}

void StopWebsockets() {
    LogPrint("websockets", "Stopping websocket server\n");
    fRequestedShutdown = true;
}

void WriteWebsockets(std::string message) {
    // -- CRITICAL SECTION --//
    websocketMutex.lock();

    if (message != "[" && message != "]" && websocketBuffer.size() > 1) {
        message = "," + message;
    }

    size_t n = boost::asio::buffer_copy(
            websocketBuffer.prepare(message.size()),
            boost::asio::buffer(message));
    websocketBuffer.commit(n);

    websocketMutex.unlock();
    //-----------------------//
}

void ClearWebsockets() {
    websocketBuffer.clear();

    WriteWebsockets("[");
}

