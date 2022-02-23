// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include "amount.h"
#include "asyncrpcoperation.h"
#include "coins.h"
#include "key.h"
#include "keystore.h"
#include "main.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "tinyformat.h"
#include "transaction_builder.h"
#include "ui_interface.h"
#include "util.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "script/ismine.h"
#include "wallet/crypter.h"
#include "wallet/orchard.h"
#include "wallet/walletdb.h"
#include "wallet/rpcwallet.h"
#include "zcash/address/unified.h"
#include "zcash/address/mnemonic.h"
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "base58.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/shared_ptr.hpp>

extern CWallet* pwalletMain;

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fSendFreeTransactions;
extern bool fPayAtLeastCustomFee;
extern unsigned int nOrchardAnchorConfirmations;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 100;
//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 0;
//! -mintxfee default
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000;
//! minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -sendfreetransactions
static const bool DEFAULT_SEND_FREE_TRANSACTIONS = false;
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 2;
//! Largest (in bytes) free transaction we're willing to create
static const unsigned int MAX_FREE_TRANSACTION_CREATE_SIZE = 1000;
static const bool DEFAULT_WALLETBROADCAST = true;
//! Size of witness cache
//  Should be large enough that we can expect not to reorg beyond our cache
//  unless there is some exceptional network disruption.
static const unsigned int WITNESS_CACHE_SIZE = MAX_REORG_LENGTH + 1;

//! Amount of entropy used in generation of the mnemonic seed, in bytes.
static const size_t WALLET_MNEMONIC_ENTROPY_LENGTH = 32;
//! -orchardanchorconfirmations default
static const unsigned int DEFAULT_ORCHARD_ANCHOR_CONFIRMATIONS = 10;

extern const char * DEFAULT_WALLET_DAT;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CWalletTx;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_LATEST = 60000
};


/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
    }
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData()
    {
        purpose = "unknown";
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

class RecipientMapping {
public:
    std::optional<libzcash::UnifiedAddress> ua;
    libzcash::RecipientAddress address;

    RecipientMapping(std::optional<libzcash::UnifiedAddress> ua_, libzcash::RecipientAddress address_) :
        ua(ua_), address(address_) {}
};

typedef std::map<std::string, std::string> mapValue_t;


static void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** A note outpoint */
class JSOutPoint
{
public:
    // Transaction hash
    uint256 hash;
    // Index into CTransaction.vJoinSplit
    uint64_t js;
    // Index into JSDescription fields of length ZC_NUM_JS_OUTPUTS
    uint8_t n;

    JSOutPoint() { SetNull(); }
    JSOutPoint(uint256 h, uint64_t js, uint8_t n) : hash {h}, js {js}, n {n} { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(hash);
        READWRITE(js);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); }
    bool IsNull() const { return hash.IsNull(); }

    friend bool operator<(const JSOutPoint& a, const JSOutPoint& b) {
        return (a.hash < b.hash ||
                (a.hash == b.hash && a.js < b.js) ||
                (a.hash == b.hash && a.js == b.js && a.n < b.n));
    }

    friend bool operator==(const JSOutPoint& a, const JSOutPoint& b) {
        return (a.hash == b.hash && a.js == b.js && a.n == b.n);
    }

    friend bool operator!=(const JSOutPoint& a, const JSOutPoint& b) {
        return !(a == b);
    }

    std::string ToString() const;
};

class SproutNoteData
{
public:
    libzcash::SproutPaymentAddress address;

    /**
     * Cached note nullifier. May not be set if the wallet was not unlocked when
     * this was SproutNoteData was created. If not set, we always assume that the
     * note has not been spent.
     *
     * It's okay to cache the nullifier in the wallet, because we are storing
     * the spending key there too, which could be used to derive this.
     * If the wallet is encrypted, this means that someone with access to the
     * locked wallet cannot spend notes, but can connect received notes to the
     * transactions they are spent in. This is the same security semantics as
     * for transparent addresses.
     */
    std::optional<uint256> nullifier;

    /**
     * Cached incremental witnesses for spendable Notes.
     * Beginning of the list is the most recent witness.
     */
    std::list<SproutWitness> witnesses;

    /**
     * Block height corresponding to the most current witness.
     *
     * When we first create a SproutNoteData in CWallet::FindMySproutNotes, this is set to
     * -1 as a placeholder. The next time CWallet::ChainTip is called, we can
     * determine what height the witness cache for this note is valid for (even
     * if no witnesses were cached), and so can set the correct value in
     * CWallet::IncrementNoteWitnesses and CWallet::DecrementNoteWitnesses.
     */
    int witnessHeight;

    SproutNoteData() : address(), nullifier(), witnessHeight {-1} { }
    SproutNoteData(libzcash::SproutPaymentAddress a) :
            address {a}, nullifier(), witnessHeight {-1} { }
    SproutNoteData(libzcash::SproutPaymentAddress a, uint256 n) :
            address {a}, nullifier {n}, witnessHeight {-1} { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(address);
        READWRITE(nullifier);
        READWRITE(witnesses);
        READWRITE(witnessHeight);
    }

    friend bool operator<(const SproutNoteData& a, const SproutNoteData& b) {
        return (a.address < b.address ||
                (a.address == b.address && a.nullifier < b.nullifier));
    }

    friend bool operator==(const SproutNoteData& a, const SproutNoteData& b) {
        return (a.address == b.address && a.nullifier == b.nullifier);
    }

    friend bool operator!=(const SproutNoteData& a, const SproutNoteData& b) {
        return !(a == b);
    }
};

class SaplingNoteData
{
public:
    /**
     * We initialize the height to -1 for the same reason as we do in SproutNoteData.
     * See the comment in that class for a full description.
     */
    SaplingNoteData() : witnessHeight {-1}, nullifier() { }
    SaplingNoteData(libzcash::SaplingIncomingViewingKey ivk) : ivk {ivk}, witnessHeight {-1}, nullifier() { }
    SaplingNoteData(libzcash::SaplingIncomingViewingKey ivk, uint256 n) : ivk {ivk}, witnessHeight {-1}, nullifier(n) { }

    std::list<SaplingWitness> witnesses;
    int witnessHeight;
    libzcash::SaplingIncomingViewingKey ivk;
    std::optional<uint256> nullifier;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH)) {
            READWRITE(nVersion);
        }
        READWRITE(ivk);
        READWRITE(nullifier);
        READWRITE(witnesses);
        READWRITE(witnessHeight);
    }

    friend bool operator==(const SaplingNoteData& a, const SaplingNoteData& b) {
        return (a.ivk == b.ivk && a.nullifier == b.nullifier && a.witnessHeight == b.witnessHeight);
    }

    friend bool operator!=(const SaplingNoteData& a, const SaplingNoteData& b) {
        return !(a == b);
    }
};

typedef std::map<JSOutPoint, SproutNoteData> mapSproutNoteData_t;
typedef std::map<SaplingOutPoint, SaplingNoteData> mapSaplingNoteData_t;

/** Sprout note, its location in a transaction, and number of confirmations. */
struct SproutNoteEntry
{
    JSOutPoint jsop;
    libzcash::SproutPaymentAddress address;
    libzcash::SproutNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    int confirmations;
};

