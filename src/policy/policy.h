// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_POLICY_POLICY_H
#define BITCOIN_POLICY_POLICY_H

#include "consensus/consensus.h"
#include "script/interpreter.h"
#include "script/standard.h"

#include <string>

class CChainParams;
class CCoinsViewCache;

/** Default for -blockmaxsize, which controls the maximum block size the mining code will create **/
static const unsigned int DEFAULT_BLOCK_MAX_SIZE = MAX_BLOCK_SIZE;

/** Maximum number of signature check operations in an IsStandard() P2SH script */
static const unsigned int MAX_P2SH_SIGOPS = 15;
/** The maximum number of sigops we're willing to relay/mine in a single tx */
static const unsigned int MAX_STANDARD_TX_SIGOPS = MAX_BLOCK_SIGOPS/5;
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

/**
 * The rate used to calculate the dust threshold, in zatoshis per 1000 bytes.
 * This will effectively be multiplied by 3 (it is necessary to express it this
 * way to ensure that rounding will be calculated the same as before #6542).
 *
 * Bitcoin Core added the concept of "dust" in bitcoin/bitcoin#2577. At that
 * point the dust threshold rate was tied to three times the minRelayTxFee rate,
 * with the motivation that if you'd pay more than a third of the minimum relay
 * fee to spend something, it should be considered dust. This was implemented
 * as a standard rule rejecting dust outputs.
 *
 * This motivation will not apply after ZIP 317 block construction is implemented:
 * at that point the ZIP 317 marginal fee will be 5000 zats per logical action,
 * but the dust threshold rate will still be 300 zats per 1000 bytes. Those costs
 * would only coincide if the marginal size per logical action were
 * 5000/300 * 1000 ~= 16667 bytes, and in practice the marginal size for any
 * kind of input is much smaller than that.
 *
 * However, to avoid interoperability problems (older wallets creating transactions
 * that newer nodes will reject because they view the outputs as dust), we will have
 * to coordinate any increase in the dust threshold carefully.
 *
 * More history: in Zcash the minRelayTxFee rate was 5000 zats/1000 bytes at launch,
 * changed to 1000 zats/1000 bytes in zcashd v1.0.3 and to 100 zats/1000 bytes in
 * zcashd v1.0.7-1 (#2141). The relaying problem for shielded transactions (#1969)
 * that prompted the latter change was fixed more thoroughly by the addition of
 * `CFeeRate::GetFeeForRelay` in #4916, ensuring that a transaction paying
 * `DEFAULT_FEE` can always be relayed. At the same time the default fee was set
 * to 1000 zats, per ZIP 313.
 *
 * #6542 changed relaying policy to be more strict about enforcing minRelayTxFee.
 * It also allowed `-minrelaytxfee=0`, which we are using to avoid some test
 * breakage. But if the dust threshold rate were still set to three times the
 * minRelayTxFee rate, then setting `-minrelaytxfee=0` would have the side effect
 * of setting the dust threshold to zero, which is not intended.
 *
 * Bitcoin Core took a different approach to disentangling the dust threshold from
 * the relay threshold, adding a `-dustrelayfee` option (bitcoin/bitcoin#9380).
 * We don't want to do that because it is likely that we will change the dust policy
 * again, and adding a user-visible config option might conflict with that. Also,
 * it isn't a good idea for the dust threshold rate to be configurable per node;
 * it's a standard rule parameter and should only be changed with network-wide
 * coordination (if it is increased then wallets have to change before nodes, and
 * vice versa if it is decreased). So for now we set it to a constant that matches
 * the behaviour before #6542.
 */
static const unsigned int ONE_THIRD_DUST_THRESHOLD_RATE = 100;

// Sanity check the magic numbers when we change them
static_assert(DEFAULT_BLOCK_MAX_SIZE <= MAX_BLOCK_SIZE);

bool IsStandard(const CScript& scriptPubKey, txnouttype& whichType);
    /**
     * Check for standard transaction types
     * @return True if all outputs (scriptPubKeys) use only standard transaction forms
     */
bool IsStandardTx(const CTransaction& tx, std::string& reason, const CChainParams& chainparams, int nHeight = 0);
    /**
     * Check for standard transaction types
     * @param[in] mapInputs    Map of previous transactions that have outputs we're spending
     * @return True if all inputs (scriptSigs) use only standard transaction forms
     */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs, uint32_t consensusBranchId);

#endif // BITCOIN_POLICY_POLICY_H
