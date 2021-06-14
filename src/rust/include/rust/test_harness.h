// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_TEST_HARNESS_H
#define ZCASH_RUST_INCLUDE_RUST_TEST_HARNESS_H

#ifdef __cplusplus
extern "C" {
#endif

/// Generates a random Jubjub base field element.
///
/// `ret` must point to a 32-byte array.
void zcash_test_harness_random_jubjub_base(
    unsigned char* ret);

/// Generates a random Jubjub point.
///
/// `ret` must point to a 32-byte array.
void zcash_test_harness_random_jubjub_point(
    unsigned char* ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_TEST_HARNESS_H