/** Sapling note, its location in a transaction, and number of confirmations. */
struct SaplingNoteEntry
{
    SaplingOutPoint op;
    libzcash::SaplingPaymentAddress address;
    libzcash::SaplingNote note;
    std::array<unsigned char, ZC_MEMO_SIZE> memo;
    int confirmations;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx : public CTransaction
{
private:
    int GetDepthInMainChainINTERNAL(const CBlockIndex* &pindexRet) const;

public:
    uint256 hashBlock;
    std::vector<uint256> vMerkleBranch;
    int nIndex;

    // memory only
    mutable bool fMerkleVerified;


    CMerkleTx()
    {
        Init();
    }

    CMerkleTx(const CTransaction& txIn) : CTransaction(txIn)
    {
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
        fMerkleVerified = false;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(*(CTransaction*)this);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlock& block);


    /**
     * Return depth of transaction in blockchain:
     * -1  : not in blockchain, and not in memory pool (conflicted transaction)
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex* &pindexRet) const;
    int GetDepthInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet); }
    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChainINTERNAL(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    /** Pass this transaction to the mempool. Fails if absolute fee exceeds maxTxFee. */
    bool AcceptToMemoryPool(bool fLimitFree=true, bool fRejectAbsurdFee=true);
};

enum class WalletUAGenerationError {
    NoSuchAccount,
    ExistingAddressMismatch,
    WalletEncrypted
};

typedef std::variant<
    std::pair<libzcash::UnifiedAddress, libzcash::diversifier_index_t>,
    libzcash::UnifiedAddressGenerationError,
    WalletUAGenerationError> WalletUAGenerationResult;

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         and sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    mapSproutNoteData_t mapSproutNoteData;
    mapSaplingNoteData_t mapSaplingNoteData;
    OrchardWalletTxMeta orchardTxMeta;

    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    unsigned int nTimeSmart;
    char fFromMe;
    int64_t nOrderPos; //!< position in ordered transaction list

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(NULL);
    }

    CWalletTx(const CWallet* pwalletIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CMerkleTx& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    CWalletTx(const CWallet* pwalletIn, const CTransaction& txIn) : CMerkleTx(txIn)
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        mapSproutNoteData.clear();
        mapSaplingNoteData.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead())
            Init(NULL);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = "";

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(mapSproutNoteData);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (fOverwintered && nVersion >= SAPLING_TX_VERSION) {
            READWRITE(mapSaplingNoteData);
        }

        if (fOverwintered && nVersion >= ZIP225_TX_VERSION) {
            READWRITE(orchardTxMeta);
        }

        if (ser_action.ForRead())
        {
            ReadOrderPos(nOrderPos, mapValue);
            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    void SetSproutNoteData(const mapSproutNoteData_t& noteData);
    void SetSaplingNoteData(const mapSaplingNoteData_t& noteData);
    void SetOrchardTxMeta(OrchardWalletTxMeta actionData);

    std::pair<libzcash::SproutNotePlaintext, libzcash::SproutPaymentAddress> DecryptSproutNote(
        JSOutPoint jsop) const;
    std::optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> DecryptSaplingNote(const Consensus::Params& params, int height, SaplingOutPoint op) const;
    std::optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> DecryptSaplingNoteWithoutLeadByteCheck(SaplingOutPoint op) const;
    std::optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> RecoverSaplingNote(const Consensus::Params& params, int height,
            SaplingOutPoint op, std::set<uint256>& ovks) const;
    std::optional<std::pair<
        libzcash::SaplingNotePlaintext,
        libzcash::SaplingPaymentAddress>> RecoverSaplingNoteWithoutLeadByteCheck(SaplingOutPoint op, std::set<uint256>& ovks) const;

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true, const isminefilter& filter=ISMINE_SPENDABLE) const;
    CAmount GetImmatureWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetChange() const;

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const;

    bool IsTrusted() const;

    int64_t GetTxTime() const;
    int GetRequestCount() const;

    bool RelayWalletTransaction();

    std::set<uint256> GetConflicts() const;
};

class NoteFilter {
private:
    std::set<libzcash::SproutPaymentAddress> sproutAddresses;
    std::set<libzcash::SaplingPaymentAddress> saplingAddresses;
    std::set<libzcash::OrchardRawAddress> orchardAddresses;

    NoteFilter() {}
public:
    static NoteFilter Empty() { return NoteFilter(); }
    static NoteFilter ForPaymentAddresses(const std::vector<libzcash::PaymentAddress>& addrs);

    const std::set<libzcash::SproutPaymentAddress>& GetSproutAddresses() const {
        return sproutAddresses;
    }

    const std::set<libzcash::SaplingPaymentAddress>& GetSaplingAddresses() const {
        return saplingAddresses;
    }

    const std::set<libzcash::OrchardRawAddress>& GetOrchardAddresses() const {
        return orchardAddresses;
    }

    bool IsEmpty() const {
        return
            sproutAddresses.empty() &&
            saplingAddresses.empty() &&
            orchardAddresses.empty();
    }

    bool HasSproutAddress(libzcash::SproutPaymentAddress addr) const {
        return sproutAddresses.count(addr) > 0;
    }

    bool HasSaplingAddress(libzcash::SaplingPaymentAddress addr) const {
        return saplingAddresses.count(addr) > 0;
    }

    bool HasOrchardAddress(libzcash::OrchardRawAddress addr) const {
        return orchardAddresses.count(addr) > 0;
    }
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;
    bool fSpendable;
    bool fIsCoinbase;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fIsCoinbaseIn = false) :
            tx(txIn), i(iIn), nDepth(nDepthIn), fSpendable(fSpendableIn), fIsCoinbase(fIsCoinbaseIn){ }

    CAmount Value() const { return tx->vout[i].nValue; }
    std::string ToString() const;
};

/**
 * A class representing the ZIP 316 unified spending authority associated with
 * a ZIP 32 account and this wallet's mnemonic seed. This is intended to be
 * used as a ZTXOPattern value to choose prior Zcash transaction outputs,
 * including both transparent UTXOs and shielded notes.
 *
 * If the account ID is set to `ZCASH_LEGACY_ACCOUNT`, the instance instead
 * represents the collective spend authorities of all legacy transparent addresses
 * generated via the `getnewaddress` RPC method. Shielded notes will never be
 * selected for this account ID.
 *
 * If the set of receiver types provided is non-empty, only outputs for
 * protocols corresponding to the provided set of receiver types may be used.
 * If the set of receiver types is empty, no restrictions are placed upon what
 * protocols outputs are selected for.
 */
class AccountZTXOPattern {
    libzcash::AccountId accountId;
    std::set<libzcash::ReceiverType> receiverTypes;
public:
    AccountZTXOPattern(libzcash::AccountId accountIdIn, std::set<libzcash::ReceiverType> receiverTypesIn):
        accountId(accountIdIn), receiverTypes(receiverTypesIn) {}

    libzcash::AccountId GetAccountId() const {
        return accountId;
    }

    const std::set<libzcash::ReceiverType>& GetReceiverTypes() const {
        return receiverTypes;
    }

    bool IncludesP2PKH() const {
        return receiverTypes.empty() || receiverTypes.count(libzcash::ReceiverType::P2PKH) > 0;
    }

    bool IncludesP2SH() const {
        return receiverTypes.empty() || receiverTypes.count(libzcash::ReceiverType::P2SH) > 0;
    }

