#include <gtest/gtest.h>

#include "addrman.h"
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <string>

#include "hash.h"
#include "random.h"
#include "util/asmap.h"

#include "netbase.h"
#include "chainparams.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

#define NODE_NONE 0

// https://stackoverflow.com/questions/16491675/how-to-send-custom-message-in-google-c-testing-framework/29155677
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

/* xxd -i est-komodo/data/asmap.raw | sed 's/unsigned char/static unsigned const char/g' */
static unsigned const char asmap_raw[] = {
    0xfb, 0x03, 0xec, 0x0f, 0xb0, 0x3f, 0xc0, 0xfe, 0x00, 0xfb, 0x03, 0xec,
    0x0f, 0xb0, 0x3f, 0xc0, 0xfe, 0x00, 0xfb, 0x03, 0xec, 0x0f, 0xb0, 0xff,
    0xff, 0xfe, 0xff, 0xed, 0xb0, 0xff, 0xd4, 0x86, 0xe6, 0x28, 0x29, 0x00,
    0x00, 0x40, 0x00, 0x00, 0x40, 0x00, 0x40, 0x99, 0x01, 0x00, 0x80, 0x01,
    0x80, 0x04, 0x00, 0x00, 0x05, 0x00, 0x06, 0x00, 0x1c, 0xf0, 0x39
};
unsigned int asmap_raw_len = 59;

class CAddrManTest : public CAddrMan
{
    private:
        uint64_t state;
        bool deterministic;
    public:

        explicit CAddrManTest(bool makeDeterministic = true,
            std::vector<bool> asmap = std::vector<bool>())
        {
            if (makeDeterministic) {
                //  Set addrman addr placement to be deterministic.
                MakeDeterministic();
            }
            deterministic = makeDeterministic;
            m_asmap = asmap;
            state = 1;
        }

        // CAddrManTest()
        // {
        //     state = 1;
        // }

        //! Ensure that bucket placement is always the same for testing purposes.
        void MakeDeterministic()
        {
            nKey.SetNull();
            seed_insecure_rand(true);
        }

        int RandomInt(int nMax)
        {
            state = (CHashWriter(SER_GETHASH, 0) << state).GetHash().GetCheapHash();
            return (unsigned int)(state % nMax);
        }

        CAddrInfo* Find(const CNetAddr& addr, int* pnId = NULL)
        {
            return CAddrMan::Find(addr, pnId);
        }

        CAddrInfo* Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId = NULL)
        {
            return CAddrMan::Create(addr, addrSource, pnId);
        }

        void Delete(int nId)
        {
            CAddrMan::Delete(nId);
        }

        // Used to test deserialization
        std::pair<int, int> GetBucketAndEntry(const CAddress& addr)
        {
            // LOCK(cs);
            int nId = mapAddr[addr];
            for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; ++bucket) {
                for (int entry = 0; entry < ADDRMAN_BUCKET_SIZE; ++entry) {
                    if (nId == vvNew[bucket][entry]) {
                        return std::pair<int, int>(bucket, entry);
                    }
                }
            }
            return std::pair<int, int>(-1, -1);
        }

        void Clear()
        {
            CAddrMan::Clear();
            if (deterministic) {
                nKey.SetNull();
                seed_insecure_rand(true);
            }
        }
};

static CNetAddr ResolveIP(const std::string& ip)
{
        vector<CNetAddr> vIPs;
        CNetAddr addr;
        if (LookupHost(ip.c_str(), vIPs)) {
                addr = vIPs[0];
        } else
        {
            // it was BOOST_CHECK_MESSAGE, but we can't use ASSERT or EXPECT outside a test
            GTEST_COUT_COLOR << strprintf("failed to resolve: %s", ip) << std::endl;
        }
        return addr;
}

static CService ResolveService(const std::string& ip, const int port = 0)
{
    CService serv;
    if (!Lookup(ip.c_str(), serv, port, false))
        GTEST_COUT_COLOR << strprintf("failed to resolve: %s:%i", ip, port) << std::endl;
    return serv;
}

