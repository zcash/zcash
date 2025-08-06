// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2025 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_MAIN_H
#define BITCOIN_MAIN_H

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/upgrades.h"
#include "fs.h"
#include "net.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "proof_verifier.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "sync.h"
#include "tinyformat.h"
#include "txdb.h"
#include "txmempool.h"
#include "uint256.h"
#include "addressindex.h"
#include "spentindex.h"
#include "timestampindex.h"

#include <algorithm>
#include <exception>
#include <map>
#include <optional>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <rust/bridge.h>
#include <rust/orchard.h>

#include <boost/unordered_map.hpp>

class CBlockIndex;
class CBlockTreeDB;
class CBloomFilter;
class CChainParams;
class CInv;
class CScriptCheck;
class CValidationInterface;
class CValidationState;
class PrecomputedTransactionData;

struct CNodeStateStats;

/** Maximum reorg length we will accept before we shut down and alert the user. */
static const unsigned int MAX_REORG_LENGTH = COINBASE_MATURITY - 1;
/** Default for DEFAULT_WHITELISTRELAY. */
static const bool DEFAULT_WHITELISTRELAY = true;
/** Default for DEFAULT_WHITELISTFORCERELAY. */
static const bool DEFAULT_WHITELISTFORCERELAY = true;
/** Default for -minrelaytxfee, minimum relay fee rate for transactions in zatoshis per 1000 bytes. TODO(misnamed, this is a rate) */
static const unsigned int DEFAULT_MIN_RELAY_TX_FEE = 100;
/** Default for -maxtxfee in zatoshis. */
static const CAmount DEFAULT_TRANSACTION_MAXFEE = 0.1 * COIN;
/** Discourage users from setting fee rates higher than this in zatoshis per 1000 bytes. */
static const CAmount HIGH_TX_FEE_PER_KB = 0.01 * COIN;
/** Warn if -maxtxfee is set to a fee higher than this in zatoshis. */
static const CAmount HIGH_MAX_TX_FEE = 100 * HIGH_TX_FEE_PER_KB;
//! -maxtxfee will error if called with a fee that won’t allow tx to have this many actions
static const unsigned int LOW_LOGICAL_ACTIONS = 10;
/** Default for -maxorphantx, maximum number of orphan transactions kept in memory */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/** Expiration time for orphan transactions in seconds */
static const int64_t ORPHAN_TX_EXPIRE_TIME = 20 * 60;
/** Minimum time between orphan transactions expire time checks in seconds */
static const int64_t ORPHAN_TX_EXPIRE_INTERVAL = 5 * 60;
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 100;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 1800;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 1000;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 5000;
/** Default for -txexpirydelta, in number of blocks */
static const unsigned int DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA = 20;
static const unsigned int DEFAULT_POST_BLOSSOM_TX_EXPIRY_DELTA = DEFAULT_PRE_BLOSSOM_TX_EXPIRY_DELTA * Consensus::BLOSSOM_POW_TARGET_SPACING_RATIO;
/** The number of blocks within expiry height when a tx is considered to be expiring soon */
static constexpr uint32_t TX_EXPIRING_SOON_THRESHOLD = 3;
/** The maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
static const unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
static const unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
/** Number of blocks that can be requested at any given time from a single peer. */
static const int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/** Timeout in seconds during which a peer must stall block download progress before being disconnected. */
static const unsigned int BLOCK_STALLING_TIMEOUT = 2;
/** Number of headers sent in one getheaders result. We rely on the assumption that if a peer sends
 *  less than this number, we reached its tip. Changing this value is a protocol upgrade. */
static const unsigned int MAX_HEADERS_RESULTS = 160;
/** Size of the "block download window": how far ahead of our current height do we fetch?
 *  Larger windows tolerate larger download speed differences between peer, but increase the potential
 *  degree of disordering of blocks on disk (which make reindexing and in the future perhaps pruning
 *  harder). We'll probably want to make this a per-peer adaptive value at some point. */
