// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_MEMO_H
#define ZCASH_WALLET_MEMO_H

#include "util/strencodings.h"
#include "zcash/Zcash.h"
#include "tinyformat.h"


enum class MemoError {
    HexDecodeError,
    MemoTooLong
};

typedef std::array<unsigned char, ZC_MEMO_SIZE> MemoBytes;

class Memo {
private:
    MemoBytes value;
public:
    // initialize to default memo (no_memo), see section 5.5 of the protocol spec
    Memo(): value({{0xF6}}) { }
    Memo(MemoBytes value): value(value) {}

    static Memo NoMemo() {
        return Memo();
    }

    static std::variant<MemoError, Memo> FromHex(const std::string& memoHex) {
        Memo result;
        std::vector<unsigned char> rawMemo = ParseHex(memoHex.c_str());

        // If ParseHex comes across a non-hex char, it will stop but still return results so far.
        size_t slen = memoHex.length();
        if (slen != rawMemo.size() * 2) {
            return MemoError::HexDecodeError;
        }

        if (rawMemo.size() > ZC_MEMO_SIZE) {
            return MemoError::MemoTooLong;
        }

        auto rest = std::copy(rawMemo.begin(), rawMemo.end(), result.value.begin());
        std::fill(rest, result.value.end(), 0);

        return result;
    }

    static Memo FromHexOrThrow(const std::string& memoHex) {
        return examine(Memo::FromHex(memoHex), match {
            [&](Memo memo) {
                return memo;
            },
            [&](MemoError err) {
                switch (err) {
                    case MemoError::HexDecodeError:
                        throw std::runtime_error(
                                "Invalid parameter, expected memo data in hexadecimal format.");
                    case MemoError::MemoTooLong:
                        throw std::runtime_error(strprintf(
                                "Invalid parameter, memo is longer than the maximum allowed %d characters.",
                                ZC_MEMO_SIZE));
                    default:
                        assert(false);
                }
                // unreachable, but the compiler can't tell
                return Memo::NoMemo();
            }
        });
    }

    // This copies, because if it returns a reference to the underlying value,
    // using an idiom like `Memo::FromHexOrThrow("abcd").ToBytes()` is unsafe
    // and can result in pointing to memory that has been deallocated.
    MemoBytes ToBytes() const {
        return value;
    }

    std::string ToHex() const {
        return HexStr(value);
    }
};

#endif
