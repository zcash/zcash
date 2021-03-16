#include <gtest/gtest.h>

#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include "addrman.h"
#include <string>
#include "netbase.h"

#define GTEST_COUT_NOCOLOR std::cerr << "[          ] [ INFO ] "
namespace testing
{
    namespace internal
    {
    enum GTestColor {
        COLOR_DEFAULT,
        COLOR_RED,
        COLOR_GREEN,
        COLOR_YELLOW
    };

    extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
    }
}
#define PRINTF(...)  do { testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN, "[          ] "); testing::internal::ColoredPrintf(testing::internal::COLOR_YELLOW, __VA_ARGS__); } while(0)

// C++ stream interface
class TestCout : public std::stringstream
{
    public:
        ~TestCout()
        {
            PRINTF("%s",str().c_str());
        }
};

#define GTEST_COUT_COLOR TestCout()

using namespace std;

static CNetAddr ResolveIP(const std::string& ip)
{
        vector<CNetAddr> vIPs;
        CNetAddr addr;
        if (LookupHost(ip.c_str(), vIPs)) {
                addr = vIPs[0];
        } else
        {
            // it was BOOST_CHECK_MESSAGE, but we can't use ASSERT outside a test
            GTEST_COUT_COLOR << strprintf("failed to resolve: %s", ip) << std::endl;
        }
        return addr;
}

namespace TestNetBaseTests {

    TEST(TestAddrmanTests, netbase_getgroup) {

        std::vector<bool> asmap; // use /16
        ASSERT_TRUE(ResolveIP("127.0.0.1").GetGroup(asmap) == std::vector<unsigned char>({0})); // Local -> !Routable()
        ASSERT_TRUE(ResolveIP("257.0.0.1").GetGroup(asmap) == std::vector<unsigned char>({0})); // !Valid -> !Routable()
        ASSERT_TRUE(ResolveIP("10.0.0.1").GetGroup(asmap) == std::vector<unsigned char>({0})); // RFC1918 -> !Routable()
        ASSERT_TRUE(ResolveIP("169.254.1.1").GetGroup(asmap) == std::vector<unsigned char>({0})); // RFC3927 -> !Routable()
        ASSERT_TRUE(ResolveIP("1.2.3.4").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // IPv4

        // std::vector<unsigned char> vch = ResolveIP("4.3.2.1").GetGroup(asmap);
        // GTEST_COUT_COLOR << boost::to_string((int)vch[0]) << boost::to_string((int)vch[1]) << boost::to_string((int)vch[2]) << std::endl;

        ASSERT_TRUE(ResolveIP("::FFFF:0:102:304").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6145
        ASSERT_TRUE(ResolveIP("64:FF9B::102:304").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC6052
        ASSERT_TRUE(ResolveIP("2002:102:304:9999:9999:9999:9999:9999").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC3964
        ASSERT_TRUE(ResolveIP("2001:0:9999:9999:9999:9999:FEFD:FCFB").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV4, 1, 2})); // RFC4380
        ASSERT_TRUE(ResolveIP("FD87:D87E:EB43:edb1:8e4:3588:e546:35ca").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_ONION, 239})); // Tor
        ASSERT_TRUE(ResolveIP("2001:470:abcd:9999:9999:9999:9999:9999").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 4, 112, 175})); //he.net
        ASSERT_TRUE(ResolveIP("2001:2001:9999:9999:9999:9999:9999:9999").GetGroup(asmap) == std::vector<unsigned char>({(unsigned char)NET_IPV6, 32, 1, 32, 1})); //IPv6

    }
}
