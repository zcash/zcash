// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_UNDO_H
#define BITCOIN_UNDO_H

#include "compressor.h"
#include "primitives/transaction.h"
#include "serialize.h"

enum CTxUndoVersion {
    UNDO_VERSION_ZERO = 0,
    UNDO_VERSION_ONE = 1
};

/** Undo information for a CTxIn
 *
 *  Contains the prevout's CTxOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTxInUndo
{
public:
    CTxOut txout;         // the txout data before being spent
    bool fCoinBase;       // if the outpoint was the last unspent: whether it belonged to a coinbase
    unsigned int nHeight; // if the outpoint was the last unspent: its height
    int nVersion;         // if the outpoint was the last unspent: its version

    CTxInUndo() : txout(), fCoinBase(false), nHeight(0), nVersion(0) {}
    CTxInUndo(const CTxOut &txoutIn, bool fCoinBaseIn = false, unsigned int nHeightIn = 0, int nVersionIn = 0) : txout(txoutIn), fCoinBase(fCoinBaseIn), nHeight(nHeightIn), nVersion(nVersionIn) { }

    template<typename Stream>
    void Serialize(Stream &s) const {
        // encode the fCoinBase flag along with the block height by bit-shifting
        // the nHeight value and then adding the flag.
        ::Serialize(s, VARINT(nHeight*2+(fCoinBase ? 1 : 0)));
        if (nHeight > 0)
            ::Serialize(s, VARINT(this->nVersion));
        ::Serialize(s, CTxOutCompressor(REF(txout)));
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nCode));
        nHeight = nCode / 2;
        fCoinBase = nCode & 1;
        if (nHeight > 0)
            ::Unserialize(s, VARINT(this->nVersion));
        ::Unserialize(s, REF(CTxOutCompressor(REF(txout))));
    }
};

/** Undo information for a CTzeIn
 *
 *  Contains the prevout's CTzeOut being spent, and if this was the
 *  last output of the affected transaction, its metadata as well
 *  (coinbase or not, height, transaction version)
 */
class CTzeInUndo
{
public:
    CTzeOut tzeout;       // the tzeout data before being spent

    // The following fields are populated only in the case that the associated
    // output is the last unspent output (transparent or TZE) of a transaction.
    // Since blocks are unwound from tip to base, at the time that such an undo
    // is applied this information can be used to reconstruct a CCoins value
    // that caches unspent outputs for the transaction.
    unsigned int nHeight; // if the outpoint was the last unspent: its height
    int nVersion;         // if the outpoint was the last unspent: its version

    CTzeInUndo() : tzeout(), nHeight(0), nVersion(0) {}
    CTzeInUndo(const CTzeOut &tzeoutIn, unsigned int nHeightIn = 0, int nVersionIn = 0):
        tzeout(tzeoutIn), nHeight(nHeightIn), nVersion(nVersionIn) { }

    template<typename Stream>
    void Serialize(Stream &s) const {
        ::Serialize(s, VARINT(nHeight));
        ::Serialize(s, VARINT(nVersion));
        ::Serialize(s, CTzeOutCompressor(REF(tzeout)));
    }

    template<typename Stream>
    void Unserialize(Stream &s) {
        unsigned int nCode = 0;
        ::Unserialize(s, VARINT(nHeight));
        ::Unserialize(s, VARINT(nVersion));
        ::Unserialize(s, REF(CTzeOutCompressor(REF(tzeout))));
    }
};

/** Undo information for a CTransaction */
class CTxUndo {
private:
    CTxUndoVersion version;
public:
    template<typename Stream>
    friend class CTxUndoReadVisitor;

    // undo information for all txins
    CTxUndo(): version(UNDO_VERSION_ONE) { }

    std::vector<CTxInUndo> vprevout;
    std::vector<CTzeInUndo> vtzeprevout;

    template<typename Stream>
    void Serialize(Stream &s) const;

    template<typename Stream>
    void Unserialize(Stream &s);
};

template<typename Stream>
class CTxUndoReadVisitor {
private:
    Stream& s;
    CTxUndo& undo;
public:
    CTxUndoReadVisitor(Stream& sIn, CTxUndo& undoIn): s(sIn), undo(undoIn) {}

    void operator()(uint64_t& nSize) const {
        undo.version = UNDO_VERSION_ZERO;
        ::UnserializeSized(s, nSize, undo.vprevout);
    }

    void operator()(CompactSizeFlags& flags) const {
        undo.version = UNDO_VERSION_ONE;
        if (flags.GetFlags() == 0x00) {
            ::Unserialize(s, undo.vprevout);
            ::Unserialize(s, undo.vtzeprevout);
        } else{
            throw std::ios_base::failure("Unrecognized compact-size encoded flags.");
        }
    }
};

template<typename Stream>
void CTxUndo::Serialize(Stream &s) const {
    if (version == UNDO_VERSION_ZERO) {
        ::Serialize(s, vprevout);
    } else {
        CompactSizeFlags csf(0x00);
        WriteCompactSizeFlags(s, csf);
        ::Serialize(s, vprevout);
        ::Serialize(s, vtzeprevout);
    }
}

template<typename Stream>
void CTxUndo::Unserialize(Stream &s) {
    auto sizeOrFlags = ReadCompactSizeOrFlags(s);
    auto visitor = CTxUndoReadVisitor(s, *this);
    std::visit(visitor, sizeOrFlags);
}

/** Undo information for a CBlock */
class CBlockUndo
{
public:
    std::vector<CTxUndo> vtxundo; // for all but the coinbase
    uint256 old_sprout_tree_root;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(vtxundo);
        READWRITE(old_sprout_tree_root);
    }
};

#endif // BITCOIN_UNDO_H