static std::vector<bool> FromBytes(const unsigned char* source, int vector_size) {
    std::vector<bool> result(vector_size);
    for (int byte_i = 0; byte_i < vector_size / 8; ++byte_i) {
        unsigned char cur_byte = source[byte_i];
        for (int bit_i = 0; bit_i < 8; ++bit_i) {
            result[byte_i * 8 + bit_i] = (cur_byte >> bit_i) & 1;
        }
    }
    return result;
}

namespace TestAddrmanTests {

    TEST(TestAddrmanTests, display_constants) {

        // Not actually the test, just used to display constants
        GTEST_COUT_COLOR << "ADDRMAN_NEW_BUCKET_COUNT = " << ADDRMAN_NEW_BUCKET_COUNT << std::endl;
        GTEST_COUT_COLOR << "ADDRMAN_TRIED_BUCKET_COUNT = " << ADDRMAN_TRIED_BUCKET_COUNT << std::endl;
        GTEST_COUT_COLOR << "ADDRMAN_BUCKET_SIZE = " << ADDRMAN_BUCKET_SIZE << std::endl;

    }

    TEST(TestAddrmanTests, addrman_simple) {

        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        CNetAddr source = CNetAddr("252.2.2.2");

        // Test 1: Does Addrman respond correctly when empty.
        ASSERT_TRUE(addrman.size() == 0);
        CAddrInfo addr_null = addrman.Select();
        ASSERT_TRUE(addr_null.ToString() == "[::]:0");

        // Test 2: Does Addrman::Add work as expected.
        CService addr1 = CService("250.1.1.1", 8333);
        addrman.Add(CAddress(addr1, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 1);
        CAddrInfo addr_ret1 = addrman.Select();
        ASSERT_TRUE(addr_ret1.ToString() == "250.1.1.1:8333");

        // Test 3: Does IP address deduplication work correctly.
        //  Expected dup IP should not be added.
        CService addr1_dup = CService("250.1.1.1", 8333);
        addrman.Add(CAddress(addr1_dup, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 1);

        // Test 5: New table has one addr and we add a diff addr we should
        //  have two addrs.
        CService addr2 = CService("250.1.1.2", 8333);
        addrman.Add(CAddress(addr2, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 2);

        // Test 6: AddrMan::Clear() should empty the new table.
        addrman.Clear();
        ASSERT_TRUE(addrman.size() == 0);
        CAddrInfo addr_null2 = addrman.Select();
        ASSERT_TRUE(addr_null2.ToString() == "[::]:0");

    }

    TEST(TestAddrmanTests, addrman_ports) {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        CNetAddr source = CNetAddr("252.2.2.2");

        ASSERT_TRUE(addrman.size() == 0);

        // Test 7; Addr with same IP but diff port does not replace existing addr.
        CService addr1 = CService("250.1.1.1", 8333);
        addrman.Add(CAddress(addr1, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 1);

        CService addr1_port = CService("250.1.1.1", 8334);
        addrman.Add(CAddress(addr1_port, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 1);
        CAddrInfo addr_ret2 = addrman.Select();
        ASSERT_TRUE(addr_ret2.ToString() == "250.1.1.1:8333");

        // Test 8: Add same IP but diff port to tried table, it doesn't get added.
        //  Perhaps this is not ideal behavior but it is the current behavior.
        addrman.Good(CAddress(addr1_port, NODE_NONE));
        ASSERT_TRUE(addrman.size() == 1);
        bool newOnly = true;
        CAddrInfo addr_ret3 = addrman.Select(newOnly);
        ASSERT_TRUE(addr_ret3.ToString() == "250.1.1.1:8333");

    }

    TEST(TestAddrmanTests, addrman_select) {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        CNetAddr source = CNetAddr("252.2.2.2");

        // Test 9: Select from new with 1 addr in new.
        CService addr1 = CService("250.1.1.1", 8333);
        addrman.Add(CAddress(addr1, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 1);

        bool newOnly = true;
        CAddrInfo addr_ret1 = addrman.Select(newOnly);
        ASSERT_TRUE(addr_ret1.ToString() == "250.1.1.1:8333");

        // Test 10: move addr to tried, select from new expected nothing returned.
        addrman.Good(CAddress(addr1, NODE_NONE));
        ASSERT_TRUE(addrman.size() == 1);
        CAddrInfo addr_ret2 = addrman.Select(newOnly);
        ASSERT_TRUE(addr_ret2.ToString() == "[::]:0");

        CAddrInfo addr_ret3 = addrman.Select();
        ASSERT_TRUE(addr_ret3.ToString() == "250.1.1.1:8333");

        ASSERT_TRUE(addrman.size() == 1);


        // Add three addresses to new table.
        CService addr2 = CService("250.3.1.1", 8333);
        CService addr3 = CService("250.3.2.2", 9999);
        CService addr4 = CService("250.3.3.3", 9999);

        addrman.Add(CAddress(addr2, NODE_NONE), CService("250.3.1.1", 8333));
        addrman.Add(CAddress(addr3, NODE_NONE), CService("250.3.1.1", 8333));
        addrman.Add(CAddress(addr4, NODE_NONE), CService("250.4.1.1", 8333));

        // Add three addresses to tried table.
        CService addr5 = CService("250.4.4.4", 8333);
        CService addr6 = CService("250.4.5.5", 7777);
        CService addr7 = CService("250.4.6.6", 8333);

        addrman.Add(CAddress(addr5, NODE_NONE), CService("250.3.1.1", 8333));
        addrman.Good(CAddress(addr5, NODE_NONE));
        addrman.Add(CAddress(addr6, NODE_NONE), CService("250.3.1.1", 8333));
        addrman.Good(CAddress(addr6, NODE_NONE));
        addrman.Add(CAddress(addr7, NODE_NONE), CService("250.1.1.3", 8333));
        addrman.Good(CAddress(addr7, NODE_NONE));

        // Test 11: 6 addrs + 1 addr from last test = 7.
        ASSERT_TRUE(addrman.size() == 7);

        // Test 12: Select pulls from new and tried regardless of port number.
        ASSERT_TRUE(addrman.Select().ToString() == "250.4.6.6:8333");
        ASSERT_TRUE(addrman.Select().ToString() == "250.3.2.2:9999");
        ASSERT_TRUE(addrman.Select().ToString() == "250.3.3.3:9999");
        ASSERT_TRUE(addrman.Select().ToString() == "250.4.4.4:8333");

    }

    TEST(TestAddrmanTests, addrman_new_collisions)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        CNetAddr source = CNetAddr("252.2.2.2");

        ASSERT_TRUE(addrman.size() == 0);

        for (unsigned int i = 1; i < 18; i++) {
            CService addr = CService("250.1.1." + boost::to_string(i));
            addrman.Add(CAddress(addr, NODE_NONE), source);
            //Test 13: No collision in new table yet.
            ASSERT_TRUE(addrman.size() == i);
        }

        //Test 14: new table collision!
        CService addr1 = CService("250.1.1.18");
        addrman.Add(CAddress(addr1, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 17);

        CService addr2 = CService("250.1.1.19");
        addrman.Add(CAddress(addr2, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 18);
    }

    TEST(TestAddrmanTests, addrman_tried_collisions)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        CNetAddr source = CNetAddr("252.2.2.2");

        ASSERT_TRUE(addrman.size() == 0);

        for (unsigned int i = 1; i < 80; i++) {
            CService addr = CService("250.1.1." + boost::to_string(i));
            addrman.Add(CAddress(addr, NODE_NONE), source);
            addrman.Good(CAddress(addr, NODE_NONE));

            //Test 15: No collision in tried table yet.
            // GTEST_COUT << addrman.size() << std::endl;
            ASSERT_TRUE(addrman.size() == i);
        }

        //Test 16: tried table collision!
        CService addr1 = CService("250.1.1.80");
        addrman.Add(CAddress(addr1, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 79);

        CService addr2 = CService("250.1.1.81");
        addrman.Add(CAddress(addr2, NODE_NONE), source);
        ASSERT_TRUE(addrman.size() == 80);
    }

    TEST(TestAddrmanTests, addrman_find)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        ASSERT_TRUE(addrman.size() == 0);

        CAddress addr1 = CAddress(CService("250.1.2.1", 8333), NODE_NONE);
        CAddress addr2 = CAddress(CService("250.1.2.1", 9999), NODE_NONE);
        CAddress addr3 = CAddress(CService("251.255.2.1", 8333), NODE_NONE);

        CNetAddr source1 = CNetAddr("250.1.2.1");
        CNetAddr source2 = CNetAddr("250.1.2.2");

        addrman.Add(addr1, source1);
        addrman.Add(addr2, source2);
        addrman.Add(addr3, source1);

        // Test 17: ensure Find returns an IP matching what we searched on.
        CAddrInfo* info1 = addrman.Find(addr1);
        ASSERT_TRUE(info1);
        if (info1)
            ASSERT_TRUE(info1->ToString() == "250.1.2.1:8333");

        // Test 18; Find does not discriminate by port number.
        CAddrInfo* info2 = addrman.Find(addr2);
        ASSERT_TRUE(info2);
        if (info2)
            ASSERT_TRUE(info2->ToString() == info1->ToString());

        // Test 19: Find returns another IP matching what we searched on.
        CAddrInfo* info3 = addrman.Find(addr3);
        ASSERT_TRUE(info3);
        if (info3)
            ASSERT_TRUE(info3->ToString() == "251.255.2.1:8333");
    }

    TEST(TestAddrmanTests, addrman_create)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        ASSERT_TRUE(addrman.size() == 0);

        CAddress addr1 = CAddress(CService("250.1.2.1", 8333), NODE_NONE);
        CNetAddr source1 = CNetAddr("250.1.2.1");

        int nId;
        CAddrInfo* pinfo = addrman.Create(addr1, source1, &nId);

        // Test 20: The result should be the same as the input addr.
        ASSERT_TRUE(pinfo->ToString() == "250.1.2.1:8333");

        CAddrInfo* info2 = addrman.Find(addr1);
        ASSERT_TRUE(info2->ToString() == "250.1.2.1:8333");
    }


    TEST(TestAddrmanTests, addrman_delete)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        ASSERT_TRUE(addrman.size() == 0);

        CAddress addr1 = CAddress(CService("250.1.2.1", 8333), NODE_NONE);
        CNetAddr source1 = CNetAddr("250.1.2.1");

        int nId;
        addrman.Create(addr1, source1, &nId);

        // Test 21: Delete should actually delete the addr.
        ASSERT_TRUE(addrman.size() == 1);
        addrman.Delete(nId);
        ASSERT_TRUE(addrman.size() == 0);
        CAddrInfo* info2 = addrman.Find(addr1);
        ASSERT_TRUE(info2 == NULL);
    }

    TEST(TestAddrmanTests, addrman_getaddr)
    {
        CAddrManTest addrman;

        // Set addrman addr placement to be deterministic.
        addrman.MakeDeterministic();

        // Test 22: Sanity check, GetAddr should never return anything if addrman
        //  is empty.
        ASSERT_TRUE(addrman.size() == 0);
        vector<CAddress> vAddr1 = addrman.GetAddr();
        ASSERT_TRUE(vAddr1.size() == 0);

        CAddress addr1 = CAddress(CService("250.250.2.1", 8333), NODE_NONE);
        addr1.nTime = GetTime(); // Set time so isTerrible = false
        CAddress addr2 = CAddress(CService("250.251.2.2", 9999), NODE_NONE);
        addr2.nTime = GetTime();
        CAddress addr3 = CAddress(CService("251.252.2.3", 8333), NODE_NONE);
        addr3.nTime = GetTime();
        CAddress addr4 = CAddress(CService("252.253.3.4", 8333), NODE_NONE);
        addr4.nTime = GetTime();
        CAddress addr5 = CAddress(CService("252.254.4.5", 8333), NODE_NONE);
        addr5.nTime = GetTime();
        CNetAddr source1 = CNetAddr("250.1.2.1");
        CNetAddr source2 = CNetAddr("250.2.3.3");

        // Test 23: Ensure GetAddr works with new addresses.
        addrman.Add(addr1, source1);
        addrman.Add(addr2, source2);
        addrman.Add(addr3, source1);
        addrman.Add(addr4, source2);
        addrman.Add(addr5, source1);

        // GetAddr returns 23% of addresses, 23% of 5 is 1 rounded down.
        ASSERT_TRUE(addrman.GetAddr().size() == 1);

        // Test 24: Ensure GetAddr works with new and tried addresses.
        addrman.Good(CAddress(addr1, NODE_NONE));
        addrman.Good(CAddress(addr2, NODE_NONE));
        ASSERT_TRUE(addrman.GetAddr().size() == 1);

        // Test 25: Ensure GetAddr still returns 23% when addrman has many addrs.
        for (unsigned int i = 1; i < (8 * 256); i++) {
            int octet1 = i % 256;
            int octet2 = (i / 256) % 256;
            int octet3 = (i / (256 * 2)) % 256;
            string strAddr = boost::to_string(octet1) + "." + boost::to_string(octet2) + "." + boost::to_string(octet3) + ".23";
            CAddress addr = CAddress(CService(strAddr), NODE_NONE);

            // Ensure that for all addrs in addrman, isTerrible == false.
            addr.nTime = GetTime();
            addrman.Add(addr, CNetAddr(strAddr));
            if (i % 8 == 0)
                addrman.Good(addr);
        }
        vector<CAddress> vAddr = addrman.GetAddr();

        size_t percent23 = (addrman.size() * 23) / 100;
        ASSERT_TRUE(vAddr.size() == percent23);
        ASSERT_TRUE(vAddr.size() == 461);
        // (Addrman.size() < number of addresses added) due to address collisons.
        ASSERT_TRUE(addrman.size() == 2007);
    }

    TEST(TestAddrmanTests, caddrinfo_get_tried_bucket_legacy)
    {
        CAddrManTest addrman;

        CAddress addr1 = CAddress(ResolveService("250.1.1.1", 8333), NODE_NONE);
        CAddress addr2 = CAddress(ResolveService("250.1.1.1", 9999), NODE_NONE);

        CNetAddr source1 = ResolveIP("250.1.1.1");

        CAddrInfo info1 = CAddrInfo(addr1, source1);

        uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
        uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

        std::vector<bool> asmap; // use /16

        ASSERT_EQ(info1.GetTriedBucket(nKey1, asmap), 40);

        // Test: Make sure key actually randomizes bucket placement. A fail on
        //  this test could be a security issue.
        ASSERT_TRUE(info1.GetTriedBucket(nKey1, asmap) != info1.GetTriedBucket(nKey2, asmap));

        // Test: Two addresses with same IP but different ports can map to
        //  different buckets because they have different keys.
        CAddrInfo info2 = CAddrInfo(addr2, source1);

        ASSERT_TRUE(info1.GetKey() != info2.GetKey());
        ASSERT_TRUE(info1.GetTriedBucket(nKey1, asmap) != info2.GetTriedBucket(nKey1, asmap));

        std::set<int> buckets;
        for (int i = 0; i < 255; i++) {
            CAddrInfo infoi = CAddrInfo(
                CAddress(ResolveService("250.1.1." + boost::to_string(i)), NODE_NONE),
                ResolveIP("250.1.1." + boost::to_string(i)));
            int bucket = infoi.GetTriedBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the same /16 prefix should
        // never get more than 8 buckets with legacy grouping
        ASSERT_EQ(buckets.size(), 8U);

        buckets.clear();
        for (int j = 0; j < 255; j++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("250." + boost::to_string(j) + ".1.1"), NODE_NONE),
                ResolveIP("250." + boost::to_string(j) + ".1.1"));
            int bucket = infoj.GetTriedBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different /16 prefix should map to more than
        // 8 buckets with legacy grouping
        ASSERT_EQ(buckets.size(), 160U);
    }

    TEST(TestAddrmanTests, caddrinfo_get_new_bucket_legacy)
    {
        CAddrManTest addrman;

        CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
        CAddress addr2 = CAddress(ResolveService("250.1.2.1", 9999), NODE_NONE);

        CNetAddr source1 = ResolveIP("250.1.2.1");

        CAddrInfo info1 = CAddrInfo(addr1, source1);

        uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
        uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

        std::vector<bool> asmap; // use /16

        // Test: Make sure the buckets are what we expect
        ASSERT_EQ(info1.GetNewBucket(nKey1, asmap), 786);
        ASSERT_EQ(info1.GetNewBucket(nKey1, source1, asmap), 786);

        // Test: Make sure key actually randomizes bucket placement. A fail on
        //  this test could be a security issue.
        ASSERT_TRUE(info1.GetNewBucket(nKey1, asmap) != info1.GetNewBucket(nKey2, asmap));

        // Test: Ports should not affect bucket placement in the addr
        CAddrInfo info2 = CAddrInfo(addr2, source1);
        ASSERT_TRUE(info1.GetKey() != info2.GetKey());
        ASSERT_EQ(info1.GetNewBucket(nKey1, asmap), info2.GetNewBucket(nKey1, asmap));

        std::set<int> buckets;
        for (int i = 0; i < 255; i++) {
            CAddrInfo infoi = CAddrInfo(
                CAddress(ResolveService("250.1.1." + boost::to_string(i)), NODE_NONE),
                ResolveIP("250.1.1." + boost::to_string(i)));
            int bucket = infoi.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the same group (\16 prefix for IPv4) should
        //  always map to the same bucket.
        ASSERT_EQ(buckets.size(), 1U);

        buckets.clear();
        for (int j = 0; j < 4 * 255; j++) {
            CAddrInfo infoj = CAddrInfo(CAddress(
                                            ResolveService(
                                                boost::to_string(250 + (j / 255)) + "." + boost::to_string(j % 256) + ".1.1"), NODE_NONE),
                ResolveIP("251.4.1.1"));
            int bucket = infoj.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the same source groups should map to NO MORE
        //  than 64 buckets.
        ASSERT_TRUE(buckets.size() <= 64);

        buckets.clear();
        for (int p = 0; p < 255; p++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("250.1.1.1"), NODE_NONE),
                ResolveIP("250." + boost::to_string(p) + ".1.1"));
            int bucket = infoj.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different source groups should map to MORE
        //  than 64 buckets.
        ASSERT_TRUE(buckets.size() > 64);

    }

    // The following three test cases use asmap_raw[] from asmap.raw file
    // We use an artificial minimal mock mapping
    // 250.0.0.0/8 AS1000
    // 101.1.0.0/16 AS1
    // 101.2.0.0/16 AS2
    // 101.3.0.0/16 AS3
    // 101.4.0.0/16 AS4
    // 101.5.0.0/16 AS5
    // 101.6.0.0/16 AS6
    // 101.7.0.0/16 AS7
    // 101.8.0.0/16 AS8

    TEST(TestAddrmanTests, caddrinfo_get_tried_bucket)
    {
        CAddrManTest addrman;

        CAddress addr1 = CAddress(ResolveService("250.1.1.1", 8333), NODE_NONE);
        CAddress addr2 = CAddress(ResolveService("250.1.1.1", 9999), NODE_NONE);

        CNetAddr source1 = ResolveIP("250.1.1.1");


        CAddrInfo info1 = CAddrInfo(addr1, source1);

        uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
        uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

        std::vector<bool> asmap = FromBytes(asmap_raw, sizeof(asmap_raw) * 8);

        ASSERT_EQ(info1.GetTriedBucket(nKey1, asmap), 236);

        // Test: Make sure key actually randomizes bucket placement. A fail on
        //  this test could be a security issue.
        ASSERT_TRUE(info1.GetTriedBucket(nKey1, asmap) != info1.GetTriedBucket(nKey2, asmap));

        // Test: Two addresses with same IP but different ports can map to
        //  different buckets because they have different keys.
        CAddrInfo info2 = CAddrInfo(addr2, source1);

        ASSERT_TRUE(info1.GetKey() != info2.GetKey());
        ASSERT_TRUE(info1.GetTriedBucket(nKey1, asmap) != info2.GetTriedBucket(nKey1, asmap));

        std::set<int> buckets;
        for (int j = 0; j < 255; j++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("101." + boost::to_string(j) + ".1.1"), NODE_NONE),
                ResolveIP("101." + boost::to_string(j) + ".1.1"));
            int bucket = infoj.GetTriedBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different /16 prefix MAY map to more than
        // 8 buckets.
        ASSERT_TRUE(buckets.size() > 8);

        buckets.clear();
        for (int j = 0; j < 255; j++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("250." + boost::to_string(j) + ".1.1"), NODE_NONE),
                ResolveIP("250." + boost::to_string(j) + ".1.1"));
            int bucket = infoj.GetTriedBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different /16 prefix MAY NOT map to more than
        // 8 buckets.
        ASSERT_TRUE(buckets.size() == 8);
    }

    TEST(TestAddrmanTests, caddrinfo_get_new_bucket)
    {
        CAddrManTest addrman;

        CAddress addr1 = CAddress(ResolveService("250.1.2.1", 8333), NODE_NONE);
        CAddress addr2 = CAddress(ResolveService("250.1.2.1", 9999), NODE_NONE);

        CNetAddr source1 = ResolveIP("250.1.2.1");

        CAddrInfo info1 = CAddrInfo(addr1, source1);

        uint256 nKey1 = (uint256)(CHashWriter(SER_GETHASH, 0) << 1).GetHash();
        uint256 nKey2 = (uint256)(CHashWriter(SER_GETHASH, 0) << 2).GetHash();

        std::vector<bool> asmap = FromBytes(asmap_raw, sizeof(asmap_raw) * 8);

        // Test: Make sure the buckets are what we expect
        ASSERT_EQ(info1.GetNewBucket(nKey1, asmap), 795);
        ASSERT_EQ(info1.GetNewBucket(nKey1, source1, asmap), 795);

        // Test: Make sure key actually randomizes bucket placement. A fail on
        //  this test could be a security issue.
        ASSERT_TRUE(info1.GetNewBucket(nKey1, asmap) != info1.GetNewBucket(nKey2, asmap));

        // Test: Ports should not affect bucket placement in the addr
        CAddrInfo info2 = CAddrInfo(addr2, source1);
        ASSERT_TRUE(info1.GetKey() != info2.GetKey());
        ASSERT_EQ(info1.GetNewBucket(nKey1, asmap), info2.GetNewBucket(nKey1, asmap));

        std::set<int> buckets;
        for (int i = 0; i < 255; i++) {
            CAddrInfo infoi = CAddrInfo(
                CAddress(ResolveService("250.1.1." + boost::to_string(i)), NODE_NONE),
                ResolveIP("250.1.1." + boost::to_string(i)));
            int bucket = infoi.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the same /16 prefix
        // usually map to the same bucket.
        ASSERT_EQ(buckets.size(), 1U);

        buckets.clear();
        for (int j = 0; j < 4 * 255; j++) {
            CAddrInfo infoj = CAddrInfo(CAddress(
                                            ResolveService(
                                                boost::to_string(250 + (j / 255)) + "." + boost::to_string(j % 256) + ".1.1"), NODE_NONE),
                ResolveIP("251.4.1.1"));
            int bucket = infoj.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the same source /16 prefix should not map to more
        // than 64 buckets.
        ASSERT_TRUE(buckets.size() <= 64);

        buckets.clear();
        for (int p = 0; p < 255; p++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("250.1.1.1"), NODE_NONE),
                ResolveIP("101." + boost::to_string(p) + ".1.1"));
            int bucket = infoj.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different source /16 prefixes usually map to MORE
        // than 1 bucket.
        ASSERT_TRUE(buckets.size() > 1);

        buckets.clear();
        for (int p = 0; p < 255; p++) {
            CAddrInfo infoj = CAddrInfo(
                CAddress(ResolveService("250.1.1.1"), NODE_NONE),
                ResolveIP("250." + boost::to_string(p) + ".1.1"));
            int bucket = infoj.GetNewBucket(nKey1, asmap);
            buckets.insert(bucket);
        }
        // Test: IP addresses in the different source /16 prefixes sometimes map to NO MORE
        // than 1 bucket.
        ASSERT_TRUE(buckets.size() == 1);
    }

}
