// Copyright (c) 2012-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "leveldbwrapper.h"
#include "uint256.h"
#include "random.h"
#include "test/test_bitcoin.h"

#include <boost/assign/std/vector.hpp> // for 'operator+=()'
#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>
                    
using namespace std;
using namespace boost::assign; // bring 'operator+=()' into scope
using namespace boost::filesystem;
         
// Test if a string consists entirely of null characters
bool is_null_key(const vector<unsigned char>& key) {
    bool isnull = true;

    for (unsigned int i = 0; i < key.size(); i++)
        isnull &= (key[i] == '\x00');

    return isnull;
}
 
BOOST_FIXTURE_TEST_SUITE(leveldbwrapper_tests, BasicTestingSetup)
                       
BOOST_AUTO_TEST_CASE(leveldbwrapper)
{
    {
        path ph = temp_directory_path() / unique_path();
        CLevelDBWrapper dbw(ph, (1 << 20), true, false);
        char key = 'k';
        uint256 in = GetRandHash();
        uint256 res;

        BOOST_CHECK(dbw.Write(key, in));
        BOOST_CHECK(dbw.Read(key, res));
        BOOST_CHECK_EQUAL(res.ToString(), in.ToString());
    }
}
 
BOOST_AUTO_TEST_SUITE_END()
