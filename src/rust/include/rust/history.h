// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_HISTORY_H
#define ZCASH_RUST_INCLUDE_RUST_HISTORY_H

#include <stddef.h>
#include <stdint.h>

#define NODE_V1_SERIALIZED_LENGTH 171
#define NODE_SERIALIZED_LENGTH 244
#define ENTRY_SERIALIZED_LENGTH (NODE_SERIALIZED_LENGTH + 9)

typedef struct HistoryNode {
    unsigned char bytes[NODE_SERIALIZED_LENGTH];
}  HistoryNode;
static_assert(
    sizeof(HistoryNode) == NODE_SERIALIZED_LENGTH,
    "HistoryNode struct is not the same size as the underlying byte array");
static_assert(alignof(HistoryNode) == 1, "HistoryNode struct alignment is not 1");

typedef struct HistoryEntry {
    unsigned char bytes[ENTRY_SERIALIZED_LENGTH];
}  HistoryEntry;
static_assert(
    sizeof(HistoryEntry) == ENTRY_SERIALIZED_LENGTH,
    "HistoryEntry struct is not the same size as the underlying byte array");
static_assert(alignof(HistoryEntry) == 1, "HistoryEntry struct alignment is not 1");

#ifdef __cplusplus
extern "C" {
#endif
uint32_t librustzcash_mmr_append(
    uint32_t cbranch,
    uint32_t t_len,
    const uint32_t* ni_ptr,
    const HistoryEntry* n_ptr,
    size_t p_len,
    const HistoryNode* nn_ptr,
    unsigned char* rt_ret,
    HistoryNode* buf_ret);

uint32_t librustzcash_mmr_delete(
    uint32_t cbranch,
    uint32_t t_len,
    const uint32_t* ni_ptr,
    const HistoryEntry* n_ptr,
    size_t p_len,
    size_t e_len,
    unsigned char* rt_ret);

uint32_t librustzcash_mmr_hash_node(
    uint32_t cbranch,
    const HistoryNode* n_ptr,
    unsigned char* h_ret);
#ifdef __cplusplus
}
#endif

#endif // ZCASH_RUST_INCLUDE_RUST_HISTORY_H