    bool IncludesSapling() const {
        return receiverTypes.empty() || receiverTypes.count(libzcash::ReceiverType::Sapling) > 0;
    }

    friend bool operator==(const AccountZTXOPattern &a, const AccountZTXOPattern &b) {
        return a.accountId == b.accountId && a.receiverTypes == b.receiverTypes;
    }
};

/**
 * A selector which can be used to choose prior Zcash transaction outputs,
 * including both transparent UTXOs and shielded notes.
 */
typedef std::variant<
    CKeyID,
    CScriptID,
    libzcash::SproutPaymentAddress,
    libzcash::SproutViewingKey,
    libzcash::SaplingPaymentAddress,
    libzcash::SaplingExtendedFullViewingKey,
    libzcash::UnifiedFullViewingKey,
    AccountZTXOPattern> ZTXOPattern;

class ZTXOSelector {
private:
    ZTXOPattern pattern;
    bool requireSpendingKeys;

    ZTXOSelector(ZTXOPattern patternIn, bool requireSpendingKeysIn):
        pattern(patternIn), requireSpendingKeys(requireSpendingKeysIn) {}

    friend class CWallet;
public:
    const ZTXOPattern& GetPattern() const {
        return pattern;
    }

    bool RequireSpendingKeys() const {
        return requireSpendingKeys;
    }

    bool SelectsTransparent() const;
    bool SelectsSprout() const;
    bool SelectsSapling() const;
};

class SpendableInputs {
public:
    std::vector<COutput> utxos;
    std::vector<SproutNoteEntry> sproutNoteEntries;
    std::vector<SaplingNoteEntry> saplingNoteEntries;
    std::vector<OrchardNoteMetadata> orchardNoteMetadata;

    /**
     * Selectively discard notes that are not required to obtain the desired
     * amount. Returns `false` if the available inputs do not add up to the
     * desired amount.
     */
    bool LimitToAmount(CAmount amount, CAmount dustThreshold);

    /**
     * Compute the total ZEC amount of spendable inputs.
     */
    CAmount Total() const {
        CAmount result = 0;
        for (const auto& t : utxos) {
            result += t.Value();
        }
        for (const auto& t : sproutNoteEntries) {
            result += t.note.value();
        }
        for (const auto& t : saplingNoteEntries) {
            result += t.note.value();
        }
        for (const auto& t : orchardNoteMetadata) {
            result += t.GetNoteValue();
        }
        return result;
    }

    /**
     * Return whether or not the set of selected UTXOs contains
     * coinbase outputs.
     */
    bool HasTransparentCoinbase() const;

    /**
     * List spendable inputs in zrpcunsafe log entries.
     */
    void LogInputs(const AsyncRPCOperationId& id) const;
};

/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

class UFVKAddressMetadata
{
private:
    // The account ID may be absent for imported UFVKs, and also may temporarily
    // be absent when this data structure is in a partially-reconstructed state
    // during the wallet load process.
    std::optional<libzcash::AccountId> accountId;
    std::map<libzcash::diversifier_index_t, std::set<libzcash::ReceiverType>> addressReceivers;
public:
    UFVKAddressMetadata() {}
    UFVKAddressMetadata(libzcash::AccountId accountId): accountId(accountId) {}

    /**
     * Return all currently known diversifier indices for which addresses
     * have been generated, each accompanied by the associated set of receiver
     * types that were used when generating that address.
     */
    const std::map<libzcash::diversifier_index_t, std::set<libzcash::ReceiverType>>& GetKnownReceiverSetsByDiversifierIndex() const {
        return addressReceivers;
    }

    std::optional<std::set<libzcash::ReceiverType>> GetReceivers(
            const libzcash::diversifier_index_t& j) const {
        auto receivers = addressReceivers.find(j);
        if (receivers != addressReceivers.end()) {
            return receivers->second;
        } else {
            return std::nullopt;
        }
    }

    /**
     * Add the specified set of receivers at the provided diversifier index.
     *
     * Returns `true` if this is a new entry or if the operation would not
     * alter the existing set of receiver types at this index, `false`
     * otherwise.
     */
    bool SetReceivers(
            const libzcash::diversifier_index_t& j,
            const std::set<libzcash::ReceiverType>& receivers) {
        const auto [it, success] = addressReceivers.insert(std::make_pair(j, receivers));
        if (success) {
            return true;
        } else {
            return it->second == receivers;
        }
    }

    std::optional<libzcash::AccountId> GetAccountId() const {
        return accountId;
    }

    bool SetAccountId(libzcash::AccountId accountIdIn) {
        if (accountId.has_value()) {
            return (accountIdIn == accountId.value());
        } else {
            accountId = accountIdIn;
            return true;
        }
    }

    /**
     * Search the for the maximum diversifier that has already been used to
     * generate a new address, and return the next diversifier. Returns the
     * zero diversifier index if no addresses have yet been generated,
     * and returns std::nullopt if the increment operation would cause an
     * overflow.
     */
    std::optional<libzcash::diversifier_index_t> GetNextDiversifierIndex() {
        if (addressReceivers.empty()) {
            return libzcash::diversifier_index_t(0);
        } else {
            return addressReceivers.rbegin()->first.succ();
        }
    }
};

