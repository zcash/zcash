// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#ifndef BITCOIN_SCRIPT_STANDARD_H
#define BITCOIN_SCRIPT_STANDARD_H

#include "script/interpreter.h"
#include "uint256.h"

#include <boost/variant.hpp>

#include <stdint.h>

class CKeyID;
class CScript;

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public uint160
{
public:
    CScriptID() : uint160() {}
    explicit CScriptID(const CScript& in);
    CScriptID(const uint160& in) : uint160(in) {}
};

static const unsigned int MAX_OP_RETURN_RELAY = 8192;      //! bytes
extern unsigned nMaxDatacarrierBytes;

/**
 * Mandatory script verification flags that all new blocks must comply with for
 * them to be valid. (but old blocks may not comply with) Currently just P2SH,
 * but in the future other flags may be added.
 *
 * Failing one of these tests may trigger a DoS ban - see CheckInputs() for
 * details.
 */
static const unsigned int MANDATORY_SCRIPT_VERIFY_FLAGS = SCRIPT_VERIFY_P2SH;

/**
 * Standard script verification flags that standard transactions will comply
 * with. However scripts violating these flags may still be present in valid
 * blocks and we must accept those blocks.
 */
static const unsigned int STANDARD_SCRIPT_VERIFY_FLAGS = MANDATORY_SCRIPT_VERIFY_FLAGS |
                                                         // SCRIPT_VERIFY_DERSIG is always enforced
                                                         SCRIPT_VERIFY_STRICTENC |
                                                         SCRIPT_VERIFY_MINIMALDATA |
                                                         SCRIPT_VERIFY_NULLDUMMY |
                                                         SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS |
                                                         SCRIPT_VERIFY_CLEANSTACK |
                                                         SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY |
                                                         SCRIPT_VERIFY_LOW_S;

/** For convenience, standard but not mandatory verify flags. */
static const unsigned int STANDARD_NOT_MANDATORY_VERIFY_FLAGS = STANDARD_SCRIPT_VERIFY_FLAGS & ~MANDATORY_SCRIPT_VERIFY_FLAGS;

enum txnouttype
{
    TX_NONSTANDARD,
    // 'standard' transaction types:
    TX_PUBKEY,
    TX_PUBKEYHASH,
    TX_SCRIPTHASH,
    TX_MULTISIG,
    TX_CRYPTOCONDITION,
    TX_NULL_DATA,
};

class CNoDestination {
public:
    friend bool operator==(const CNoDestination &a, const CNoDestination &b) { return true; }
    friend bool operator<(const CNoDestination &a, const CNoDestination &b) { return true; }
};

/** 
 * A txout script template with a specific destination. It is either:
 *  * CNoDestination: no destination set
 *  * CKeyID: TX_PUBKEYHASH destination
 *  * CScriptID: TX_SCRIPTHASH destination
 *  A CTxDestination is the internal data type encoded in a bitcoin address
 */
typedef boost::variant<CNoDestination, CPubKey, CKeyID, CScriptID> CTxDestination;

class COptCCParams
{
    public:
        static const uint8_t VERSION = 1;

        uint8_t version;
        uint8_t evalCode;
        uint8_t m, n; // for m of n sigs required, n pub keys for sigs will follow
        std::vector<CPubKey> vKeys; // n public keys to aid in signing
        std::vector<std::vector<unsigned char>> vData; // extra parameters

        COptCCParams() : version(0), evalCode(0), n(0), m(0) {}

        COptCCParams(uint8_t ver, uint8_t code, uint8_t _n, uint8_t _m, std::vector<CPubKey> &vkeys, std::vector<std::vector<unsigned char>> &vdata) : 
            version(ver), evalCode(code), n(_n), m(_m), vKeys(vkeys), vData(vdata) {}

        COptCCParams(std::vector<unsigned char> &vch);

        bool IsValid() { return version != 0; }

        std::vector<unsigned char> AsVector();
};

class CStakeParams
{
    public:
        static const uint32_t STAKE_MINPARAMS = 4;
        static const uint32_t STAKE_MAXPARAMS = 5;
        
        uint32_t srcHeight;
        uint32_t blkHeight;
        uint256 prevHash;
        CPubKey pk;
    
        CStakeParams() : srcHeight(0), blkHeight(0), prevHash(), pk() {}

        CStakeParams(const std::vector<std::vector<unsigned char>> &vData);

        CStakeParams(uint32_t _srcHeight, uint32_t _blkHeight, const uint256 &_prevHash, const CPubKey &_pk) :
            srcHeight(_srcHeight), blkHeight(_blkHeight), prevHash(_prevHash), pk(_pk) {}

        std::vector<unsigned char> AsVector()
        {
            std::vector<unsigned char> ret;
            CScript scr = CScript();
            scr << OPRETTYPE_STAKEPARAMS;
            scr << srcHeight;
            scr << blkHeight;
            scr << std::vector<unsigned char>(prevHash.begin(), prevHash.end());
            
            if (pk.IsValid())
            {
                scr << std::vector<unsigned char>(pk.begin(), pk.end());
            }
                                    
            ret = std::vector<unsigned char>(scr.begin(), scr.end());
            return ret;
        }

        bool IsValid() { return srcHeight != 0; }
};

/** Check whether a CTxDestination is a CNoDestination. */
bool IsValidDestination(const CTxDestination& dest);

const char* GetTxnOutputType(txnouttype t);

bool Solver(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<std::vector<unsigned char> >& vSolutionsRet);
int ScriptSigArgsExpected(txnouttype t, const std::vector<std::vector<unsigned char> >& vSolutions);
bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType);
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet, bool returnPubKey=false);
bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

CScript GetScriptForDestination(const CTxDestination& dest);
CScript GetScriptForMultisig(int nRequired, const std::vector<CPubKey>& keys);

#endif // BITCOIN_SCRIPT_STANDARD_H
