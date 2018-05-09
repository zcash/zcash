#ifndef CC_UTILS_H
#define CC_UTILS_H

#include "streams.h"
#include "version.h"


/*
 * Serialisation boilerplate
 */

template <class T>
std::vector<uint8_t> SerializeF(const T f)
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    f(ss);
    return std::vector<unsigned char>(ss.begin(), ss.end());
}

template <class T>
bool DeserializeF(const std::vector<unsigned char> vIn, T f)
{
    CDataStream ss(vIn, SER_NETWORK, PROTOCOL_VERSION);
    try {
         f(ss);
        if (ss.eof()) return true;
    } catch(...) {}
    return false;
}

#define E_MARSHAL(body) SerializeF([&] (CDataStream &ss) {body;})
#define E_UNMARSHAL(params, body) DeserializeF(params, [&] (CDataStream &ss) {body;})

#endif /* CC_UTILS_H */