/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet : public CCryptoKeyStore, public CValidationInterface
{
private:
    /**
     * Select a set of coins such that nValueRet >= nTargetValue and at least
     * all coins from coinControl are selected; Never select unconfirmed coins
     * if they are not ours
     */
    bool SelectCoins(const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet, bool& fOnlyCoinbaseCoinsRet, bool& fNeedCoinbaseCoinsRet, const CCoinControl *coinControl = NULL) const;

    CWalletDB *pwalletdbEncryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;
    int64_t nLastSetChain;
    int nSetChainUpdates;
    bool fBroadcastTransactions;

    /**
     * A map from a protocol-specific transaction output identifier to
     * a txid.
     */
    template <class T>
    using TxSpendMap = std::multimap<T, uint256>;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef TxSpendMap<COutPoint> TxSpends;
    TxSpends mapTxSpends;

    /**
     * Used to keep track of spent Notes, and
     * detect and report conflicts (double-spends).
     */
    typedef TxSpendMap<uint256> TxNullifiers;
    TxNullifiers mapTxSproutNullifiers;
    TxNullifiers mapTxSaplingNullifiers;

    std::vector<CTransaction> pendingSaplingMigrationTxs;
    AsyncRPCOperationId saplingMigrationOperationId;

    void AddToTransparentSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSproutSpends(const uint256& nullifier, const uint256& wtxid);
    void AddToSaplingSpends(const uint256& nullifier, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

public:
    /*
     * Size of the incremental witness cache for the notes in our wallet.
     * This will always be greater than or equal to the size of the largest
     * incremental witness cache in any transaction in mapWallet.
     */
    int64_t nWitnessCacheSize;
    bool fSaplingMigrationEnabled = false;

    void ClearNoteWitnessCache();

protected:
    /**
     * pindex is the new tip being connected.
     */
    void IncrementNoteWitnesses(
            const Consensus::Params& consensus,
            const CBlockIndex* pindex,
            const CBlock* pblock,
            SproutMerkleTree& sproutTree,
            SaplingMerkleTree& saplingTree,
            bool performOrchardWalletUpdates
            );
    /**
     * pindex is the old tip being disconnected.
     */
    void DecrementNoteWitnesses(
            const Consensus::Params& consensus,
            const CBlockIndex* pindex
            );

    template <typename WalletDB>
    void SetBestChainINTERNAL(WalletDB& walletdb, const CBlockLocator& loc) {
        if (!walletdb.TxnBegin()) {
            // This needs to be done atomically, so don't do it at all
            LogPrintf("SetBestChain(): Couldn't start atomic write\n");
            return;
        }
        try {
            LOCK(cs_wallet);
            for (std::pair<const uint256, CWalletTx>& wtxItem : mapWallet) {
                auto wtx = wtxItem.second;
                // We skip transactions for which mapSproutNoteData and mapSaplingNoteData
                // are empty. This covers transactions that have no Sprout or Sapling data
                // (i.e. are purely transparent), as well as shielding and unshielding
                // transactions in which we only have transparent addresses involved.
                if (!(wtx.mapSproutNoteData.empty() && wtx.mapSaplingNoteData.empty())) {
                    if (!walletdb.WriteTx(wtx)) {
                        LogPrintf("SetBestChain(): Failed to write CWalletTx, aborting atomic write\n");
                        walletdb.TxnAbort();
                        return;
                    }
                }
            }
            // Add persistence of Orchard incremental witness tree
            orchardWallet.GarbageCollect();
            if (!walletdb.WriteOrchardWitnesses(orchardWallet)) {
                LogPrintf("SetBestChain(): Failed to write Orchard witnesses, aborting atomic write\n");
                walletdb.TxnAbort();
                return;
            }
            if (!walletdb.WriteWitnessCacheSize(nWitnessCacheSize)) {
                LogPrintf("SetBestChain(): Failed to write nWitnessCacheSize, aborting atomic write\n");
                walletdb.TxnAbort();
                return;
            }
            if (!walletdb.WriteBestBlock(loc)) {
                LogPrintf("SetBestChain(): Failed to write best block, aborting atomic write\n");
                walletdb.TxnAbort();
                return;
            }
        } catch (const std::exception &exc) {
            // Unexpected failure
            LogPrintf("SetBestChain(): Unexpected error during atomic write:\n");
            LogPrintf("%s\n", exc.what());
            walletdb.TxnAbort();
            return;
        }
        if (!walletdb.TxnCommit()) {
            // Couldn't commit all to db, but in-memory state is fine
            LogPrintf("SetBestChain(): Couldn't commit atomic write\n");
            return;
        }
    }

private:
    template <class T>
    void SyncMetaData(std::pair<typename TxSpendMap<T>::iterator, typename TxSpendMap<T>::iterator>);
    void ChainTipAdded(
            const CBlockIndex *pindex,
            const CBlock *pblock,
            SproutMerkleTree sproutTree,
            SaplingMerkleTree saplingTree,
            bool performOrchardWalletUpdates);

    /* Add a transparent secret key to the wallet. Internal use only. */
    CPubKey AddTransparentSecretKey(
            const uint256& seedFingerprint,
            const CKey& secret,
            const HDKeyPath& keyPath);

    std::map<libzcash::OrchardIncomingViewingKey, CKeyMetadata> mapOrchardZKeyMetadata;

protected:
    bool UpdatedNoteData(const CWalletTx& wtxIn, CWalletTx& wtx);
    void MarkAffectedTransactionsDirty(const CTransaction& tx);

    /* the hd chain metadata for keys derived from the mnemonic seed */
    std::optional<CHDChain> mnemonicHDChain;

    /* the network ID string for the network for which this wallet was created */
    std::string networkIdString;

    /* The Orchard subset of wallet data. As many operations as possible are
     * delegated to the Orchard wallet.
     */
    OrchardWallet orchardWallet;

public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    std::string strWalletFile;

    std::set<int64_t> setKeyPool;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    std::map<libzcash::SproutPaymentAddress, CKeyMetadata> mapSproutZKeyMetadata;
    std::map<libzcash::SaplingIncomingViewingKey, CKeyMetadata> mapSaplingZKeyMetadata;
    std::map<std::pair<libzcash::SeedFingerprint, libzcash::AccountId>, libzcash::UFVKId> mapUnifiedAccountKeys;
    std::map<libzcash::UFVKId, UFVKAddressMetadata> mapUfvkAddressMetadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    CWallet(const CChainParams& params)
    {
        SetNull(params);
    }

    CWallet(const CChainParams& params, const std::string& strWalletFileIn)
    {
        SetNull(params);

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    ~CWallet()
    {
        delete pwalletdbEncryption;
        pwalletdbEncryption = NULL;
    }

    void SetNull(const CChainParams& params)
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nLastSetChain = 0;
        nSetChainUpdates = 0;
        nTimeFirstKey = 0;
        fBroadcastTransactions = false;
        nWitnessCacheSize = 0;
        networkIdString = params.NetworkIDString();
    }

    /**
     * The reverse mapping of nullifiers to notes.
     *
     * The mapping cannot be updated while an encrypted wallet is locked,
     * because we need the SpendingKey to create the nullifier (#1502). This has
     * several implications for transactions added to the wallet while locked:
     *
     * - Parent transactions can't be marked dirty when a child transaction that
     *   spends their output notes is updated.
     *
     *   - We currently don't cache any note values, so this is not a problem,
     *     yet.
     *
     * - GetFilteredNotes can't filter out spent notes.
     *
     *   - Per the comment in SproutNoteData, we assume that if we don't have a
     *     cached nullifier, the note is not spent.
     *
     * Another more problematic implication is that the wallet can fail to
     * detect transactions on the blockchain that spend our notes. There are two
     * possible cases in which this could happen:
     *
     * - We receive a note when the wallet is locked, and then spend it using a
     *   different wallet client.
     *
     * - We spend from a PaymentAddress we control, then we export the
     *   SpendingKey and import it into a new wallet, and reindex/rescan to find
     *   the old transactions.
     *
     * The wallet will only miss "pure" spends - transactions that are only
     * linked to us by the fact that they contain notes we spent. If it also
     * sends notes to us, or interacts with our transparent addresses, we will
     * detect the transaction and add it to the wallet (again without caching
     * nullifiers for new notes). As by default JoinSplits send change back to
     * the origin PaymentAddress, the wallet should rarely miss transactions.
     *
     * To work around these issues, whenever the wallet is unlocked, we scan all
     * cached notes, and cache any missing nullifiers. Since the wallet must be
     * unlocked in order to spend notes, this means that GetFilteredNotes will
     * always behave correctly within that context (and any other uses will give
     * correct responses afterwards), for the transactions that the wallet was
     * able to detect. Any missing transactions can be rediscovered by:
     *
     * - Unlocking the wallet (to fill all nullifier caches).
     *
     * - Restarting the node with -reindex (which operates on a locked wallet
     *   but with the now-cached nullifiers).
     */
    std::map<uint256, JSOutPoint> mapSproutNullifiersToNotes;

    std::map<uint256, SaplingOutPoint> mapSaplingNullifiersToNotes;

    std::map<uint256, CWalletTx> mapWallet;

    typedef std::multimap<int64_t, CWalletTx*> TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    CPubKey vchDefaultKey;

    std::set<COutPoint> setLockedCoins;
    std::set<JSOutPoint> setLockedSproutNotes;
    std::set<SaplingOutPoint> setLockedSaplingNotes;

    int64_t nTimeFirstKey;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(std::vector<COutput>& vCoins,
                        bool fOnlyConfirmed=true,
                        const CCoinControl *coinControl = NULL,
                        bool fIncludeZeroValue=false,
                        bool fIncludeCoinBase=true,
                        bool fOnlySpendable=false,
                        int nMinDepth = 0,
                        std::set<CTxDestination>* onlyFilterByDests = nullptr) const;

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled
     */
    static bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, CAmount& nValueRet);

    /**
     * Returns the ZTXO selector for the specified account ID.
     *
     * Returns `std::nullopt` if the account ID has not been generated yet by
     * the wallet.
     *
     * If the `requireSpendingKey` flag is set, this will only return a selector
     * that will choose outputs for which this wallet holds the spending keys.
     */
    std::optional<ZTXOSelector> ZTXOSelectorForAccount(
            libzcash::AccountId account,
            bool requireSpendingKey,
            std::set<libzcash::ReceiverType> receiverTypes={}) const;

    /**
     * Returns the ZTXO selector for the specified payment address, if the
     * address is known to the wallet. If the `requireSpendingKey` flag is set,
     * this will only return a selector that will choose outputs for which this
     * wallet holds the spending keys.
     */
    std::optional<ZTXOSelector> ZTXOSelectorForAddress(
            const libzcash::PaymentAddress& addr,
            bool requireSpendingKey) const;

    /**
     * Returns the ZTXO selector for the specified viewing key, if that key
     * is known to the wallet. If the `requireSpendingKey` flag is set, this
     * will only return a selector that will choose outputs for which this
     * wallet holds the spending keys.
     */
    std::optional<ZTXOSelector> ZTXOSelectorForViewingKey(
            const libzcash::ViewingKey& vk,
            bool requireSpendingKey) const;

    /**
     * Returns the ZTXO selector that will select UTXOs sent to legacy
     * transparent addresses managed by this wallet.
     */
    static ZTXOSelector LegacyTransparentZTXOSelector(bool requireSpendingKey);

    /**
     * Look up the account for a given selector. This resolves the account ID
     * even in the in that a bare transparent or Sapling address that
     * corresponds to a non-legacy account is provided as a selector.  This is
     * used in z_sendmany to ensure that we always correctly determine change
     * addresses and OVKs on the basis of account UFVKs when possible.
     */
    std::optional<libzcash::AccountId> FindAccountForSelector(const ZTXOSelector& paymentSource) const;

    /**
     * Generate a change address for the specified account.
     *
     * If a shielded change address is requested, this will return the default
     * unified address for the internal unified full viewing key.
     *
     * If a transparent change address is requested, this will generate a fresh
     * diversified unified address from the internal unified full viewing key,
     * and return the associated transparent change address.
     *
     * Returns `std::nullopt` if the account does not have an internal spending
     * key matching the requested `ChangeType`.
     */
    std::optional<libzcash::RecipientAddress> GenerateChangeAddressForAccount(
            libzcash::AccountId accountId,
            std::set<libzcash::ChangeType> changeOptions);

    SpendableInputs FindSpendableInputs(
            ZTXOSelector paymentSource,
            bool allowTransparentCoinbase,
            uint32_t minDepth) const;

    bool SelectorMatchesAddress(const ZTXOSelector& source, const CTxDestination& a0) const;
    bool SelectorMatchesAddress(const ZTXOSelector& source, const libzcash::SproutPaymentAddress& a0) const;
    bool SelectorMatchesAddress(const ZTXOSelector& source, const libzcash::SaplingPaymentAddress& a0) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;
    bool IsSproutSpent(const uint256& nullifier) const;
    bool IsSaplingSpent(const uint256& nullifier) const;
    bool IsOrchardSpent(const OrchardOutPoint& outpoint) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    void LockCoin(COutPoint& output);
    void UnlockCoin(COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);

    bool IsLockedNote(const JSOutPoint& outpt) const;
    void LockNote(const JSOutPoint& output);
    void UnlockNote(const JSOutPoint& output);
    void UnlockAllSproutNotes();
    std::vector<JSOutPoint> ListLockedSproutNotes();

    bool IsLockedNote(const SaplingOutPoint& output) const;
    void LockNote(const SaplingOutPoint& output);
    void UnlockNote(const SaplingOutPoint& output);
    void UnlockAllSaplingNotes();
    std::vector<SaplingOutPoint> ListLockedSaplingNotes();

    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(bool external);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    void LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &metadata);

    bool LoadMinVersion(int nVersion) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript);
    bool LoadCScript(const CScript& redeemScript);

    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript &dest);
    bool RemoveWatchOnly(const CScript &dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const;

    /**
      * Sprout ZKeys
      */
    //! Generates a new Sprout zaddr
    libzcash::SproutPaymentAddress GenerateNewSproutZKey();
    //! Adds spending key to the store, and saves it to disk
    bool AddSproutZKey(const libzcash::SproutSpendingKey &key);
    //! Adds spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadZKey(const libzcash::SproutSpendingKey &key);
    //! Load spending key metadata (used by LoadWallet)
    void LoadZKeyMetadata(const libzcash::SproutPaymentAddress &addr, const CKeyMetadata &meta);
    //! Adds an encrypted spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedZKey(const libzcash::SproutPaymentAddress &addr, const libzcash::ReceivingKey &rk, const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds an encrypted spending key to the store, and saves it to disk (virtual method, declared in crypter.h)
    bool AddCryptedSproutSpendingKey(
        const libzcash::SproutPaymentAddress &address,
        const libzcash::ReceivingKey &rk,
        const std::vector<unsigned char> &vchCryptedSecret);

    //! Adds a Sprout viewing key to the store, and saves it to disk.
    bool AddSproutViewingKey(const libzcash::SproutViewingKey &vk);
    bool RemoveSproutViewingKey(const libzcash::SproutViewingKey &vk);
    //! Adds a Sprout viewing key to the store, without saving it to disk (used by LoadWallet)
    bool LoadSproutViewingKey(const libzcash::SproutViewingKey &dest);

    /**
      * Sapling ZKeys
      */

    //! Generates new Sapling key, stores the newly generated spending
    //! key to the wallet, and returns the default address for the newly generated key.
    libzcash::SaplingPaymentAddress GenerateNewLegacySaplingZKey();
    //! Generates Sapling key at the specified address index, and stores that
    //! key to the wallet if it has not already been persisted. Returns the
    //! default address for the key, and a flag that is true when the key
    //! was newly generated (not already in the wallet).
    std::pair<libzcash::SaplingPaymentAddress, bool> GenerateLegacySaplingZKey(uint32_t addrIndex);
    //! Adds Sapling spending key to the store, and saves it to disk
    bool AddSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key);
    //! Add Sapling full viewing key to the wallet.
    //!
    //! This overrides CBasicKeyStore::AddSaplingFullViewingKey to persist the
    //! full viewing key to disk. Inside CCryptoKeyStore and CBasicKeyStore,
    //! CBasicKeyStore::AddSaplingFullViewingKey is called directly when adding a
    //! full viewing key to the keystore, to avoid this override.
    bool AddSaplingFullViewingKey(
            const libzcash::SaplingExtendedFullViewingKey &extfvk);
    bool AddSaplingPaymentAddress(
        const libzcash::SaplingIncomingViewingKey &ivk,
        const libzcash::SaplingPaymentAddress &addr);
    bool AddCryptedSaplingSpendingKey(
        const libzcash::SaplingExtendedFullViewingKey &extfvk,
        const std::vector<unsigned char> &vchCryptedSecret);
    //! Adds spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadSaplingZKey(const libzcash::SaplingExtendedSpendingKey &key);
    //! Load spending key metadata (used by LoadWallet)
    void LoadSaplingZKeyMetadata(const libzcash::SaplingIncomingViewingKey &ivk, const CKeyMetadata &meta);
    //! Add Sapling full viewing key to the store, without saving it to disk (used by LoadWallet)
    bool LoadSaplingFullViewingKey(const libzcash::SaplingExtendedFullViewingKey &extfvk);
    //! Adds a Sapling payment address -> incoming viewing key map entry,
    //! without saving it to disk (used by LoadWallet)
    bool LoadSaplingPaymentAddress(
        const libzcash::SaplingPaymentAddress &addr,
        const libzcash::SaplingIncomingViewingKey &ivk);
    //! Adds an encrypted spending key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedSaplingZKey(const libzcash::SaplingExtendedFullViewingKey &extfvk,
                                const std::vector<unsigned char> &vchCryptedSecret);

    //
    // Orchard Support
    //

    bool AddOrchardZKey(const libzcash::OrchardSpendingKey &sk);
    bool AddOrchardFullViewingKey(const libzcash::OrchardFullViewingKey &fvk);
    /**
     * Adds an address/ivk mapping to the in-memory wallet. Returns `false` if
     * the mapping could not be persisted, or the IVK does not correspond to an
     * FVK known by the wallet.
     */
    bool AddOrchardRawAddress(
        const libzcash::OrchardIncomingViewingKey &ivk,
        const libzcash::OrchardRawAddress &addr);
    /**
     * Loads an address/ivk mapping to the in-memory wallet. Returns `true`
     * if the provided IVK corresponds to an FVK known by the wallet.
     */
    bool LoadOrchardRawAddress(
        const libzcash::OrchardRawAddress &addr,
        const libzcash::OrchardIncomingViewingKey &ivk);

    /**
     * Returns a loader that can be used to read an Orchard note commitment
     * tree from a stream into the Orchard wallet.
     */
    OrchardWalletNoteCommitmentTreeLoader GetOrchardNoteCommitmentTreeLoader();

    //
    // Unified keys, addresses, and accounts
    //

    //! Obtain the account key for the legacy account by deriving it from
    //! the wallet's mnemonic seed.
    libzcash::transparent::AccountKey GetLegacyAccountKey() const;

    //! Generate the unified spending key from the wallet's mnemonic seed
    //! for the next unused account identifier.
    std::pair<libzcash::UnifiedFullViewingKey, libzcash::AccountId>
        GenerateNewUnifiedSpendingKey();

    //! Generate the unified spending key for the specified ZIP-32/BIP-44
    //! account identifier from the wallet's mnemonic seed, or returns
    //! std::nullopt if the account identifier does not produce a valid
    //! spending key for all receiver types.
    std::optional<libzcash::ZcashdUnifiedSpendingKey>
        GenerateUnifiedSpendingKeyForAccount(libzcash::AccountId accountId);

    //! Retrieves the UFVK derived from the wallet's mnemonic seed for the specified account.
    std::optional<libzcash::ZcashdUnifiedFullViewingKey>
        GetUnifiedFullViewingKeyByAccount(libzcash::AccountId account) const;

    //! Generate a new unified address for the specified account, diversifier, and
    //! set of receiver types.
    //!
    //! If no diversifier index is provided, the next unused diversifier index
    //! will be selected.
    WalletUAGenerationResult GenerateUnifiedAddress(
        const libzcash::AccountId& accountId,
        const std::set<libzcash::ReceiverType>& receivers,
        std::optional<libzcash::diversifier_index_t> j = std::nullopt);

    bool AddUnifiedFullViewingKey(const libzcash::UnifiedFullViewingKey &ufvk);

    bool LoadUnifiedFullViewingKey(const libzcash::UnifiedFullViewingKey &ufvk);
    bool LoadUnifiedAccountMetadata(const ZcashdUnifiedAccountMetadata &skmeta);
    bool LoadUnifiedAddressMetadata(const ZcashdUnifiedAddressMetadata &addrmeta);

    //! Reconstructs (in memory) caches and mappings for unified accounts,
    //! addresses and keying material. This should be called once, after the
    //! remainder of the on-disk wallet data has been loaded.
    //!
    //! Returns true if and only if there were no detected inconsistencies or
    //! failures in reconstructing the cache.
    bool LoadCaches();

    std::optional<libzcash::UFVKId> FindUnifiedFullViewingKey(const libzcash::UnifiedAddress& addr) const;
    std::optional<libzcash::AccountId> GetUnifiedAccountId(const libzcash::UFVKId& ufvkId) const;

    std::optional<libzcash::ZcashdUnifiedFullViewingKey> FindUFVKByReceiver(const libzcash::Receiver& receiver) const;
    std::optional<libzcash::UnifiedAddress> FindUnifiedAddressByReceiver(const libzcash::Receiver& receiver) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB *pwalletdb = NULL);

    DBErrors ReorderTransactions();

    void MarkDirty();
    bool UpdateNullifierNoteMap();
    void UpdateNullifierNoteMapWithTx(const CWalletTx& wtx);
    void UpdateSaplingNullifierNoteMapWithTx(CWalletTx& wtx);
    void UpdateSaplingNullifierNoteMapForBlock(const CBlock* pblock);
    void LoadWalletTx(const CWalletTx& wtxIn);
    bool AddToWallet(const CWalletTx& wtxIn, CWalletDB* pwalletdb);
    void SyncTransaction(const CTransaction& tx, const CBlock* pblock, const int nHeight);
    bool AddToWalletIfInvolvingMe(
            const Consensus::Params& consensus,
            const CTransaction& tx,
            const CBlock* pblock,
            const int nHeight,
            bool fUpdate
            );
    void EraseFromWallet(const uint256 &hash);
    void WitnessNoteCommitment(
         std::vector<uint256> commitments,
         std::vector<std::optional<SproutWitness>>& witnesses,
         uint256 &final_anchor);
    int ScanForWalletTransactions(
        CBlockIndex* pindexStart,
        bool fUpdate,
        bool isInitScan);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime);
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime);
    CAmount GetBalance(const isminefilter& filter=ISMINE_SPENDABLE, const int min_depth=0) const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    CAmount GetLegacyBalance(const isminefilter& filter, int minDepth) const;

    /**
     * Insert additional inputs into the transaction by
     * calling CreateTransaction();
     */
    bool FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosRet, std::string& strFailReason, bool includeWatching);

    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     */
    bool CreateTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosRet,
                           std::string& strFailReason, const CCoinControl *coinControl = NULL, bool sign = true);

    /**
     * Save a set of (txid, RecipientAddress, std::optional<UnifiedAddress>) mappings to the wallet.
     * This information is persisted so that it's possible to correctly display the unified
     * address to which a payment was sent.
     */
    template <typename RecipientMapping>
    bool SaveRecipientMappings(const uint256& txid, const std::vector<RecipientMapping>& recipients)
    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("SaveRecipientMappings:\n%s", txid.ToString());

        for (const auto& recipient : recipients)
        {
            if (recipient.ua.has_value()) {
                CWalletDB(strWalletFile).WriteRecipientMapping(
                    txid,
                    recipient.address,
                    recipient.ua.value()
                );
            }
        }

        return true;
    }

    bool CommitTransaction(CWalletTx& wtxNew, std::optional<std::reference_wrapper<CReserveKey>> reservekey);

    static CFeeRate minTxFee;
    /**
     * Estimate the minimum fee considering user set parameters
     * and the required fee
     */
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);
    /**
     * Return the minimum required fee taking into account the
     * floating relay fee and user set minimum transaction fee
     */
    static CAmount GetRequiredFee(unsigned int nTxBytes);

