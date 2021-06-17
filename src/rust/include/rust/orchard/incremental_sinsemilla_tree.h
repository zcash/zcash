// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_INCREMENTAL_MERKLE_H
#define ZCASH_RUST_INCLUDE_INCREMENTAL_MERKLE_H

#include "rust/streams.h"

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

// Create an empty Orchard Merkle frontier.
//
// Memory allocated to the resulting value must be manually freed.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_empty();

// Clones the given Orchard Merkle frontier and returns
// a pointer to the newly created tree. Both the original
// tree's memory and the newly allocated one need to be freed
// independently.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_clone(
        const OrchardMerkleFrontierPtr* tree_ptr
        );

// Free the memory allocated for the given Orchard Merkle frontier.
void orchard_merkle_frontier_free(
        OrchardMerkleFrontierPtr* tree_ptr
        );

// Parses an Orchard Merkle frontier from a stream. If parsing
// fails, this will return the null pointer.
//
// Memory allocated to the resulting value must be manually freed.
OrchardMerkleFrontierPtr* orchard_merkle_frontier_parse(
        void* stream,
        read_callback_t read_cb
        );

// Parses an Orchard Merkle frontier from a stream. If parsing
// fails, this will return the null pointer.
//
// Memory allocated to the resulting value must be manually freed.
bool orchard_merkle_frontier_serialize(
        const OrchardMerkleFrontierPtr* tree_ptr,
        void* stream,
        write_callback_t write_cb
        );

// For each action in the provided bundle, append its
// commitment to the frontier.
bool orchard_merkle_frontier_append_bundle(
        OrchardMerkleFrontierPtr* tree_ptr,
        const OrchardBundlePtr* bundle
        );

// Compute the root of the provided orchard Merkle frontier
//
// If no error occurs, `digest_ret` will point to the resulting
// digest and this procedure returns `true`.
// If an error occurs, `digest_ret` will be unaltered and this
// procedure returns `false`.
bool orchard_merkle_frontier_root(
        const OrchardMerkleFrontierPtr* tree_ptr,
        unsigned char* digest_ret
        );

// The total number of leaves that have been appended to obtain
// the current state of the frontier. Subtract 1 from this value
// to obtain the position of the most recently appended leaf.
size_t orchard_merkle_frontier_num_leaves(
        const OrchardMerkleFrontierPtr* tree_ptr
        );

/// Pointer to an Orchard incremental sinsemilla tree
struct IncrementalSinsemillaTreePtr;
typedef struct IncrementalSinsemillaTreePtr IncrementalSinsemillaTreePtr;

// Create an empty incremental sinsemilla tree.
//
// Memory allocated to the resulting value must be manually freed.
IncrementalSinsemillaTreePtr* incremental_sinsemilla_tree_empty();

// Clones the given incremental sinsemilla tree and returns
// a pointer to the newly created tree. Both the original
// tree's memory and the newly allocated one need to be freed
// independently.
IncrementalSinsemillaTreePtr* incremental_sinsemilla_tree_clone(
        const IncrementalSinsemillaTreePtr* tree_ptr
        );

// Free the memory allocated for the given incremental sinsemilla tree.
void incremental_sinsemilla_tree_free(
        IncrementalSinsemillaTreePtr* tree_ptr
        );

// Parses an incremental sinsemilla tree from a stream. If parsing
// fails, this will return the null pointer.
//
// Memory allocated to the resulting value must be manually freed.
IncrementalSinsemillaTreePtr* incremental_sinsemilla_tree_parse(
        void* stream,
        read_callback_t read_cb
        );

// Parses an incremental sinsemilla tree from a stream. If parsing
// fails, this will return the null pointer.
//
// Memory allocated to the resulting value must be manually freed.
bool incremental_sinsemilla_tree_serialize(
        const IncrementalSinsemillaTreePtr* tree_ptr,
        void* stream,
        write_callback_t write_cb
        );

// For each action in the provided bundle, append its
// commitment to the incremental Merkle tree.
bool incremental_sinsemilla_tree_append_bundle(
        IncrementalSinsemillaTreePtr* tree_ptr,
        const OrchardBundlePtr* bundle
        );

// Save the current state of the incremental merkle tree
// as a point to which the tree can be rewound.
void incremental_sinsemilla_tree_checkpoint(
        IncrementalSinsemillaTreePtr* tree_ptr
        );

// Rewind the incremental merkle tree to the latest checkpoint.
bool incremental_sinsemilla_tree_rewind(
        IncrementalSinsemillaTreePtr* tree_ptr
        );

// Compute the root of the provided incremental sinsemilla tree.
//
// If no error occurs, `digest_ret` will point to the resulting
// digest and this procedure returns `true`.
// If an error occurs, `digest_ret` will be unaltered and this
// procedure returns `false`.
bool incremental_sinsemilla_tree_root(
        const IncrementalSinsemillaTreePtr* tree_ptr,
        unsigned char* digest_ret
        );

// Return the empty leaf value for the incremental sinsemilla tree.
bool incremental_sinsemilla_tree_empty_root(
        size_t depth,
        unsigned char* digest_ret
        );

#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_INCREMENTAL_MERKLE_H