static const unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/** Time to wait (in seconds) between writing blocks/block index to disk. */
static const unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/** Time to wait (in seconds) between flushing chainstate to disk. */
static const unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/** Time to wait (in seconds) between writing wallet witness data to disk. */
static const unsigned int WITNESS_WRITE_INTERVAL = 10 * 60;
/** Number of updates between writing wallet witness data to disk. */
static const unsigned int WITNESS_WRITE_UPDATES = 10000;
/** Maximum length of reject messages. */
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Average delay between local address broadcasts in seconds. */
static const unsigned int AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL = 24 * 24 * 60;
/** Average delay between peer address broadcasts in seconds. */
static const unsigned int AVG_ADDRESS_BROADCAST_INTERVAL = 30;
/** Average delay between trickled inventory transmissions in seconds.
 *  Blocks and whitelisted receivers bypass this, outbound peers get half this delay. */
static const unsigned int INVENTORY_BROADCAST_INTERVAL = 5;
/** Maximum number of inventory items to send per transmission.
 *  Limits the impact of low-fee transaction floods. */
static const unsigned int INVENTORY_BROADCAST_MAX = 7 * INVENTORY_BROADCAST_INTERVAL;

static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;

/** Default for -permitbaremultisig */
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_IBD_SKIP_TX_VERIFICATION = false;
static const bool DEFAULT_TXINDEX = false;
static const unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;

/** Default for -nurejectoldversions */
static const bool DEFAULT_NU_REJECT_OLD_VERSIONS = true;

#define equihash_parameters_acceptable(N, K) \
    ((CBlockHeader::HEADER_SIZE + equihash_solution_size(N, K))*MAX_HEADERS_RESULTS < \
     MAX_PROTOCOL_MESSAGE_LENGTH-1000)

static const bool DEFAULT_PEERBLOOMFILTERS = true;
static const bool DEFAULT_ENFORCENODEBLOOM = false;

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetCheapHash(); }
};

extern std::optional<unsigned int> expiryDeltaArg;
extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern CTxMemPool mempool;
typedef boost::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern BlockMap mapBlockIndex;
extern std::optional<uint64_t> last_block_num_txs;
extern std::optional<uint64_t> last_block_size;
extern const std::string strMessageMagic;

//! These four variables are used to notify getblocktemplate RPC of new tips.
//! When UpdateTip() establishes a new tip (best block), it must awaken a
//! waiting getblocktemplate RPC (if there is one) immediately. But upon waking
//! up, getblocktemplate cannot call chainActive->Tip() because it does not
//! (and cannot) hold cs_main. So the g_best_block_height and g_best_block variables
//! (protected by g_best_block_mutex) provide the needed height and block
//! hash respectively to getblocktemplate without it requiring cs_main.
extern Mutex g_best_block_mutex;
extern std::condition_variable g_best_block_cv;
extern int g_best_block_height;
extern uint256 g_best_block;

extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
extern int nScriptCheckThreads;
extern bool fTxIndex;

// The following flags enable specific indices (DB tables), but are not exposed as
// separate command-line options; instead they are enabled by experimental feature "-insightexplorer"

// Maintain a full address index, used to query for the balance, txids and unspent outputs for addresses
extern bool fAddressIndex;

// Maintain a full spent index, used to query the spending txid and input index for an outpoint
extern bool fSpentIndex;

// Maintain a full timestamp index, used to query for blocks within a time range
extern bool fTimestampIndex;

// END insightexplorer

extern bool fIsBareMultisigStd;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
extern bool fIBDSkipTxVerification;
// TODO: remove this flag by structuring our code such that
// it is unneeded for testing
extern bool fCoinbaseEnforcedShieldingEnabled;
extern size_t nCoinCacheUsage;
/** Transactions must have at least this fee rate (in zatoshis per 1000 bytes) for relaying, mining and transaction creation. */
extern CFeeRate minRelayTxFee;
/** Absolute maximum transaction fee (in zatoshis) used by wallet and mempool (rejects high fee in sendrawtransaction). */
extern CAmount maxTxFee;
/** Limit on the number of unpaid actions a transaction can have to be accepted to the mempool. */
extern CAmount nTxUnpaidActionLimit;
/** If the tip is older than this (in seconds), the node is considered to be in initial block download. */
extern int64_t nMaxTipAge;

/** Best header we've seen so far (used for getheaders queries' starting points). */
extern CBlockIndex *pindexBestHeader;

/** Minimum disk space required - used in CheckDiskSpace() */
static const uint64_t nMinDiskSpace = 52428800;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of chainActive.Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;

static const signed int DEFAULT_CHECKBLOCKS = MIN_BLOCKS_TO_KEEP;
static const unsigned int DEFAULT_CHECKLEVEL = 3;