private:
    bool NewKeyPool();
public:
    bool TopUpKeyPool(unsigned int kpSize = 0);
    void ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;

    std::set< std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    std::optional<uint256> GetSproutNoteNullifier(
        const JSDescription& jsdesc,
        const libzcash::SproutPaymentAddress& address,
        const ZCNoteDecryption& dec,
        const uint256& hSig,
        uint8_t n) const;
    mapSproutNoteData_t FindMySproutNotes(const CTransaction& tx) const;
    std::pair<mapSaplingNoteData_t, SaplingIncomingViewingKeyMap> FindMySaplingNotes(const CTransaction& tx, int height) const;
    bool IsSproutNullifierFromMe(const uint256& nullifier) const;
    bool IsSaplingNullifierFromMe(const uint256& nullifier) const;

    void GetSproutNoteWitnesses(
         const std::vector<JSOutPoint>& notes,
         std::vector<std::optional<SproutWitness>>& witnesses,
         uint256 &final_anchor);
    void GetSaplingNoteWitnesses(
         const std::vector<SaplingOutPoint>& notes,
         std::vector<std::optional<SaplingWitness>>& witnesses,
         uint256 &final_anchor);
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> GetOrchardSpendInfo(
        const std::vector<OrchardNoteMetadata>& orchardNoteMetadata) const;

    isminetype IsMine(const CTxIn& txin) const;
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void ChainTip(
        const CBlockIndex *pindex,
        const CBlock *pblock,
        std::optional<std::pair<SproutMerkleTree, SaplingMerkleTree>> added);
    void RunSaplingMigration(int blockHeight);
    void AddPendingSaplingMigrationTx(const CTransaction& tx);
    /** Saves witness caches and best block locator to disk. */
    void SetBestChain(const CBlockLocator& loc);

    std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> GetSproutNullifiers(
            const std::set<libzcash::SproutPaymentAddress>& addresses);
    bool IsNoteSproutChange(
            const std::set<std::pair<libzcash::SproutPaymentAddress, uint256>> & nullifierSet,
            const libzcash::SproutPaymentAddress& address,
            const JSOutPoint & entry);

    std::set<std::pair<libzcash::SaplingPaymentAddress, uint256>> GetSaplingNullifiers(
            const std::set<libzcash::SaplingPaymentAddress>& addresses);
    bool IsNoteSaplingChange(
            const std::set<std::pair<libzcash::SaplingPaymentAddress, uint256>> & nullifierSet,
            const libzcash::SaplingPaymentAddress& address,
            const SaplingOutPoint & entry);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);

    bool DelAddressBook(const CTxDestination& address);

    void UpdatedTransaction(const uint256 &hashTx);

    void Inventory(const uint256 &hash)
    {
        {
            LOCK(cs_wallet);
            std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
            if (mi != mapRequestCount.end())
                (*mi).second++;
        }
    }

    void GetAddressForMining(std::optional<MinerAddress> &minerAddress);
    void ResetRequestCount(const uint256 &hash)
    {
        LOCK(cs_wallet);
        mapRequestCount[hash] = 0;
    };

    unsigned int GetKeyPoolSize()
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setKeyPool.size();
    }

    bool SetDefaultKey(const CPubKey &vchPubKey);

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! Flush wallet (bitdb flush)
    void Flush(bool shutdown=false);

    //! Verify the wallet database and perform salvage if required
    static bool Verify();

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** Inquire whether this wallet broadcasts transactions. */
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /** Set whether this wallet broadcasts transactions. */
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /* Returns true if HD is enabled for all address types, false if only for Sapling */
    bool IsHDFullyEnabled() const;

    /* Generates a new HD seed (will reset the chain child index counters)
     * by randomly generating a mnemonic phrase that can be used for wallet
     * recovery, and deriving the HD seed from that phrase in accordance with
     * BIP 39 / ZIP 339. Sets the seed's version based on the current wallet
     * version (the caller must ensure the current wallet version is correct
     * before calling this function). */
    void GenerateNewSeed(Language language = English);

    bool SetMnemonicSeed(const MnemonicSeed& seed);
    bool SetCryptedMnemonicSeed(const uint256& seedFp, const std::vector<unsigned char> &vchCryptedSecret);
    /* Checks the wallet's seed against the specified mnemonic, and marks the
     * wallet's seed as having been backed up if the phrases match. */
    bool VerifyMnemonicSeed(const SecureString& mnemonic);
    bool MnemonicVerified();

    /* Set the current mnemonic phrase, without saving it to disk (used by LoadWallet) */
    bool LoadMnemonicSeed(const MnemonicSeed& seed);
    /* Set the legacy HD seed, without saving it to disk (used by LoadWallet) */
    bool LoadLegacyHDSeed(const HDSeed& seed);

    /* Set the current encrypted mnemonic phrase, without saving it to disk (used by LoadWallet) */
    bool LoadCryptedMnemonicSeed(const uint256& seedFp, const std::vector<unsigned char>& seed);
    /* Set the legacy encrypted HD seed, without saving it to disk (used by LoadWallet) */
    bool LoadCryptedLegacyHDSeed(const uint256& seedFp, const std::vector<unsigned char>& seed);

    /* Returns the wallet's HD seed or throw JSONRPCError(...) */
    HDSeed GetHDSeedForRPC() const;

    /* Set the metadata for the mnemonic HD seed (chain child index counters) */
    void SetMnemonicHDChain(const CHDChain& chain, bool memonly);
    const std::optional<CHDChain>& GetMnemonicHDChain() const { return mnemonicHDChain; }

    bool CheckNetworkInfo(std::pair<std::string, std::string> networkInfo) const;
    uint32_t BIP44CoinType() const;

    /**
     * Check whether the wallet contains spending keys for all the addresses
     * contained in the given address set.
     */
    bool HasSpendingKeys(const NoteFilter& noteFilter) const;

    /* Find notes filtered by payment addresses, min depth, max depth, if they are spent,
       if a spending key is required, and if they are locked */
    void GetFilteredNotes(std::vector<SproutNoteEntry>& sproutEntriesRet,
                          std::vector<SaplingNoteEntry>& saplingEntriesRet,
                          std::vector<OrchardNoteMetadata>& orchardNotesRet,
                          const std::optional<NoteFilter>& noteFilter,
                          int minDepth=1,
                          int maxDepth=INT_MAX,
                          bool ignoreSpent=true,
                          bool requireSpendingKey=true,
                          bool ignoreLocked=true) const;

    /* Returns the wallets help message */
    static std::string GetWalletHelpString(bool showDebug);

    /* Initializes the wallet, returns a new CWallet instance or a null pointer in case of an error */
    static bool InitLoadWallet(const CChainParams& params, bool clearWitnessCaches);

    /* Wallets parameter interaction */
    static bool ParameterInteraction(const CChainParams& params);
};

