// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_UTIL_TEST_H
#define ZCASH_UTIL_TEST_H

#include "coins.h"
#include "key_io.h"
#include "wallet/wallet.h"
#include "zcash/Address.hpp"
#include "zcash/Note.hpp"
#include "zcash/NoteEncryption.hpp"

// A fake chain state view where anchors and nullifiers are assumed to exist.
class AssumeShieldedInputsExistAndAreSpendable : public CCoinsView {
public:
    AssumeShieldedInputsExistAndAreSpendable() {}

    bool GetSproutAnchorAt(const uint256 &rt, SproutMerkleTree &tree) const {
        return true;
    }

    bool GetSaplingAnchorAt(const uint256 &rt, SaplingMerkleTree &tree) const {
        return true;
    }

    bool GetOrchardAnchorAt(const uint256 &rt, OrchardMerkleFrontier &tree) const {
        return true;
    }

    bool GetNullifier(const uint256 &nf, ShieldedType type) const {
        // Always return false so we treat every nullifier as being unspent.
        return false;
    }

    bool GetCoins(const uint256 &txid, CCoins &coins) const { return false; }
    bool HaveCoins(const uint256 &txid) const { return false; }
    uint256 GetBestBlock() const {
        throw std::runtime_error("`GetBestBlock` unimplemented for mock AssumeShieldedInputsExistAndAreSpendable");
    }
    uint256 GetBestAnchor(ShieldedType type) const {
        throw std::runtime_error("`GetBestAnchor` unimplemented for mock AssumeShieldedInputsExistAndAreSpendable");
    }
    HistoryIndex GetHistoryLength(uint32_t epochId) const { return 0; }
    HistoryNode GetHistoryAt(uint32_t epochId, HistoryIndex index) const {
        throw std::runtime_error("`GetHistoryAt` unimplemented for mock AssumeShieldedInputsExistAndAreSpendable");
    }
    uint256 GetHistoryRoot(uint32_t epochId) const {
        throw std::runtime_error("`GetHistoryRoot` unimplemented for mock AssumeShieldedInputsExistAndAreSpendable");
    }

    bool BatchWrite(CCoinsMap &mapCoins,
                    const uint256 &hashBlock,
                    const uint256 &hashSproutAnchor,
                    const uint256 &hashSaplingAnchor,
                    const uint256 &hashOrchardAnchor,
                    CAnchorsSproutMap &mapSproutAnchors,
                    CAnchorsSaplingMap &mapSaplingAnchors,
                    CAnchorsOrchardMap &mapOrchardAnchors,
                    CNullifiersMap &mapSproutNullifiers,
                    CNullifiersMap &mapSaplingNullifiers,
                    CNullifiersMap &mapOrchardNullifiers,
                    CHistoryCacheMap &historyCacheMap) {
        return false;
    }
    bool GetStats(CCoinsStats &stats) const { return false; }
};

// Sprout
CWalletTx GetValidSproutReceive(const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                uint32_t versionGroupId = SAPLING_VERSION_GROUP_ID,
                                int32_t version = SAPLING_TX_VERSION);
CWalletTx GetInvalidCommitmentSproutReceive(
                                const libzcash::SproutSpendingKey& sk,
                                CAmount value,
                                bool randomInputs,
                                uint32_t versionGroupId = SAPLING_VERSION_GROUP_ID,
                                int32_t version = SAPLING_TX_VERSION);
libzcash::SproutNote GetSproutNote(const libzcash::SproutSpendingKey& sk,
                                   const CTransaction& tx, size_t js, size_t n);
CWalletTx GetValidSproutSpend(const libzcash::SproutSpendingKey& sk,
                              const libzcash::SproutNote& note,
                              CAmount value);

// Sapling
static const std::string T_SECRET_REGTEST = "cND2ZvtabDbJ1gucx9GWH6XT9kgTAqfb6cotPt5Q5CyxVDhid2EN";

struct TestSaplingNote {
    libzcash::SaplingNote note;
    SaplingMerkleTree tree;
};

const CChainParams& RegtestActivateOverwinter();
void RegtestDeactivateOverwinter();

const Consensus::Params& RegtestActivateSapling();

void RegtestDeactivateSapling();

const CChainParams& RegtestActivateBlossom(bool updatePow, int blossomActivationHeight = Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

void RegtestDeactivateBlossom();

const Consensus::Params& RegtestActivateHeartwood(bool updatePow, int heartwoodActivationHeight = Consensus::NetworkUpgrade::ALWAYS_ACTIVE);

void RegtestDeactivateHeartwood();

const Consensus::Params& RegtestActivateCanopy(bool updatePow, int canopyActivationHeight = Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
const Consensus::Params& RegtestActivateCanopy();

void RegtestDeactivateCanopy();

const Consensus::Params& RegtestActivateNU5(bool updatePow, int nu5ActivationHeight = Consensus::NetworkUpgrade::ALWAYS_ACTIVE);
const Consensus::Params& RegtestActivateNU5();

void RegtestDeactivateNU5();

libzcash::SaplingExtendedSpendingKey GetTestMasterSaplingSpendingKey();

CKey AddTestCKeyToKeyStore(CBasicKeyStore& keyStore);

SpendDescription RandomInvalidSpendDescription();
OutputDescription RandomInvalidOutputDescription();

/**
 * Generate a dummy SaplingNote and a SaplingMerkleTree with that note's commitment.
 */
TestSaplingNote GetTestSaplingNote(const libzcash::SaplingPaymentAddress& pa, CAmount value);

CWalletTx GetValidSaplingReceive(const Consensus::Params& consensusParams,
                                 CBasicKeyStore& keyStore,
                                 const libzcash::SaplingExtendedSpendingKey &sk,
                                 CAmount value);

#endif // ZCASH_UTIL_TEST_H
