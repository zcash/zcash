#ifndef BITCOIN_HTTPWEBSOCKETS_H
#define BITCOIN_HTTPWEBSOCKETS_H

#include <boost/thread.hpp>

/** Start HTTP Websocket subsystem.
 */
bool StartWebsockets(boost::thread_group& threadGroup);

/** Interrupt RPC Websocket subsystem.
 */
void InterruptWebsockets();

/** Stop HTTP Websocket subsystem.
 */
void StopWebsockets();

/** Write to the websocket buffer
 */
void WriteBufferWebsockets(std::string message);
#endif