/** A key allocated from the key pool. */
class CReserveKey : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
public:
    CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
    }

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    virtual bool GetReservedKey(CPubKey &pubkey);
    void KeepKey();
    void KeepScript() { KeepKey(); }
};

//
// Shielded key and address generalizations
//

// PaymentAddressBelongsToWallet visitor :: (CWallet&, PaymentAddress) -> bool
class PaymentAddressBelongsToWallet
{
private:
    CWallet *m_wallet;
public:
    PaymentAddressBelongsToWallet(CWallet *wallet) : m_wallet(wallet) {}

    bool operator()(const CKeyID &zaddr) const;
    bool operator()(const CScriptID &zaddr) const;
    bool operator()(const libzcash::SproutPaymentAddress &zaddr) const;
    bool operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
    bool operator()(const libzcash::UnifiedAddress &uaddr) const;
};

// GetViewingKeyForPaymentAddress visitor :: (CWallet&, PaymentAddress) -> std::optional<ViewingKey>
class GetViewingKeyForPaymentAddress
{
private:
    CWallet *m_wallet;
public:
    GetViewingKeyForPaymentAddress(CWallet *wallet) : m_wallet(wallet) {}

    std::optional<libzcash::ViewingKey> operator()(const CKeyID &zaddr) const;
    std::optional<libzcash::ViewingKey> operator()(const CScriptID &zaddr) const;
    std::optional<libzcash::ViewingKey> operator()(const libzcash::SproutPaymentAddress &zaddr) const;
    std::optional<libzcash::ViewingKey> operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
    std::optional<libzcash::ViewingKey> operator()(const libzcash::UnifiedAddress &uaddr) const;
};

