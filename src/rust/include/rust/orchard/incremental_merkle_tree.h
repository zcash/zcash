// Copyright (c) 2021 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_ORCHARD_INCREMENTAL_MERKLE_TREE_H
#define ZCASH_RUST_INCLUDE_RUST_ORCHARD_INCREMENTAL_MERKLE_TREE_H

#include "rust/streams.h"
#include "rust/orchard.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SINSEMILLA_DIGEST_LEN 32U

/// Pointer to an Orchard incremental merkle tree frontier
struct OrchardMerkleFrontierPtr;
typedef struct OrchardMerkleFrontierPtr OrchardMerkleFrontierPtr;

// Create an empty Orchard Merkle frontier.
//
// Memory allocated to the resulting value must be manually freed.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_empty();

// Clones the given Orchard Merkle frontier and returns
// a pointer to the newly created tree. Both the original
// tree's memory and the newly allocated one need to be freed
// independently.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_clone(
        const OrchardMerkleFrontierPtr* tree_ptr);

// Free the memory allocated for the given Orchard Merkle frontier.
void orchard_merkle_frontier_free(
        OrchardMerkleFrontierPtr* tree_ptr);

// Parses an Orchard Merkle frontier from a stream. If parsing
// fails, this will return the null pointer.
//
// Memory allocated to the resulting value must be manually freed.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_parse(
        void* stream,
        read_callback_t read_cb);

// Serializes an Orchard Merkle frontier to a stream.
//
// Returns `false` if an error occurs while writing to the stream.
bool orchard_merkle_frontier_serialize(
        const OrchardMerkleFrontierPtr* tree_ptr,
        void* stream,
        write_callback_t write_cb);

// For each action in the provided bundle, append its
// commitment to the frontier.
//
// Returns `true` if the append succeeds, `false` if the
// tree is full.
bool orchard_merkle_frontier_append_bundle(
        OrchardMerkleFrontierPtr* tree_ptr,
        const OrchardBundlePtr* bundle);

// Computes the root of the provided orchard Merkle frontier
void orchard_merkle_frontier_root(
        const OrchardMerkleFrontierPtr* tree_ptr,
        unsigned char* digest_ret);

// The total number of leaves that have been appended to obtain
// the current state of the frontier. Subtract 1 from this value
// to obtain the position of the most recently appended leaf.
uint64_t orchard_merkle_frontier_num_leaves(
        const OrchardMerkleFrontierPtr* tree_ptr);

// Estimate the amount of memory consumed by the merkle frontier.
size_t orchard_merkle_frontier_dynamic_mem_usage(
        const OrchardMerkleFrontierPtr* tree_ptr);

// Computes the empty leaf value for the incremental Merkle tree.
void orchard_merkle_tree_empty_root(
        unsigned char* digest_ret);

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_ORCHARD_INCREMENTAL_MERKLE_TREE_H
