#include <gtest/gtest.h>
// #include "util/asmap.h"

#include <gtest/gtest.h>

#include "addrman.h"
#include "test/test_bitcoin.h"
#include <string>

#include "hash.h"
#include "random.h"

#define NODE_NONE 0
#define GTEST_COUT std::cerr << "[          ] [ INFO ] "

using namespace std;

namespace TestAddrmanTests {

    class CAddrManTest : public CAddrMan
    {
        uint64_t state;

        public:
            CAddrManTest()
            {
                state = 1;
            }

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
    };

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

}