enum class PaymentAddressSource {
    Random,
    LegacyHDSeed,
    MnemonicHDSeed,
    Imported,
    ImportedWatchOnly,
    AddressNotFound,
};

// GetSourceForPaymentAddress visitor :: (CWallet&, PaymentAddress) -> PaymentAddressSource
class GetSourceForPaymentAddress
{
private:
    CWallet *m_wallet;
public:
    GetSourceForPaymentAddress(CWallet *wallet) : m_wallet(wallet) {}

    PaymentAddressSource GetUnifiedSource(const libzcash::Receiver& receiver) const;

    PaymentAddressSource operator()(const CKeyID &zaddr) const;
    PaymentAddressSource operator()(const CScriptID &zaddr) const;
    PaymentAddressSource operator()(const libzcash::SproutPaymentAddress &zaddr) const;
    PaymentAddressSource operator()(const libzcash::SaplingPaymentAddress &zaddr) const;
    PaymentAddressSource operator()(const libzcash::UnifiedAddress &uaddr) const;
};

enum KeyAddResult {
    SpendingKeyExists,
    KeyAlreadyExists,
    KeyAdded,
    KeyNotAdded,
};

// AddViewingKeyToWallet visitor :: (CWallet&, ViewingKey) -> KeyAddResult
class AddViewingKeyToWallet
{
private:
    CWallet *m_wallet;
    bool addDefaultAddress;
public:
    AddViewingKeyToWallet(CWallet *wallet, bool addDefaultAddressIn) : m_wallet(wallet), addDefaultAddress(addDefaultAddressIn) {}

