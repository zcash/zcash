// Copyright (c) 2022-2023 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_WALLET_WALLET_TX_BUILDER_H
#define ZCASH_WALLET_WALLET_TX_BUILDER_H

#include "chainparams.h"
#include "transaction_builder.h"
#include "wallet/wallet.h"
#include "zcash/memo.h"

using namespace libzcash;

int GetAnchorHeight(const CChain& chain, int anchorConfirmations);

/**
 * A payment that has been resolved to send to a specific recipient address in a single pool. This
 * is an internal type that represents both user-requested payment addresses and generated
 * (internal) payments (like change).
 */
class ResolvedPayment : public RecipientMapping {
public:
    CAmount amount;
    std::optional<Memo> memo;
    bool isInternal;

    ResolvedPayment(
            std::optional<libzcash::UnifiedAddress> ua,
            libzcash::RecipientAddress address,
            CAmount amount,
            std::optional<Memo> memo,
            bool isInternal) :
        RecipientMapping(ua, address), amount(amount), memo(memo), isInternal(isInternal) {}
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
        address(address), amount(amount), memo(memo) {
        assert(MoneyRange(amount));
    }

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

typedef std::pair<libzcash::PaymentAddress, std::optional<Memo>> NetAmountRecipient;

typedef std::variant<
    std::vector<Payment>,
    NetAmountRecipient> Recipients;

class Payments {
private:
    std::vector<ResolvedPayment> payments;
    std::set<OutputPool> recipientPools;
    CAmount t_outputs_total{0};
    CAmount sapling_outputs_total{0};
    CAmount orchard_outputs_total{0};
public:
    Payments(std::vector<ResolvedPayment> payments) {
        for (const ResolvedPayment& payment : payments) {
            AddPayment(payment);
        }
    }

    void AddPayment(ResolvedPayment payment) {
        examine(payment.address, match {
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
        });
        payments.push_back(payment);
    }

