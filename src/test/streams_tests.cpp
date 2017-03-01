// Copyright (c) 2019 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#include "fs.h"
#include "main.h"
#include "test/test_bitcoin.h"
#include "test/test_random.h"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(streams_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(streams_buffered_file)
{
    FILE* file = fopen("streams_test_tmp", "w+b");
    // The value at each offset is the offset.
    for (uint8_t j = 0; j < 40; ++j) {
        fwrite(&j, 1, 1, file);
    }
    rewind(file);

    // The buffer size (second arg) must be greater than the rewind
    // amount (third arg).
    try {
        CBufferedFile bfbad(file, 25, 25, 222, 333);
        BOOST_CHECK(false);
    } catch (const std::exception& e) {
        BOOST_CHECK(strstr(e.what(),
                        "Rewind limit must be less than buffer size") != nullptr);
    }

    // The buffer is 25 bytes, allow rewinding 10 bytes.
    CBufferedFile bf(file, 25, 10, 222, 333);
    BOOST_CHECK(!bf.eof());

    // These two members have no functional effect.
    BOOST_CHECK_EQUAL(bf.GetType(), 222);
    BOOST_CHECK_EQUAL(bf.GetVersion(), 333);

    uint8_t i;
    bf >> i;
    BOOST_CHECK_EQUAL(i, 0);
    bf >> i;
    BOOST_CHECK_EQUAL(i, 1);

    // After reading bytes 0 and 1, we're positioned at 2.
    BOOST_CHECK_EQUAL(bf.GetPos(), 2);

    // Rewind to offset 0, ok (within the 10 byte window).
    BOOST_CHECK(bf.SetPos(0));
    bf >> i;
    BOOST_CHECK_EQUAL(i, 0);

    // We can go forward to where we've been, but beyond may fail.
    BOOST_CHECK(bf.SetPos(2));
    bf >> i;
    BOOST_CHECK_EQUAL(i, 2);

    // If you know the maximum number of bytes that should be
    // read to deserialize the variable, you can limit the read
    // extent. The current file offset is 3, so the following
    // SetLimit() allows zero bytes to be read.
    BOOST_CHECK(bf.SetLimit(3));
    try {
        bf >> i;
        BOOST_CHECK(false);
    } catch (const std::exception& e) {
        BOOST_CHECK(strstr(e.what(),
                        "Read attempted past buffer limit") != nullptr);
    }
    // The default argument removes the limit completely.
    BOOST_CHECK(bf.SetLimit());
    // The read position should still be at 3 (no change).
    BOOST_CHECK_EQUAL(bf.GetPos(), 3);

    // Read from current offset, 3, forward until position 10.
    for (uint8_t j = 3; j < 10; ++j) {
        bf >> i;
        BOOST_CHECK_EQUAL(i, j);
    }
    BOOST_CHECK_EQUAL(bf.GetPos(), 10);

    // We're guaranteed (just barely) to be able to rewind to zero.
    BOOST_CHECK(bf.SetPos(0));
    BOOST_CHECK_EQUAL(bf.GetPos(), 0);
    bf >> i;
    BOOST_CHECK_EQUAL(i, 0);

    // We can set the position forward again up to the farthest
    // into the stream we've been, but no farther. (Attempting
    // to go farther may succeed, but it's not guaranteed.)
    BOOST_CHECK(bf.SetPos(10));
    bf >> i;
    BOOST_CHECK_EQUAL(i, 10);
    BOOST_CHECK_EQUAL(bf.GetPos(), 11);

    // Now it's only guaranteed that we can rewind to offset 1
    // (current read position, 11, minus rewind amount, 10).
    BOOST_CHECK(bf.SetPos(1));
    BOOST_CHECK_EQUAL(bf.GetPos(), 1);
    bf >> i;
    BOOST_CHECK_EQUAL(i, 1);

    // We can stream into large variables, even larger than
    // the buffer size.
    BOOST_CHECK(bf.SetPos(11));
    {
        uint8_t a[40 - 11];
        bf >> FLATDATA(a);
        for (uint8_t j = 0; j < sizeof(a); ++j) {
            BOOST_CHECK_EQUAL(a[j], 11 + j);
        }
    }
    BOOST_CHECK_EQUAL(bf.GetPos(), 40);

    // We've read the entire file, the next read should throw.
    try {
        bf >> i;
        BOOST_CHECK(false);
    } catch (const std::exception& e) {
        BOOST_CHECK(strstr(e.what(),
                        "CBufferedFile::Fill: end of file") != nullptr);
    }
    // Attempting to read beyond the end sets the EOF indicator.
    BOOST_CHECK(bf.eof());

    // Still at offset 40, we can go back 10, to 30.
    BOOST_CHECK_EQUAL(bf.GetPos(), 40);
    BOOST_CHECK(bf.SetPos(30));
    bf >> i;
    BOOST_CHECK_EQUAL(i, 30);
    BOOST_CHECK_EQUAL(bf.GetPos(), 31);

    // We're too far to rewind to position zero.
    BOOST_CHECK(!bf.SetPos(0));
    // But we should now be positioned at least as far back as allowed
    // by the rewind window (relative to our farthest read position, 40).
    BOOST_CHECK(bf.GetPos() <= 30);

    // We can explicitly close the file, or the destructor will do it.
    bf.fclose();

    boost::filesystem::remove("streams_test_tmp");
}