    KeyAddResult operator()(const libzcash::SproutViewingKey &sk) const;
    KeyAddResult operator()(const libzcash::SaplingExtendedFullViewingKey &sk) const;
    KeyAddResult operator()(const libzcash::UnifiedFullViewingKey &sk) const;
};

// AddSpendingKeyToWallet visitor ::
// (CWallet&, Consensus::Params, ..., ViewingKey) -> KeyAddResult
class AddSpendingKeyToWallet
{
private:
    CWallet *m_wallet;
    const Consensus::Params &params;
    int64_t nTime;
    std::optional<std::string> hdKeypath; // currently sapling only
    std::optional<std::string> seedFpStr; // currently sapling only
    bool log;
    bool addDefaultAddress;
public:
    AddSpendingKeyToWallet(CWallet *wallet, const Consensus::Params &params) :
        m_wallet(wallet), params(params), nTime(1), hdKeypath(std::nullopt), seedFpStr(std::nullopt), log(false), addDefaultAddress(true) {}
    AddSpendingKeyToWallet(
        CWallet *wallet,
        const Consensus::Params &params,
        int64_t _nTime,
        std::optional<std::string> _hdKeypath,
        std::optional<std::string> _seedFp,
        bool _log,
        bool _addDefaultAddress
    ) : m_wallet(wallet), params(params), nTime(_nTime), hdKeypath(_hdKeypath), seedFpStr(_seedFp), log(_log), addDefaultAddress(_addDefaultAddress) {}


    KeyAddResult operator()(const libzcash::SproutSpendingKey &sk) const;
    KeyAddResult operator()(const libzcash::SaplingExtendedSpendingKey &sk) const;
};

// UFVKForReceiver :: (CWallet&, Receiver) -> std::optional<ZcashdUnifiedFullViewingKey>
class UFVKForReceiver {
private:
    const CWallet& wallet;

public:
    UFVKForReceiver(const CWallet& wallet): wallet(wallet) {}

    std::optional<libzcash::ZcashdUnifiedFullViewingKey> operator()(const libzcash::OrchardRawAddress& orchardAddr) const;
    std::optional<libzcash::ZcashdUnifiedFullViewingKey> operator()(const libzcash::SaplingPaymentAddress& saplingAddr) const;
    std::optional<libzcash::ZcashdUnifiedFullViewingKey> operator()(const CScriptID& scriptId) const;
    std::optional<libzcash::ZcashdUnifiedFullViewingKey> operator()(const CKeyID& keyId) const;
    std::optional<libzcash::ZcashdUnifiedFullViewingKey> operator()(const libzcash::UnknownReceiver& receiver) const;
};

// UnifiedAddressForReceiver :: (CWallet&, Receiver) -> std::optional<UnifiedAddress>
class UnifiedAddressForReceiver {
private:
    const CWallet& wallet;

public:
    UnifiedAddressForReceiver(const CWallet& wallet): wallet(wallet) {}

    std::optional<libzcash::UnifiedAddress> operator()(const libzcash::OrchardRawAddress& orchardAddr) const;
    std::optional<libzcash::UnifiedAddress> operator()(const libzcash::SaplingPaymentAddress& saplingAddr) const;
    std::optional<libzcash::UnifiedAddress> operator()(const CScriptID& scriptId) const;
    std::optional<libzcash::UnifiedAddress> operator()(const CKeyID& keyId) const;
    std::optional<libzcash::UnifiedAddress> operator()(const libzcash::UnknownReceiver& receiver) const;
};

#endif // BITCOIN_WALLET_WALLET_H
