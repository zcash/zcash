// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CORE_IO_H
#define BITCOIN_CORE_IO_H

#include <string>
#include <vector>

class CBlock;
class CScript;
class CTransaction;
class uint256;
class UniValue;

// core_read.cpp
extern CScript ParseScript(const std::string& s);
extern std::string ScriptToAsmStr(const CScript& script, const bool fAttemptSighashDecode = false);

/**
 * Deserialize a transaction from a hex string.
 *
 * On success, populates `tx`. On failure, throws an exception:
 *   - `consensus_rule_failure` if the input bytes violate a consensus rule
 *     (e.g. a structural/type violation enforced at the parsing layer).
 *     Callers with peer context (i.e. P2P handlers) SHOULD treat this as
 *     grounds for `Misbehaving(pnode, 100)`.
 *   - `std::ios_base::failure` for other deserialization failures (not a
 *     valid hex string, unexpected end of stream, etc.).
 *
 * Note: the P2P `tx` and `block` message handlers do not go through this
 * function; they read directly from a `CDataStream` and must apply the same
 * policy themselves via a try/catch around the read.
 */
extern void DecodeHexTx(CTransaction& tx, const std::string& strHexTx);
extern bool DecodeHexBlk(CBlock&, const std::string& strHexBlk);
extern uint256 ParseHashUV(const UniValue& v, const std::string& strName);
extern uint256 ParseHashStr(const std::string&, const std::string& strName);
extern std::vector<unsigned char> ParseHexUV(const UniValue& v, const std::string& strName);

// core_write.cpp
extern std::string FormatScript(const CScript& script);
extern std::string EncodeHexTx(const CTransaction& tx);
extern void ScriptPubKeyToUniv(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex);
extern void TxToUniv(const CTransaction& tx, const uint256& hashBlock, UniValue& entry);

#endif // BITCOIN_CORE_IO_H
