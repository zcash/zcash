#ifndef SKIPTEST_H
#define SKIPTEST_H

/* FIXME: Is there a standard boost way to do this? */

/** SKIP_TEST provides a hacky way to skip tests, while emitting a single
 * line about the skipped test.
 *
 * To use it, replace ``BOOST_AUTO_TEST_CASE(...)`` with
 * ``SKIP_TEST(...)`` and then comment out the following test body.
 */

#define SKIP_TEST(name)                                             \
    BOOST_AUTO_TEST_CASE(name) {                                    \
        std::cerr << "SKIPPED TEST: " << #name << std::endl;        \
    }


#endif  // SKIPTEST_H
