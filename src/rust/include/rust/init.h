// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_INIT_H
#define ZCASH_RUST_INCLUDE_RUST_INIT_H

#ifdef __cplusplus
extern "C" {
#endif

/// Initializes the global Rayon threadpool.
void zcashd_init_rayon_threadpool();

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_INIT_H