/** Prefer to create v4 transactions. */
static const int32_t DEFAULT_PREFERRED_TX_VERSION = ZIP225_TX_VERSION;
static const std::set<int32_t> SUPPORTED_TX_VERSIONS = { SAPLING_TX_VERSION, ZIP225_TX_VERSION };
extern int32_t nPreferredTxVersion;

// Require that user allocate at least 550MB for block & undo files (blk???.dat and rev???.dat)
// At 1MB per block, 288 blocks = 288MB.
// Add 15% for Undo data = 331MB
// Add 20% for Orphan block rate = 397MB
// We want the low water mark after pruning to be at least 397 MB and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128MB block file + added 15% undo data = 147MB greater for a total of 545MB
// Setting the target to > than 550MB will make it likely we can respect the target.
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

/** Register with a network node to receive its signals */
void RegisterNodeSignals(CNodeSignals& nodeSignals);
/** Unregister a network node */
void UnregisterNodeSignals(CNodeSignals& nodeSignals);

/**
 * Process an incoming block. This only returns after the best known valid
 * block is made active. Note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 *
 * @param[out]  state   This may be set to an Error state if any error occurred processing it, including during validation/connection/etc of otherwise unrelated blocks during reorganisation; or it may be set to an Invalid state if pblock is itself invalid (but this is not guaranteed even when the block is checked). If you want to *possibly* get feedback on whether pblock is valid, you must also install a CValidationInterface (see validationinterface.h) - this will have its BlockChecked method called whenever *any* block completes validation.
 * @param[in]   pfrom   The node which we are receiving the block from; it is added to mapBlockSource and may be penalised if the block is invalid.
 * @param[in]   pblock  The block we want to process.
 * @param[in]   fForceProcessing Process this block even if unrequested; used for non-network block sources and whitelisted peers.
 * @param[out]  dbp     The already known disk position of pblock, or NULL if not yet stored.
 * @return True if state.IsValid()
 */
bool ProcessNewBlock(CValidationState& state, const CChainParams& chainparams, const CNode* pfrom, const CBlock* pblock, bool fForceProcessing, const CDiskBlockPos* dbp);
/** Check whether enough disk space is available for an incoming block */
bool CheckDiskSpace(uint64_t nAdditionalBytes = 0);
/** Open a block file (blk?????.dat) */
FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/** Open an undo file (rev?????.dat) */
FILE* OpenUndoFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/** Translation to a filesystem path */
fs::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix);
/** Import blocks from an external file */
bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp = NULL);
/** Initialize a new block tree database + block data on disk */
bool InitBlockIndex(const CChainParams& chainparams);
/** Load the block tree and coins database from disk */
bool LoadBlockIndex();
/** Unload database information */
void UnloadBlockIndex();
/** Process protocol messages received from a given node */
bool ProcessMessages(const CChainParams& chainparams, CNode* pfrom);
/**
 * Send queued protocol messages to be sent to a give node.
 *
 * @param[in]   params          Active chain parameters.
 * @param[in]   pto             The node which we are sending messages to.
 */
bool SendMessages(const Consensus::Params& params, CNode* pto);
/** Run an instance of the script checking thread */
void ThreadScriptCheck();
/** Check whether we are doing an initial block download (synchronizing from disk or network) */
bool IsInitialBlockDownload(const Consensus::Params& params);
/** testing-only, set or reset initial block down (IBD) state, return previous */
bool TestSetIBD(bool);
/** Format a string that describes several potential problems detected by the core */
std::pair<std::string, int64_t> GetWarnings(const std::string& strFor);
/** Retrieve a transaction (from memory pool, or from disk, if possible) */
bool GetTransaction(const uint256& hash, CTransaction& tx, const Consensus::Params& params, uint256& hashBlock, bool fAllowSlow = false, const CBlockIndex* blockIndex = nullptr);
/** Find the best known block, and make it the tip of the block chain */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, const CBlock* pblock = NULL);
CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

