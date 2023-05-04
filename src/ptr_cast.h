// Copyright (c) 2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_PTR_CAST_H
#define ZCASH_PTR_CAST_H

#include <cassert>
#include <memory>

/// Reinterpret p as an R*, aborting (rather than invoking UB) if p is nullptr
/// or does not satisfy the alignment requirements of an R*. To avoid UB, the
/// caller must ensure all other safety requirements for reinterpret_cast<R*>(p).
template<typename R, typename T>
static inline R* ptr_cast_checking_alignment(T* p) {
    assert(p != nullptr);
    void *pv = p;
    size_t space = sizeof(T);
    std::align(alignof(R), sizeof(R), pv, space);
    assert(pv == p);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    return reinterpret_cast<R*>(p);
#pragma clang diagnostic pop
}

/// Reinterpret p as a const R*, aborting (rather than invoking UB) if p is nullptr
/// or does not satisfy the alignment requirements of a const R*. To avoid UB, the
/// caller must ensure all other safety requirements for reinterpret_cast<const R*>(p).
template<typename R, typename T>
static inline const R* const_ptr_cast_checking_alignment(const T* p) {
    assert(p != nullptr);
    void *pv = const_cast<void*>(static_cast<const void*>(p));
    size_t space = sizeof(T);
    std::align(alignof(R), sizeof(R), pv, space);
    assert(pv == p);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-align"
    return reinterpret_cast<const R*>(p);
#pragma clang diagnostic pop
}

#endif // ZCASH_PTR_CAST_H