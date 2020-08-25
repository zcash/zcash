// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "cleanse.h"

#include <cstring>

#if defined(_MSC_VER)
#include <Windows.h> // For SecureZeroMemory.
#endif

void memory_cleanse(void *ptr, size_t len)
{
#if defined(_MSC_VER)
    /* SecureZeroMemory is guaranteed not to be optimized out by MSVC. */
    SecureZeroMemory(ptr, len);
#else
    std::memset(ptr, 0, len);

    /* In order to prevent the compiler from optimizing out the memset, this uses an
     * unremovable (see https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html#Volatile )
     * asm block that the compiler must assume could access arbitary memory, including
     * the zero bytes written by std::memset.
     *
     * Quoting Adam Langley <agl@google.com> in commit ad1907fe73334d6c696c8539646c21b11178f20f
     * in BoringSSL (ISC License):
     *    As best as we can tell, this is sufficient to break any optimisations that
     *    might try to eliminate "superfluous" memsets.
     * This method is used by memzero_explicit() in the Linux kernel, too. Its advantage is that it
     * is pretty efficient because the compiler can still implement the memset() efficiently,
     * just not remove it entirely. See "Dead Store Elimination (Still) Considered Harmful" by
     * Yang et al. (USENIX Security 2017) for more background.
     */
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}