/**
 * Prune block and undo files (blk???.dat and undo???.dat) so that the disk space used is less than a user-defined target.
 * The user sets the target (in MB) on the command line or in config file.  This will be run on startup and whenever new
 * space is allocated in a block or undo file, staying below the target. Changing back to unpruned requires a reindex
 * (which in this case means the blockchain must be re-downloaded.)
 *
 * Pruning functions are called from FlushStateToDisk when the global fCheckForPruning flag has been set.
 * Block and undo files are deleted in lock-step (when blk00003.dat is deleted, so is rev00003.dat.)
 * Pruning cannot take place until the longest chain is at least a certain length (100000 on mainnet, 1000 on testnet, 10 on regtest).
 * Pruning will never delete a block within a defined distance (currently 288) from the active chain's tip.
 * The block index is updated by unsetting HAVE_DATA and HAVE_UNDO for any blocks that were stored in the deleted files.
 * A db flag records the fact that at least some block files have been pruned.
 *
 * @param[out]   setFilesToPrune   The set of file indices that can be unlinked will be returned
 */
void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight);

/**
 *  Actually unlink the specified files
 */
void UnlinkPrunedFiles(std::set<int>& setFilesToPrune);

/** Create a new block index entry for a given block hash */
CBlockIndex * InsertBlockIndex(const uint256& hash);
/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch);
/** Flush all state, indexes and buffers to disk. */
void FlushStateToDisk();
/** Prune block files and flush state to disk. */
void PruneAndFlush();

/** (try to) add transaction to memory pool **/
bool AcceptToMemoryPool(
        const CChainParams& chainparams,
        CTxMemPool& pool, CValidationState &state, const CTransaction &tx, bool fLimitFree,
        bool* pfMissingInputs, bool fRejectAbsurdFee=false);

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state);

struct CNodeStateStats {
    int nMisbehavior;
    int nSyncHeight;
    int nCommonHeight;
    std::vector<int> vHeightInFlight;
};


/**
 * Count ECDSA signature operations the old-fashioned (pre-0.6) way
 * @return number of sigops this transaction's outputs will produce when spent
 * @see CTransaction::FetchInputs
 */
unsigned int GetLegacySigOpCount(const CTransaction& tx);

/**
 * Count ECDSA signature operations in pay-to-script-hash inputs.
 *
 * @param[in] mapInputs Map of previous transactions that have outputs we're spending
 * @return maximum number of sigops required to validate this transaction's inputs
 * @see CTransaction::FetchInputs
 */
unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& mapInputs);


/**
 * Check whether all inputs of this transaction are valid (no double spends, scripts & sigs, amounts)
 * This does not modify the UTXO set. If pvChecks is not NULL, script checks are pushed onto it
 * instead of being performed inline.
 */
bool ContextualCheckInputs(const CTransaction& tx, CValidationState &state, const CCoinsViewCache &view, bool fScriptChecks,
                           unsigned int flags, bool cacheStore, PrecomputedTransactionData& txdata,
                           const Consensus::Params& consensusParams, uint32_t consensusBranchId,
                           std::vector<CScriptCheck> *pvChecks = NULL);

/**
 * Check whether all shielded inputs of this transaction are valid.
 *
 * This checks that:
 * - The anchors in the transaction exist in the given view.
 * - The nullifiers in the transaction do not exist in the given view.
 * - The signatures for the transaction's shielded components are valid.
 *
 * This also currently checks the Sapling proofs, due to the way the Rust verification
 * code is written. Sprout and Orchard proofs are currently checked in CheckTransaction().
 * Once we have batch proof validation implemented, these will all be accumulated in
 * CheckTransaction().
 *
 * To skip checking signatures, use `Consensus::CheckTxShieldedInputs` instead.
 *
 * This does not modify the view to add the nullifiers to the spent set.
 *
 * The `isInitBlockDownload` argument is a function parameter to assist with testing.
 */
bool ContextualCheckShieldedInputs(
        const CTransaction& tx,
        const PrecomputedTransactionData& txdata,
        CValidationState &state,
        const CCoinsViewCache &view,
        std::optional<rust::Box<sapling::BatchValidator>>& saplingAuth,
        std::optional<rust::Box<orchard::BatchValidator>>& orchardAuth,
        const Consensus::Params& consensus,
        uint32_t consensusBranchId,
        bool nu5Active,
        bool isMined,
        bool (*isInitBlockDownload)(const Consensus::Params&) = IsInitialBlockDownload);

/** Check a transaction contextually against a set of consensus rules */
bool ContextualCheckTransaction(const CTransaction& tx, CValidationState &state,
                                const CChainParams& chainparams, int nHeight, bool isMined,
                                bool (*isInitBlockDownload)(const Consensus::Params&) = IsInitialBlockDownload);

