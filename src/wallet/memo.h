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

    static tl::expected<Memo, MemoError> FromHex(const std::string& memoHex) {
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

    // This copies, because if it returns a reference to the underlying value,
    MemoBytes ToBytes() const {
        return value;
    }

    std::string ToHex() const {
        return HexStr(value);
    }
};

#endif
