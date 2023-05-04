// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_ZCASH_MEMO_H
#define ZCASH_ZCASH_MEMO_H

#include <tl/expected.hpp>

#include <array>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace libzcash
{

/// Memos are described in the Zcash Protocol Specification §§ 3.2.1 & 5.5 and ZIP 302.
///
/// Memos are generally wrapped in `std::optional` with the special “no memo” byte string of
/// `0xF60000…00`  represented as `std::nullopt`. For this reason, there are a number of static
/// members that handle `std::optional<Memo>` instead of `Memo` directly.
class Memo
{
public:
    static constexpr size_t SIZE{512};

    typedef unsigned char Byte;

    typedef std::array<Byte, SIZE> Bytes;

    /// These are memo contents that have not yet been assigned a meaning. The type is the same as
    /// `MemoBytes`, but is used in the result of `Interpret` for memos that have yet to be given an
    /// interpretation.
    typedef std::array<Byte, SIZE> FutureData;

    /// Arbitrary data (initial byte 0xFF) size must be exactly one less than `SIZE` because the
    /// initial byte is not part of the arbitrary data and we don’t know what type of padding is
    /// required for the data, so we can’t internally pad short data.
    typedef std::array<Byte, SIZE - 1> ArbitraryData;

    /// The possible interpretations of the memo’s content.
    ///
    /// NB: The empty memo case doesn’t need to be covered here, because it is not represented by
    ///     `Memo` proper.
    typedef std::variant<
        /// the first byte (byte 0) has a value of 0xF4 or smaller
        std::string,
        /// * the first byte has a value of 0xF5;
        /// * the first byte has a value of 0xF6, and the remaining 511 bytes are not all 0x00; or
        /// * the first byte has a value between 0xF7 and 0xFE inclusive
        FutureData,
        /// the first byte has a value of 0xFF […] the remaining 511 bytes are then unconstrained
        ArbitraryData
        > Contents;

private:
    Bytes value_;

    static constexpr const Bytes noMemo{0xf6};

    /// This constructor trusts that the Memo is a valid memo, and constructs one even if it’s the
    /// `noMemo` value.
    explicit Memo(Bytes value): value_(value) {}

public:
    enum class ConversionError {
        MemoTooLong,
    };

    enum class TextConversionError {
        MemoTooLong,
        InvalidUTF8,
    };

    enum class InterpretationError {
        InvalidUTF8,
    };

    /// Creates a memo from arbitrary data, which will always be prefixed by `0xFF`. This can _not_
    /// be use to create an arbitrary memo (e.g., this will never have a UTF-8 representation or be
    /// the empty memo).
    Memo(const ArbitraryData& data);

    friend bool operator==(const Memo& a, const Memo& b);

    // TODO: Remove once we’re using C++20.
    friend bool operator!=(const Memo& a, const Memo& b);

    static std::optional<Memo> FromBytes(const Bytes& rawMemo);

    static std::optional<Memo> FromBytes(const Byte (&rawMemo)[SIZE]);

    static tl::expected<std::optional<Memo>, ConversionError>
    FromBytes(const std::vector<Byte>& rawMemo);

    /// Only supports UTF8-encoded memos. I.e., this doesn’t return in `std::optional`, because the
    /// argument can’t contain the `noMemo` value.
    static tl::expected<Memo, TextConversionError> FromText(const std::string& memoStr);

    const Bytes& ToBytes() const;

    static const Bytes& ToBytes(const std::optional<Memo>& memo);

    /// Interprets the memo according to https://zips.z.cash/zip-0302#specification. The
    /// uninterpreted contents can be accessed via `ToBytes` and `ToHex`.
    tl::expected<Contents, InterpretationError> Interpret() const;
};
}

#endif // ZCASH_ZCASH_MEMO_H