    std::set<OutputPool> GetRecipientPools() const {
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

    CAmount GetTransparentTotal() const {
        return t_outputs_total;
    }

    CAmount GetSaplingTotal() const {
        return sapling_outputs_total;
    }

    CAmount GetOrchardTotal() const {
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
    uint32_t anchorConfirmations{1};
    SpendableInputs spendable;
    Payments payments;
    std::optional<ChangeAddress> changeAddr;
    CAmount fee{0};
    uint256 internalOVK;
    uint256 externalOVK;
    // TODO: This needs to be richer, like an `anchorBlock`, so the `TransactionEffects` can
    //       be recalculated if the state of the chain has changed significantly between the time of
    //       preparation and the time of approval.
    int anchorHeight;

public:
    TransactionEffects(
        uint32_t anchorConfirmations,
        SpendableInputs spendable,
        Payments payments,
        std::optional<ChangeAddress> changeAddr,
        CAmount fee,
        uint256 internalOVK,
        uint256 externalOVK,
        int anchorHeight) :
            anchorConfirmations(anchorConfirmations),
            spendable(spendable),
            payments(payments),
            changeAddr(changeAddr),
            fee(fee),
            internalOVK(internalOVK),
            externalOVK(externalOVK),
            anchorHeight(anchorHeight) {}

    /**
     * Returns the strongest `PrivacyPolicy` that is compatible with the transaction’s effects.
     */
    PrivacyPolicy GetRequiredPrivacyPolicy() const;

    const SpendableInputs& GetSpendable() const {
        return spendable;
    }

    /**
     * This should be called upon creating `TransactionEffects`, it locks exactly the notes that
     * will be spent in the built transaction.
     */
    void LockSpendable(CWallet& wallet) const;

    /**
     * This should be called when we are finished with the transaction (whether it succeeds or
     * fails).
     *
     * TODO: This currently needs to be called while the `TransactionEffects` exists. In future, it
     *       would be useful to keep these notes locked until we have confirmation that the tx is on
     *       the chain or not.
     */
    void UnlockSpendable(CWallet& wallet) const;

    const Payments& GetPayments() const {
        return payments;
    }

    CAmount GetFee() const {
        return fee;
    }

    bool InvolvesOrchard() const;

    TransactionBuilderResult ApproveAndBuild(
            const CChainParams& params,
            const CWallet& wallet,
            const CChain& chain,
            const TransactionStrategy& strategy) const;
};

enum class AddressResolutionError {
    //! Zcashd no longer supports sending to Sprout.
    SproutRecipientsNotSupported,
    //! Requested `PrivacyPolicy` doesn’t include `AllowRevealedRecipients`
    TransparentRecipientNotAllowed,
    //! Requested `PrivacyPolicy` doesn’t include `AllowRevealedRecipients`
    TransparentChangeNotAllowed,
    //! Requested `PrivacyPolicy` doesn’t include `AllowRevealedAmounts`, but we don’t have enough
    //! Sapling funds to avoid revealing amounts
    RevealingSaplingAmountNotAllowed,
    //! Despite a lax `PrivacyPolicy`, other factors made it impossible to find a receiver for a
    //! recipient UA
    CouldNotResolveReceiver,
    //! Requested `PrivacyPolicy` doesn’t include `AllowRevealedRecipients`, but we are trying to
    //! pay a UA where we can only select a transparent receiver
    TransparentReceiverNotAllowed,
    //! Requested `PrivacyPolicy` doesn’t include `AllowRevealedAmounts`, but we are trying to pay a
    //! UA where we don’t have enough funds in any single pool that it has a receiver for
    RevealingReceiverAmountsNotAllowed,
};

/// Phantom change is change that appears to exist until we add the output for it, at which point it
/// is consumed by the increase to the conventional fee. When we are at the limit of selectable
/// notes, this makes it impossible to create the transaction without either creating a 0-valued
/// output or overpaying the fee.
class PhantomChangeError {
public:
    CAmount finalFee;
    CAmount dustThreshold;

    PhantomChangeError(CAmount finalFee, CAmount dustThreshold):
        finalFee(finalFee), dustThreshold(dustThreshold) { }
};

class InsufficientFundsError {
public:
    CAmount required;

    InsufficientFundsError(CAmount required):
        required(required) { }
};

class DustThresholdError {
public:
    CAmount dustThreshold;
    CAmount changeAmount;

    DustThresholdError(CAmount dustThreshold, CAmount changeAmount):
        dustThreshold(dustThreshold), changeAmount(changeAmount) { }
};

typedef std::variant<
    PhantomChangeError,
    InsufficientFundsError,
    DustThresholdError> InvalidFundsReason;

class InvalidFundsError {
public:
    CAmount available;
    const InvalidFundsReason reason;

    InvalidFundsError(CAmount available, const InvalidFundsReason reason):
        available(available), reason(reason) { }
};

class ChangeNotAllowedError {
public:
    CAmount available;
    CAmount required;

    ChangeNotAllowedError(CAmount available, CAmount required):
        available(available), required(required) { }
};

/// Error when a fee is outside `MoneyRange`
class InvalidFeeError {
public:
    CAmount fixedFee;

    InvalidFeeError(CAmount fixedFee):
        fixedFee(fixedFee) { }
};

/// Error when a fee is higher than can be useful. This reduces the chance of accidentally
/// overpaying with explicit fees.
class AbsurdFeeError {
public:
    CAmount conventionalFee;
    CAmount fixedFee;

    AbsurdFeeError(CAmount conventionalFee, CAmount fixedFee):
        conventionalFee(conventionalFee), fixedFee(fixedFee) { }
};

/// Error when a fee is higher than this instance allows.
class MaxFeeError {
public:
    CAmount fixedFee;

    MaxFeeError(CAmount fixedFee):
        fixedFee(fixedFee) { }
};

enum ActionSide {
    Input,
    Output,
    Both,
};

class ExcessOrchardActionsError {
public:
    ActionSide side;
    uint32_t orchardNotes;
    uint32_t maxNotes;

    ExcessOrchardActionsError(ActionSide side, uint32_t orchardNotes, uint32_t maxNotes):
        side(side), orchardNotes(orchardNotes), maxNotes(maxNotes) { }
};

typedef std::variant<
    AddressResolutionError,
    InvalidFundsError,
    ChangeNotAllowedError,
    InvalidFeeError,
    AbsurdFeeError,
    MaxFeeError,
    ExcessOrchardActionsError> InputSelectionError;

class InputSelection {
private:
    SpendableInputs inputs;
    Payments payments;
    CAmount fee;
    std::optional<ChangeAddress> changeAddr;

public:
    InputSelection(SpendableInputs inputs, Payments payments, CAmount fee, std::optional<ChangeAddress> changeAddr):
        inputs(inputs), payments(payments), fee(fee), changeAddr(changeAddr) {}

    const SpendableInputs& GetInputs() const;
    const Payments& GetPayments() const;
    CAmount GetFee() const;
    const std::optional<ChangeAddress> GetChangeAddress() const;
};

class WalletTxBuilder {
private:
    const CChainParams& params;
    CFeeRate minRelayFee;
    uint32_t maxOrchardActions;

    /**
     * Compute the default dust threshold
     */
    CAmount DefaultDustThreshold() const;

    tl::expected<ChangeAddress, AddressResolutionError>
    GetChangeAddress(
            CWallet& wallet,
            const ZTXOSelector& selector,
            const SpendableInputs& spendable,
            const Payments& resolvedPayments,
            const TransactionStrategy& strategy,
            bool afterNU5) const;

    tl::expected<
        std::tuple<SpendableInputs, CAmount, std::optional<ChangeAddress>>,
        InputSelectionError>
    IterateLimit(
            CWallet& wallet,
            const ZTXOSelector& selector,
            const TransactionStrategy& strategy,
            CAmount sendAmount,
            CAmount dustThreshold,
            const SpendableInputs& spendable,
            Payments& resolved,
            bool afterNU5,
            uint32_t consensusBranchId) const;

    /**
     * Select inputs sufficient to fulfill the specified requested payments,
     * and choose unified address receivers based upon the available inputs
     * and the requested transaction strategy.
     */
    tl::expected<InputSelection, InputSelectionError>
    ResolveInputsAndPayments(
            CWallet& wallet,
            const ZTXOSelector& selector,
            const SpendableInputs& spendable,
            const std::vector<Payment>& payments,
            const TransactionStrategy& strategy,
            const std::optional<CAmount>& fee,
            bool afterNU5,
            uint32_t consensusBranchId) const;
    /**
     * Compute the internal and external OVKs to use in transaction construction, given
     * the spendable inputs.
     */
    std::pair<uint256, uint256> SelectOVKs(
            const CWallet& wallet,
            const ZTXOSelector& selector,
            const SpendableInputs& spendable) const;

public:
    WalletTxBuilder(const CChainParams& params, CFeeRate minRelayFee):
        params(params), minRelayFee(minRelayFee), maxOrchardActions(nOrchardActionLimit) {}

    SpendableInputs FindAllSpendableInputs(
            const CWallet& wallet,
            const ZTXOSelector& selector,
            int32_t minDepth) const;

    tl::expected<TransactionEffects, InputSelectionError>
    PrepareTransaction(
            CWallet& wallet,
            const ZTXOSelector& selector,
            const SpendableInputs& spendable,
            const Recipients& payments,
            const CChain& chain,
            const TransactionStrategy& strategy,
            /// A fixed fee is used if provided, otherwise it is calculated based on ZIP 317.
            const std::optional<CAmount>& fee,
            uint32_t anchorConfirmations) const;
};

#endif