/** Apply the effects of this transaction on the UTXO set represented by view */
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight);

/** Transaction validation functions */

/** Context-independent validity checks */
bool CheckTransaction(const CTransaction& tx, CValidationState& state,
                      ProofVerifier& verifier);
bool CheckTransactionWithoutProofVerification(const CTransaction& tx, CValidationState &state);

namespace Consensus {

/**
 * Check whether all shielded inputs of this transaction are valid.
 *
 * This checks that:
 * - The anchors in the transaction exist in the given view.
 * - The nullifiers in the transaction do not exist in the given view.
 *
 * This does not modify the view to add the nullifiers to the spent set.
 * This does not check proofs or signatures.
 */
bool CheckTxShieldedInputs(
    const CTransaction& tx,
    CValidationState& state,
    const CCoinsViewCache& view,
    int dosLevel);

/**
 * Check whether all inputs of this transaction are valid (no double spends and amounts)
 * This does not modify the UTXO set. This does not check scripts and sigs.
 * Preconditions: tx.IsCoinBase() is false.
 */
bool CheckTxInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, int nSpendHeight, const Consensus::Params& consensusParams);

} // namespace Consensus

/**
 * Check if transaction is final and can be included in a block with the
 * specified height and time. Consensus critical.
 */
bool IsFinalTx(const CTransaction &tx, int nBlockHeight, int64_t nBlockTime);

/**
 * Check if transaction is expired and can be included in a block with the
 * specified height. Consensus critical.
 */
bool IsExpiredTx(const CTransaction &tx, int nBlockHeight);

/**
 * Check if transaction is expiring soon.  If yes, not propagating the transaction
 * can help DoS mitigation.  This is not consensus critical.
 */
bool IsExpiringSoonTx(const CTransaction &tx, int nNextBlockHeight);

/**
 * Check if transaction will be final in the next block to be created.
 *
 * Calls IsFinalTx() with current block height and appropriate block time.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckFinalTx(const CTransaction &tx, int flags = -1);

/**
 * Closure representing one script verification
 * Note that this stores references to the spending transaction
 */
class CScriptCheck
{
private:
    CScript scriptPubKey;
    CAmount amount;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    uint32_t consensusBranchId;
    ScriptError error;
    // We store a pointer instead of a reference here, to allow it to be null for
    // performance reasons (enabling fast swaps in CCheckQueue::Loop).
    PrecomputedTransactionData *txdata;

public:
    CScriptCheck(): amount(0), ptxTo(0), nIn(0), nFlags(0), cacheStore(false), consensusBranchId(0), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CCoins& txFromIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn, uint32_t consensusBranchIdIn, PrecomputedTransactionData* txdataIn) :
        scriptPubKey(txFromIn.vout[txToIn.vin[nInIn].prevout.n].scriptPubKey), amount(txFromIn.vout[txToIn.vin[nInIn].prevout.n].nValue),
        ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), consensusBranchId(consensusBranchIdIn), error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        scriptPubKey.swap(check.scriptPubKey);
        std::swap(ptxTo, check.ptxTo);
        std::swap(amount, check.amount);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(consensusBranchId, check.consensusBranchId);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

bool GetSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);
bool GetAddressIndex(const uint160& addressHash, int type,
        std::vector<CAddressIndexDbEntry> &addressIndex,
        int start = 0, int end = 0);
bool GetAddressUnspent(const uint160& addressHash, int type,
        std::vector<CAddressUnspentDbEntry>& unspentOutputs);
bool GetTimestampIndex(unsigned int high, unsigned int low, bool fActiveOnly,
    std::vector<std::pair<uint256, unsigned int> > &hashes);

/** Functions for disk access for blocks */
bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos, const CMessageHeader::MessageStartChars& messageStart);
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams);

/** Functions for validating blocks and updating the block tree */

/** Context-independent validity checks */

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state,
    const CChainParams& chainparams,
    bool fCheckPOW = true);

bool CheckBlock(const CBlock& block, CValidationState& state,
                const CChainParams& chainparams,
                ProofVerifier& verifier,
                bool fCheckPOW,
                bool fCheckMerkleRoot,
                bool fCheckTransactions);

