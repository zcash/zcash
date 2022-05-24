// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_WALLET_TX_BUILDER_H
#define ZCASH_WALLET_WALLET_TX_BUILDER_H

#include "consensus/params.h"
#include "transaction_builder.h"
#include "wallet/memo.h"
#include "wallet/wallet.h"

using namespace libzcash;

/**
 * A payment that has been resolved to send to a specific
 * recipient address in a single pool.
 */
class ResolvedPayment : public RecipientMapping {
public:
    CAmount amount;
    std::optional<Memo> memo;

    ResolvedPayment(
            std::optional<libzcash::UnifiedAddress> ua,
            libzcash::RecipientAddress address,
            CAmount amount,
            std::optional<Memo> memo) :
        RecipientMapping(ua, address), amount(amount), memo(memo) {}
};

/**
 * A requested payment that has not yet been resolved to a
 * specific recipient address.
 */
class Payment {
private:
    PaymentAddress address;
    CAmount amount;
    std::optional<Memo> memo;
public:
    Payment(
            PaymentAddress address,
            CAmount amount,
            std::optional<Memo> memo) :
        address(address), amount(amount), memo(memo) {}

    const PaymentAddress& GetAddress() const {
        return address;
    }

    CAmount GetAmount() const {
        return amount;
    }

    const std::optional<Memo>& GetMemo() const {
        return memo;
    }
};

class Payments {
private:
    std::vector<ResolvedPayment> payments;
    std::set<OutputPool> recipientPools;
    CAmount t_outputs_total{0};
    CAmount sapling_outputs_total{0};
    CAmount orchard_outputs_total{0};
public:
    Payments(std::vector<ResolvedPayment> payments): payments(payments) {
        for (const ResolvedPayment& payment : payments) {
            std::visit(match {
                [&](const CKeyID& addr) {
                    t_outputs_total += payment.amount;
                    recipientPools.insert(OutputPool::Transparent);
                },
                [&](const CScriptID& addr) {
                    t_outputs_total += payment.amount;
                    recipientPools.insert(OutputPool::Transparent);
                },
                [&](const libzcash::SaplingPaymentAddress& addr) {
                    sapling_outputs_total += payment.amount;
                    recipientPools.insert(OutputPool::Sapling);
                },
                [&](const libzcash::OrchardRawAddress& addr) {
                    orchard_outputs_total += payment.amount;
                    recipientPools.insert(OutputPool::Orchard);
                }
            }, payment.address);
        }
    }

    const std::set<OutputPool>& GetRecipientPools() const {
        return recipientPools;
    }

    bool HasTransparentRecipient() const {
        return recipientPools.count(OutputPool::Transparent) > 0;
    }

    bool HasSaplingRecipient() const {
        return recipientPools.count(OutputPool::Sapling) > 0;
    }

    bool HasOrchardRecipient() const {
        return recipientPools.count(OutputPool::Orchard) > 0;
    }

    const std::vector<ResolvedPayment>& GetResolvedPayments() const {
        return payments;
    }

    CAmount GetTransparentBalance() const {
        return t_outputs_total;
    }

    CAmount GetSaplingBalance() const {
        return sapling_outputs_total;
    }

    CAmount GetOrchardBalance() const {
        return orchard_outputs_total;
    }

    CAmount Total() const {
        return
            t_outputs_total +
            sapling_outputs_total +
            orchard_outputs_total;
    }
};

typedef std::variant<
    RecipientAddress,
    SproutPaymentAddress> ChangeAddress;

class TransactionEffects {
private:
    AccountId sendFromAccount;
    uint32_t anchorConfirmations{1};
    SpendableInputs spendable;
    Payments payments;
    ChangeAddress changeAddr;
    CAmount fee{0};
    uint256 internalOVK;
    uint256 externalOVK;

public:
    TransactionEffects(
        AccountId sendFromAccount,
        uint32_t anchorConfirmations,
        SpendableInputs spendable,
        Payments payments,
        ChangeAddress changeAddr,
        CAmount fee,
        uint256 internalOVK,
        uint256 externalOVK) :
            sendFromAccount(sendFromAccount),
            anchorConfirmations(anchorConfirmations),
            spendable(spendable),
            payments(payments),
            changeAddr(changeAddr),
            fee(fee),
            internalOVK(internalOVK),
            externalOVK(externalOVK) {}

    PrivacyPolicy GetRequiredPrivacyPolicy() const;

    const SpendableInputs& GetSpendable() const {
        return spendable;
    }

    const Payments& GetPayments() const {
        return payments;
    }

    CAmount GetFee() const {
        return fee;
    }

    TransactionBuilderResult ApproveAndBuild(
            const Consensus::Params& consensus,
            const CWallet& wallet,
            const CChain& chain,
            const TransactionStrategy& strategy) const;
};

enum class AddressResolutionError {
    SproutSpendNotPermitted,
    SproutRecipientNotPermitted,
    TransparentRecipientNotPermitted,
    InsufficientSaplingFunds,
    UnifiedAddressResolutionError,
    ChangeAddressSelectionError
};

class DustThresholdError {
public:
    CAmount dustThreshold;
    CAmount available;
    CAmount changeAmount;

    DustThresholdError(CAmount dustThreshold, CAmount available, CAmount changeAmount):
        dustThreshold(dustThreshold), available(available), changeAmount(changeAmount) { }
};

class ChangeNotAllowedError {
public:
    CAmount available;
    CAmount required;

    ChangeNotAllowedError(CAmount available, CAmount required):
        available(available), required(required) { }
};

class InsufficientFundsError {
public:
    CAmount available;
    CAmount required;
    bool transparentCoinbasePermitted;

    InsufficientFundsError(CAmount available, CAmount required, bool transparentCoinbasePermitted):
        available(available), required(required), transparentCoinbasePermitted(transparentCoinbasePermitted) { }
};

class ExcessOrchardActionsError {
public:
    uint32_t orchardNotes;

    ExcessOrchardActionsError(uint32_t orchardNotes): orchardNotes(orchardNotes) { }
};

typedef std::variant<
    AddressResolutionError,
    InsufficientFundsError,
    DustThresholdError,
    ChangeNotAllowedError,
    ExcessOrchardActionsError> InputSelectionError;

typedef std::variant<
    InputSelectionError,
    std::pair<SpendableInputs, Payments>> InputSelectionResult;

typedef std::variant<
    InputSelectionError,
    TransactionEffects> PrepareTransactionResult;

class WalletTxBuilder {
private:
    const CWallet& wallet;
    CFeeRate minRelayFee;
    uint32_t maxOrchardActions;

    /**
     * Select inputs sufficient to fulfill the specified requested payments.
     */
    InputSelectionResult SelectInputs(
        const ZTXOSelector& selector,
        const std::vector<Payment>& payments,
        TransactionStrategy strategy,
        int mindepth,
        CAmount fee) const;

    /**
     * Compute the default dust threshold
     */
    CAmount DefaultDustThreshold() const;

    /**
     * Compute the internal and external OVKs to use in transaction construction, given
     * the spendable inputs.
     */
    std::pair<uint256, uint256> SelectOVKs(
        const ZTXOSelector& selector,
        const SpendableInputs& spendable) const;

public:
    WalletTxBuilder(const CWallet& wallet, CFeeRate minRelayFee):
        wallet(wallet), minRelayFee(minRelayFee) {}

    PrepareTransactionResult PrepareTransaction(
            const ZTXOSelector& selector,
            const std::vector<Payment>& payments,
            TransactionStrategy strategy,
            int32_t minDepth,
            CAmount fee,
            uint32_t anchorConfirmations) const;
};

#endif
