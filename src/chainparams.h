// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2016-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "consensus/params.h"
#include "primitives/block.h"
#include "protocol.h"

#include <vector>

#include <rust/bridge.h>

struct CDNSSeedData {
    std::string name, host;
    CDNSSeedData(const std::string &strName, const std::string &strHost) : name(strName), host(strHost) {}
};

struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

typedef std::map<int, uint256> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
    int64_t nTimeLastCheckpoint;
    int64_t nTransactionsLastCheckpoint;
    double fTransactionsPerDay;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams: public KeyConstants
{
public:
    const Consensus::Params& GetConsensus() const { return consensus; }
    const rust::Box<consensus::Network> RustNetwork() const {
        return consensus::network(
            NetworkIDString(),
            consensus.vUpgrades[Consensus::UPGRADE_OVERWINTER].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_SAPLING].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_BLOSSOM].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_HEARTWOOD].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_CANOPY].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_NU5].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_NU6].nActivationHeight,
            consensus.vUpgrades[Consensus::UPGRADE_ZFUTURE].nActivationHeight);
    }
    const CMessageHeader::MessageStartChars& MessageStart() const { return pchMessageStart; }
    const std::vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    int GetDefaultPort() const { return nDefaultPort; }

    CAmount SproutValuePoolCheckpointHeight() const { return nSproutValuePoolCheckpointHeight; }
    CAmount SproutValuePoolCheckpointBalance() const { return nSproutValuePoolCheckpointBalance; }
    uint256 SproutValuePoolCheckpointBlockHash() const { return hashSproutValuePoolCheckpointBlock; }
    bool ZIP209Enabled() const { return fZIP209Enabled; }
    bool RequireWalletBackup() const { return fRequireWalletBackup; }

    const CBlock& GenesisBlock() const { return genesis; }
    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return fMiningRequiresPeers; }
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Policy: Filter transactions that do not match well-defined patterns */
    bool RequireStandard() const { return fRequireStandard; }
    int64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    std::string CurrencyUnits() const { return strCurrencyUnits; }
    /** Make miner stop after a block is found. In RPC, don't return until nGenProcLimit blocks are generated */
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /** In the future use NetworkIDString() for RPC fields */
    bool TestnetToBeDeprecatedFieldRPC() const { return fTestnetToBeDeprecatedFieldRPC; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const {
        return keyConstants.NetworkIDString();
    }
    /** Return the BIP44 coin type for addresses created by the zcashd embedded wallet. */
    uint32_t BIP44CoinType() const {
        return keyConstants.BIP44CoinType();
    }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const {
        return keyConstants.Base58Prefix(type);
    }
    const std::string& Bech32HRP(Bech32Type type) const {
        return keyConstants.Bech32HRP(type);
    }
    const std::string& Bech32mHRP(Bech32mType type) const {
        return keyConstants.Bech32mHRP(type);
    }
    const std::vector<SeedSpec6>& FixedSeeds() const { return vFixedSeeds; }
    const CCheckpointData& Checkpoints() const { return checkpointData; }
    /** Return the founder's reward address and script for a given block height */
    std::string GetFoundersRewardAddressAtHeight(int height) const;
    CScript GetFoundersRewardScriptAtHeight(int height) const;
    std::string GetFoundersRewardAddressAtIndex(int i) const;
    /** Enforce coinbase consensus rule in regtest mode */
    void SetRegTestCoinbaseMustBeShielded() { consensus.fCoinbaseMustBeShielded = true; }
protected:
    CChainParams() {}

    Consensus::Params consensus;
    CMessageHeader::MessageStartChars pchMessageStart;
    //! Raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char> vAlertPubKey;
    int nDefaultPort = 0;
    uint64_t nPruneAfterHeight = 0;
    std::vector<CDNSSeedData> vSeeds;
    CBaseKeyConstants keyConstants;
    std::string strCurrencyUnits;
    CBlock genesis;
    std::vector<SeedSpec6> vFixedSeeds;
    bool fMiningRequiresPeers = false;
    bool fDefaultConsistencyChecks = false;
    bool fRequireStandard = false;
    bool fMineBlocksOnDemand = false;
    bool fTestnetToBeDeprecatedFieldRPC = false;
    CCheckpointData checkpointData;
    std::vector<std::string> vFoundersRewardAddress;

    CAmount nSproutValuePoolCheckpointHeight = 0;
    CAmount nSproutValuePoolCheckpointBalance = 0;
    uint256 hashSproutValuePoolCheckpointBlock;
    bool fZIP209Enabled = false;
    bool fRequireWalletBackup = true;
};

/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams &Params();

/**
 * @returns CChainParams for the given BIP70 chain name.
 */
const CChainParams& Params(const std::string& chain);

/**
 * Sets the params returned by Params() to those for the given BIP70 chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string& chain);

/**
 * Allows modifying the network upgrade regtest parameters.
 */
void UpdateNetworkUpgradeParameters(Consensus::UpgradeIndex idx, int nActivationHeight);

void UpdateRegtestPow(
    int64_t nPowMaxAdjustDown,
    int64_t nPowMaxAdjustUp,
    uint256 powLimit,
    bool noRetargeting);

/**
 * Allows modifying the regtest funding stream parameters.
 */
void UpdateFundingStreamParameters(Consensus::FundingStreamIndex idx, Consensus::FundingStream fs);

#endif // BITCOIN_CHAINPARAMS_H
