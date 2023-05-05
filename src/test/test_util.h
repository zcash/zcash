#ifndef BITCOIN_TEST_TEST_UTIL_H
#define BITCOIN_TEST_TEST_UTIL_H

#include <string>

#include <univalue.h>

UniValue read_json(const std::string& jsondata);
unsigned int ParseScriptFlags(std::string strFlags);
std::string FormatScriptFlags(unsigned int flags);
UniValue createArgs(int nRequired, const char* address1 = nullptr, const char* address2 = nullptr);
UniValue CallRPC(std::string args);


#endif