/** Context-dependent validity checks.
 *  By "context", we mean only the previous block headers, but not the UTXO
 *  set; UTXO-related validity checks are done in ConnectBlock(). */
bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state,
                                const CChainParams& chainparams, CBlockIndex *pindexPrev);
bool ContextualCheckBlock(const CBlock& block, CValidationState& state,
                          const CChainParams& chainparams,
                          CBlockIndex *pindexPrev,
                          bool fCheckTransactions);

/**
 * How a given block should be checked.
 *
 * - `CheckAs::Block` applies all relevant block checks.
 * - `CheckAs::BlockTemplate` is the same as `CheckAs::Block` except that proofs
 *   and signatures are not validated, and the authDataRoot is not checked (as
 *   the coinbase transaction is not fully complete).
 * - `CheckAs::SlowBenchmark` is the same as `CheckAs::Block` except that the
 *   authDataRoot is not checked (as the required history tree state is not
 *   currently faked).
 */
enum class CheckAs {
    Block,
    BlockTemplate,
    SlowBenchmark,
};

/** Apply the effects of this block (with given index) on the UTXO set represented by coins.
 *  Validity checks that depend on the UTXO set are also done; ConnectBlock()
 *  can fail if those validity checks fail (among other reasons). */
bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& coins,
                  const CChainParams& chainparams,
                  bool fJustCheck = false, CheckAs blockChecks = CheckAs::Block);

/**
 * Check a block is completely valid from start to finish (only works on top
 * of our current best block, with cs_main held)
 */
bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fIsBlockTemplate);

/**
 * This will clear the subtree database for a given shielded type from the
 * current CCoinsView and regenerate the subtree database based on the current
 * active chain.
 *
 * Only supports Sapling and Orchard. This does nothing in the event the chain
 * is fresh or if the shielded protocol has not activated yet on chain.
 */
bool RegenerateSubtrees(ShieldedType type, const Consensus::Params& consensusParams);

/**
 * When there are blocks in the active chain with missing data (e.g. if the
 * activation height and branch ID of a particular upgrade have been altered),
 * rewind the chainstate and remove them from the block index.
 *
 * clearWitnessCaches is an output parameter that will be set to true iff
 * witness caches should be cleared in order to handle an intended long rewind.
 */
bool RewindBlockIndex(const CChainParams& chainparams, bool& clearWitnessCaches);

/** RAII wrapper for VerifyDB: Verify consistency of the block and coin databases */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth);
};

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator);

/** Mark a block as invalid. */
bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex *pindex);

/** Remove invalidity status from a block and its descendants. */
bool ReconsiderBlock(CValidationState& state, CBlockIndex *pindex);

/** The currently-connected chain of blocks (protected by cs_main). */
extern CChain chainActive;

/** Global variable that points to the active CCoinsView (protected by cs_main) */
extern CCoinsViewCache *pcoinsTip;

/** Global variable that points to the active block tree (protected by cs_main) */
extern CBlockTreeDB *pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache& inputs);

/** Reject codes greater or equal to this can be returned by AcceptToMemPool
 * for transactions, to signal internal conditions. They cannot and should not
 * be sent over the P2P network.
 */
static const unsigned int REJECT_INTERNAL = 0x100;
/** Too high fee. Can not be triggered by P2P transactions */
static const unsigned int REJECT_HIGHFEE = 0x100;
/** Transaction is already known (either in mempool or blockchain) */
static const unsigned int REJECT_ALREADY_KNOWN = 0x101;
/** Transaction conflicts with a transaction already known */
static const unsigned int REJECT_CONFLICT = 0x102;

uint64_t CalculateCurrentUsage();

/**
 * Return a CMutableTransaction with contextual default values based on set of consensus rules at nHeight. The expiryDelta will
 * either be based on the command-line argument '-txexpirydelta' or derived from consensusParams.
 */
CMutableTransaction CreateNewContextualCMutableTransaction(
    const Consensus::Params& consensusParams,
    int nHeight,
    bool requireV4);

std::pair<std::list<CTransaction>, std::optional<uint64_t>> TakeRecentlyConflicted(const CBlockIndex* pindex);
uint64_t GetChainConnectedSequence();
void SetChainNotifiedSequence(const CChainParams& chainparams, uint64_t recentlyConflictedSequence);
bool ChainIsFullyNotified(const CChainParams& chainparams);

#endif // BITCOIN_MAIN_H