BOOST_AUTO_TEST_CASE(streams_buffered_file_rand)
{
    // Make this test deterministic.
    seed_insecure_rand(true);

    for (int rep = 0; rep < 500; ++rep) {
        FILE* file = fopen("streams_test_tmp", "w+b");
        size_t fileSize = GetRandInt(256);
        for (uint8_t i = 0; i < fileSize; ++i) {
            fwrite(&i, 1, 1, file);
        }
        rewind(file);

        size_t bufSize = GetRandInt(300) + 1;
        size_t rewindSize = GetRandInt(bufSize);
        CBufferedFile bf(file, bufSize, rewindSize, 222, 333);
        size_t currentPos = 0;
        size_t maxPos = 0;
        for (int step = 0; step < 100; ++step) {
            if (currentPos >= fileSize)
                break;

            // We haven't read to the end of the file yet.
            BOOST_CHECK(!bf.eof());
            BOOST_CHECK_EQUAL(bf.GetPos(), currentPos);

            // Pretend the file consists of a series of objects of varying
            // sizes; the boundaries of the objects can interact arbitrarily
            // with the CBufferFile's internal buffer. These first three
            // cases simulate objects of various sizes (1, 2, 5 bytes).
            switch (GetRandInt(5)) {
            case 0: {
                uint8_t a[1];
                if (currentPos + 1 > fileSize)
                    continue;
                bf.SetLimit(currentPos + 1);
                bf >> FLATDATA(a);
                for (uint8_t i = 0; i < 1; ++i) {
                    BOOST_CHECK_EQUAL(a[i], currentPos);
                    currentPos++;
                }
                break;
            }
            case 1: {
                uint8_t a[2];
                if (currentPos + 2 > fileSize)
                    continue;
                bf.SetLimit(currentPos + 2);
                bf >> FLATDATA(a);
                for (uint8_t i = 0; i < 2; ++i) {
                    BOOST_CHECK_EQUAL(a[i], currentPos);
                    currentPos++;
                }
                break;
            }
            case 2: {
                uint8_t a[5];
                if (currentPos + 5 > fileSize)
                    continue;
                bf.SetLimit(currentPos + 5);
                bf >> FLATDATA(a);
                for (uint8_t i = 0; i < 5; ++i) {
                    BOOST_CHECK_EQUAL(a[i], currentPos);
                    currentPos++;
                }
                break;
            }
            case 3: {
                // Find a byte value (that is at or ahead of the current position).
                size_t find = currentPos + GetRandInt(8);
                if (find >= fileSize)
                    find = fileSize - 1;
                bf.FindByte(static_cast<char>(find));
                // The value at each offset is the offset.
                BOOST_CHECK_EQUAL(bf.GetPos(), find);
                currentPos = find;

                bf.SetLimit(currentPos + 1);
                uint8_t i;
                bf >> i;
                BOOST_CHECK_EQUAL(i, currentPos);
                currentPos++;
                break;
            }
            case 4: {
                size_t requestPos = GetRandInt(maxPos + 4);
                bool okay = bf.SetPos(requestPos);
                // The new position may differ from the requested position
                // because we may not be able to rewind beyond the rewind
                // window, and we may not be able to move forward beyond the
                // farthest position we've reached so far.
                currentPos = bf.GetPos();
                BOOST_CHECK_EQUAL(okay, currentPos == requestPos);
                // Check that we can position within the rewind window.
                if (requestPos <= maxPos &&
                    maxPos > rewindSize &&
                    requestPos >= maxPos - rewindSize) {
                    // We requested a position within the rewind window.
                    BOOST_CHECK(okay);
                }
                break;
            }
            }
            if (maxPos < currentPos)
                maxPos = currentPos;
        }
    }
    boost::filesystem::remove("streams_test_tmp");
}

BOOST_AUTO_TEST_SUITE_END()
