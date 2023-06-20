// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/wallet_tx_builder.h"

#include "script/sign.h"
#include "util/moneystr.h"
#include "zip317.h"

using namespace libzcash;

int GetAnchorHeight(const CChain& chain, uint32_t anchorConfirmations)
{
    int nextBlockHeight = chain.Height() + 1;
    return std::max(0, nextBlockHeight - (int) anchorConfirmations);
}

static size_t PadCount(size_t n)
{
    return n == 1 ? 2 : n;
}

static CAmount
CalcZIP317Fee(
        const CWallet& wallet,
        const std::optional<SpendableInputs>& inputs,
        const std::vector<ResolvedPayment>& payments,
        const std::optional<ChangeAddress>& changeAddr,
        uint32_t consensusBranchId)
{
    std::vector<CTxOut> vout{};
    size_t sproutOutputCount{}, saplingOutputCount{}, orchardOutputCount{};
    for (const auto& payment : payments) {
        std::visit(match {
            [&](const CKeyID& addr) {
                vout.emplace_back(payment.amount, GetScriptForDestination(addr));
            },
            [&](const CScriptID& addr) {
                vout.emplace_back(payment.amount, GetScriptForDestination(addr));
            },
            [&](const libzcash::SaplingPaymentAddress&) {
                ++saplingOutputCount;
            },
            [&](const libzcash::OrchardRawAddress&) {
                ++orchardOutputCount;
            }
        }, payment.address);
    }

    if (changeAddr.has_value()) {
        examine(changeAddr.value(), match {
            [&](const SproutPaymentAddress&) { ++sproutOutputCount; },
            [&](const RecipientAddress& addr) {
                examine(addr, match {
                    [&](const CKeyID& taddr) {
                        vout.emplace_back(0, GetScriptForDestination(taddr));
                    },
                    [&](const CScriptID taddr) {
                        vout.emplace_back(0, GetScriptForDestination(taddr));
                    },
                    [&](const libzcash::SaplingPaymentAddress&) { ++saplingOutputCount; },
                    [&](const libzcash::OrchardRawAddress&) { ++orchardOutputCount; }
                });
            }
        });
    }

    std::vector<CTxIn> vin{};
    size_t sproutInputCount = 0;
    size_t saplingInputCount = 0;
    size_t orchardInputCount = 0;
    if (inputs.has_value()) {
        for (const auto& utxo : inputs.value().utxos) {
            SignatureData sigdata;
            ProduceSignature(
                    DummySignatureCreator(&wallet),
                    utxo.tx->vout[utxo.i].scriptPubKey,
                    sigdata,
                    consensusBranchId);
            vin.emplace_back(COutPoint(utxo.tx->GetHash(), utxo.i), sigdata.scriptSig);
        }
        sproutInputCount = inputs.value().sproutNoteEntries.size();
        saplingInputCount = inputs.value().saplingNoteEntries.size();
        orchardInputCount = inputs.value().orchardNoteMetadata.size();
    }
    size_t logicalActionCount = CalculateLogicalActionCount(
            vin,
            vout,
            std::max(sproutInputCount, sproutOutputCount),
            saplingInputCount,
            PadCount(saplingOutputCount),
            PadCount(std::max(orchardInputCount, orchardOutputCount)));

    return CalculateConventionalFee(logicalActionCount);
}

static tl::expected<ResolvedPayment, AddressResolutionError>
ResolvePayment(
        const Payment& payment,
        bool canResolveOrchard,
        const TransactionStrategy& strategy,
        CAmount& maxSaplingAvailable,
        CAmount& maxOrchardAvailable,
        uint32_t& orchardOutputs)
{
    return examine(payment.GetAddress(), match {
        [&](const CKeyID& p2pkh) -> tl::expected<ResolvedPayment, AddressResolutionError> {
            if (strategy.AllowRevealedRecipients()) {
                return {{std::nullopt, p2pkh, payment.GetAmount(), payment.GetMemo(), false}};
            } else {
                return tl::make_unexpected(AddressResolutionError::TransparentRecipientNotAllowed);
            }
        },
        [&](const CScriptID& p2sh) -> tl::expected<ResolvedPayment, AddressResolutionError> {
            if (strategy.AllowRevealedRecipients()) {
                return {{std::nullopt, p2sh, payment.GetAmount(), payment.GetMemo(), false}};
            } else {
                return tl::make_unexpected(AddressResolutionError::TransparentRecipientNotAllowed);
            }
        },
        [&](const SproutPaymentAddress&) -> tl::expected<ResolvedPayment, AddressResolutionError> {
            return tl::make_unexpected(AddressResolutionError::SproutRecipientsNotSupported);
        },
        [&](const SaplingPaymentAddress& addr)
            -> tl::expected<ResolvedPayment, AddressResolutionError> {
            if (strategy.AllowRevealedAmounts() || payment.GetAmount() <= maxSaplingAvailable) {
                if (!strategy.AllowRevealedAmounts()) {
                    maxSaplingAvailable -= payment.GetAmount();
                }
                return {{std::nullopt, addr, payment.GetAmount(), payment.GetMemo(), false}};
            } else {
                return tl::make_unexpected(AddressResolutionError::RevealingSaplingAmountNotAllowed);
            }
        },
        [&](const UnifiedAddress& ua) -> tl::expected<ResolvedPayment, AddressResolutionError> {
            if (canResolveOrchard
                && ua.GetOrchardReceiver().has_value()
                && (strategy.AllowRevealedAmounts() || payment.GetAmount() <= maxOrchardAvailable)
                ) {
                if (!strategy.AllowRevealedAmounts()) {
                    maxOrchardAvailable -= payment.GetAmount();
                }
                orchardOutputs += 1;
                return {{
                    ua,
                    ua.GetOrchardReceiver().value(),
                    payment.GetAmount(),
                    payment.GetMemo(),
                    false
                }};
            } else if (ua.GetSaplingReceiver().has_value()
                && (strategy.AllowRevealedAmounts() || payment.GetAmount() <= maxSaplingAvailable)
                ) {
                if (!strategy.AllowRevealedAmounts()) {
                    maxSaplingAvailable -= payment.GetAmount();
                }
                return {{ua, ua.GetSaplingReceiver().value(), payment.GetAmount(), payment.GetMemo(), false}};
            } else {
                if (strategy.AllowRevealedRecipients()) {
                    if (ua.GetP2SHReceiver().has_value()) {
                        return {{
                            ua, ua.GetP2SHReceiver().value(), payment.GetAmount(), std::nullopt, false}};
                    } else if (ua.GetP2PKHReceiver().has_value()) {
                        return {{
                            ua, ua.GetP2PKHReceiver().value(), payment.GetAmount(), std::nullopt, false}};
                    } else {
                        // This should only occur when we have
                        // • an Orchard-only UA,
                        // • `AllowRevealedRecipients`, and
                        // • can’t resolve Orchard (which means either a Sprout selector or pre-NU5).
                        return tl::make_unexpected(AddressResolutionError::CouldNotResolveReceiver);
                    }
                } else if (strategy.AllowRevealedAmounts()) {
                    return tl::make_unexpected(AddressResolutionError::TransparentReceiverNotAllowed);
                } else {
                    return tl::make_unexpected(AddressResolutionError::RevealingReceiverAmountsNotAllowed);
                }
            }
        }
    });
}

InvalidFundsError ReportInvalidFunds(
        const SpendableInputs& spendable,
        bool hasPhantomChange,
        CAmount fee,
        CAmount dustThreshold,
        CAmount targetAmount,
        CAmount changeAmount)
{
    return InvalidFundsError(
            spendable.Total(),
            hasPhantomChange
            // TODO: NEED TESTS TO EXERCISE THIS
            ? InvalidFundsReason(PhantomChangeError(fee, dustThreshold))
            : (changeAmount > 0 && changeAmount < dustThreshold
               // TODO: we should provide the option for the caller to explicitly forego change
               // (definitionally an amount below the dust amount) and send the extra to the
               // recipient or the miner fee to avoid creating dust change, rather than prohibit
               // them from sending entirely in this circumstance. (Daira disagrees, as this could
               // leak information to the recipient or publicly in the fee.)
               ? InvalidFundsReason(DustThresholdError(dustThreshold, changeAmount))
               : InvalidFundsReason(InsufficientFundsError(targetAmount))));
}

static tl::expected<void, InputSelectionError>
ValidateAmount(const SpendableInputs& spendable, const CAmount& fee)
{
    // TODO: The actual requirement should probably be higher than simply `fee` – do we need to
    //       take into account the dustThreshold when adding an output? But, this was the
    //       pre-WalletTxBuilder behavior, so it’s fine to maintain it for now.
    auto targetAmount = fee;
    if (spendable.Total() < targetAmount)
        return tl::make_unexpected(ReportInvalidFunds(spendable, false, fee, 0, targetAmount, 0));
    else
        return {};
}

static tl::expected<std::pair<ResolvedPayment, CAmount>, InputSelectionError>
ResolveNetPayment(
        const CWallet& wallet,
        const ZTXOSelector& selector,
        const SpendableInputs& spendable,
        const NetAmountRecipient& netpay,
        const std::optional<CAmount>& fee,
        const TransactionStrategy& strategy,
        bool afterNU5,
        uint32_t consensusBranchId)
{
    bool canResolveOrchard = afterNU5 && !selector.SelectsSprout();
    CAmount maxSaplingAvailable = spendable.GetSaplingTotal();
    CAmount maxOrchardAvailable = spendable.GetOrchardTotal();
    uint32_t orchardOutputs{0};

    // We initially resolve the payment with `MINIMUM_FEE` so that we can use the payment to
    // calculate the actual fee.
    auto initialFee = fee.value_or(MINIMUM_FEE);
    return ValidateAmount(spendable, initialFee)
        .and_then([&](void) {
            // Needed so that the initial call to `ResolvePayment` (which is just a placeholder used
            // to help calculate the fee) doesn’t accidentally decrement funds.
            CAmount tempMaxSapling = maxSaplingAvailable;
            CAmount tempMaxOrchard = maxOrchardAvailable;
            return ResolvePayment(
                    Payment(netpay.first, spendable.Total() - initialFee, netpay.second),
                    canResolveOrchard,
                    strategy,
                    tempMaxSapling,
                    tempMaxOrchard,
                    orchardOutputs)
                .map_error([](const auto& error) -> InputSelectionError { return error; })
                .and_then([&](const auto& rpayment) {
                    auto finalFee = fee.value_or(CalcZIP317Fee(wallet, spendable, {rpayment}, std::nullopt, consensusBranchId));
                    return ValidateAmount(spendable, finalFee)
                        .and_then([&](void) {
                            return ResolvePayment(
                                    Payment(netpay.first, spendable.Total() - finalFee, netpay.second),
                                    canResolveOrchard,
                                    strategy,
                                    maxSaplingAvailable,
                                    maxOrchardAvailable,
                                    orchardOutputs)
                                .map([&](const auto& actualPayment) {
                                    return std::make_pair(actualPayment, finalFee);
                                })
                                .map_error([](const auto& error) -> InputSelectionError { return error; });
                        });
                });
        });
}

tl::expected<ChangeAddress, AddressResolutionError>
WalletTxBuilder::GetChangeAddress(
        CWallet& wallet,
        const ZTXOSelector& selector,
        const SpendableInputs& spendable,
        const Payments& resolvedPayments,
        const TransactionStrategy& strategy,
        bool afterNU5) const
{
    // Determine the account we're sending from.
    auto sendFromAccount = wallet.FindAccountForSelector(selector).value_or(ZCASH_LEGACY_ACCOUNT);

    auto getAllowedChangePools = [&](const std::set<ReceiverType>& receiverTypes) {
        std::set<OutputPool> result{resolvedPayments.GetRecipientPools()};
        // We always allow shielded change when not sending from the legacy account.
        if (sendFromAccount != ZCASH_LEGACY_ACCOUNT) {
            result.insert(OutputPool::Sapling);
        }
        for (ReceiverType rtype : receiverTypes) {
            switch (rtype) {
                case ReceiverType::P2PKH:
                case ReceiverType::P2SH:
                    if (strategy.AllowRevealedRecipients()) {
                        result.insert(OutputPool::Transparent);
                    }
                    break;
                case ReceiverType::Sapling:
                    if (!spendable.saplingNoteEntries.empty() || strategy.AllowRevealedAmounts()) {
                        result.insert(OutputPool::Sapling);
                    }
                    break;
                case ReceiverType::Orchard:
                    if (afterNU5
                        && (!spendable.orchardNoteMetadata.empty() || strategy.AllowRevealedAmounts())) {
                        result.insert(OutputPool::Orchard);
                    }
                    break;
            }
        }
        return result;
    };

    auto changeAddressForTransparentSelector = [&](const std::set<ReceiverType>& receiverTypes)
        -> tl::expected<ChangeAddress, AddressResolutionError> {
        auto addr = wallet.GenerateChangeAddressForAccount(
                sendFromAccount,
                getAllowedChangePools(receiverTypes));
        if (addr.has_value()) {
            return {addr.value()};
        } else {
            return tl::make_unexpected(AddressResolutionError::TransparentChangeNotAllowed);
        }
    };

    auto changeAddressForSaplingAddress = [&](const libzcash::SaplingPaymentAddress& addr)
        -> RecipientAddress {
        // for Sapling, if using a legacy address, return change to the
        // originating address; otherwise return it to the Sapling internal
        // address corresponding to the UFVK.
        if (sendFromAccount == ZCASH_LEGACY_ACCOUNT) {
            return addr;
        } else {
            auto addr = wallet.GenerateChangeAddressForAccount(
                    sendFromAccount,
                    getAllowedChangePools({ReceiverType::Sapling}));
            assert(addr.has_value());
            return addr.value();
        }
    };

    auto changeAddressForZUFVK = [&](
            const ZcashdUnifiedFullViewingKey& zufvk,
            const std::set<ReceiverType>& receiverTypes) {
        auto addr = zufvk.GetChangeAddress(getAllowedChangePools(receiverTypes));
        assert(addr.has_value());
        return addr.value();
    };

    return examine(selector.GetPattern(), match {
        [&](const CKeyID&) {
            return changeAddressForTransparentSelector({ReceiverType::P2PKH});
        },
        [&](const CScriptID&) {
            return changeAddressForTransparentSelector({ReceiverType::P2SH});
        },
        [](const libzcash::SproutPaymentAddress& addr)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            // for Sprout, we return change to the originating address using the tx builder.
            return addr;
        },
        [](const libzcash::SproutViewingKey& vk)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            // for Sprout, we return change to the originating address using the tx builder.
            return vk.address();
        },
        [&](const libzcash::SaplingPaymentAddress& addr)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            return changeAddressForSaplingAddress(addr);
        },
        [&](const libzcash::SaplingExtendedFullViewingKey& fvk)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            return changeAddressForSaplingAddress(fvk.DefaultAddress());
        },
        [&](const libzcash::UnifiedAddress& ua)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            auto zufvk = wallet.GetUFVKForAddress(ua);
            assert(zufvk.has_value());
            return changeAddressForZUFVK(zufvk.value(), ua.GetKnownReceiverTypes());
        },
        [&](const libzcash::UnifiedFullViewingKey& fvk)
            -> tl::expected<ChangeAddress, AddressResolutionError> {
            return changeAddressForZUFVK(
                    ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(params, fvk),
                    fvk.GetKnownReceiverTypes());
        },
        [&](const AccountZTXOPattern& acct) -> tl::expected<ChangeAddress, AddressResolutionError> {
            return wallet.GenerateChangeAddressForAccount(
                    acct.GetAccountId(),
                    getAllowedChangePools(acct.GetReceiverTypes()))
                .map_error([](auto err) {
                    switch (err) {
                        case AccountChangeAddressFailure::DisjointReceivers:
                            return AddressResolutionError::CouldNotResolveReceiver;
                        case AccountChangeAddressFailure::TransparentChangeNotPermitted:
                            return AddressResolutionError::TransparentChangeNotAllowed;
                        case AccountChangeAddressFailure::NoSuchAccount:
                            throw std::runtime_error("Selector account doesn’t exist.");
                    }
                });

        }
    });
}

tl::expected<TransactionEffects, InputSelectionError>
WalletTxBuilder::PrepareTransaction(
        CWallet& wallet,
        const ZTXOSelector& selector,
        const SpendableInputs& spendable,
        const Recipients& payments,
        const CChain& chain,
        const TransactionStrategy& strategy,
        const std::optional<CAmount>& fee,
        uint32_t anchorConfirmations) const
{
    if (fee.has_value()) {
        if (maxTxFee < fee.value()) {
            return tl::make_unexpected(MaxFeeError(fee.value()));
        } else if (!MoneyRange(fee.value())) {
            // TODO: This check will be obviated by #6579.
            return tl::make_unexpected(InvalidFeeError(fee.value()));
        }
    }

    auto consensus = params.GetConsensus();
    int anchorHeight = GetAnchorHeight(chain, anchorConfirmations);
    bool afterNU5 = consensus.NetworkUpgradeActive(anchorHeight, Consensus::UPGRADE_NU5);
    auto consensusBranchId = CurrentEpochBranchId(chain.Height(), consensus);
    auto selected = examine(payments, match {
            [&](const std::vector<Payment>& payments) {
                return ResolveInputsAndPayments(
                        wallet,
                        selector,
                        spendable,
                        payments,
                        strategy,
                        fee,
                        afterNU5,
                        consensusBranchId);
            },
            [&](const NetAmountRecipient& netRecipient) {
                return ResolveNetPayment(wallet, selector, spendable, netRecipient, fee, strategy, afterNU5, consensusBranchId)
                    .map([&](const auto& pair) {
                        const auto& [payment, finalFee] = pair;
                        return InputSelection(spendable, {{payment}}, finalFee, std::nullopt);
                    });
            },
        });
    return selected.map([&](const InputSelection& resolvedSelection) {
        auto ovks = SelectOVKs(wallet, selector, spendable);

        return TransactionEffects(
                anchorConfirmations,
                resolvedSelection.GetInputs(),
                resolvedSelection.GetPayments(),
                resolvedSelection.GetChangeAddress(),
                resolvedSelection.GetFee(),
                ovks.first,
                ovks.second,
                anchorHeight);
    });
}

const SpendableInputs& InputSelection::GetInputs() const {
    return inputs;
}

const Payments& InputSelection::GetPayments() const {
    return payments;
}

CAmount InputSelection::GetFee() const {
    return fee;
}

const std::optional<ChangeAddress> InputSelection::GetChangeAddress() const {
    return changeAddr;
}

CAmount WalletTxBuilder::DefaultDustThreshold() const {
    CKey secret{CKey::TestOnlyRandomKey(true)};
    CScript scriptPubKey = GetScriptForDestination(secret.GetPubKey().GetID());
    CTxOut txout(CAmount(1), scriptPubKey);
    return txout.GetDustThreshold();
}

SpendableInputs WalletTxBuilder::FindAllSpendableInputs(
        const CWallet& wallet,
        const ZTXOSelector& selector,
        int32_t minDepth) const
{
    LOCK2(cs_main, wallet.cs_wallet);
    return wallet.FindSpendableInputs(selector, minDepth, std::nullopt);
}

CAmount GetConstrainedFee(
        const CWallet& wallet,
        const std::optional<SpendableInputs>& inputs,
        const std::vector<ResolvedPayment>& payments,
        const std::optional<ChangeAddress>& changeAddr,
        uint32_t consensusBranchId)
{
    // We know that minRelayFee <= MINIMUM_FEE <= conventional_fee, so we can use an arbitrary
    // transaction size when constraining the fee, because we are guaranteed to already satisfy the
    // lower bound.
    constexpr unsigned int DUMMY_TX_SIZE = 1;

    return CWallet::ConstrainFee(
            CalcZIP317Fee(wallet, inputs, payments, changeAddr, consensusBranchId),
            DUMMY_TX_SIZE);
}

static tl::expected<void, InputSelectionError>
AddChangePayment(
        const SpendableInputs& spendable,
        Payments& resolvedPayments,
        const ChangeAddress& changeAddr,
        CAmount changeAmount,
        CAmount targetAmount)
{
    assert(changeAmount > 0);

    // When spending transparent coinbase outputs, all inputs must be fully consumed.
    if (spendable.HasTransparentCoinbase()) {
        return tl::make_unexpected(ChangeNotAllowedError(spendable.Total(), targetAmount));
    }

    examine(changeAddr, match {
        // TODO: Once we can add Sprout change to `resolvedPayments`, we don’t need to pass
        //       `changeAddr` around the rest of these functions.
        [](const libzcash::SproutPaymentAddress&) {},
        [](const libzcash::SproutViewingKey&) {},
        [&](const auto& sendTo) {
            resolvedPayments.AddPayment(
                    ResolvedPayment(std::nullopt, sendTo, changeAmount, std::nullopt, true));
        }
    });

    return {};
}

/// On the initial call, we haven’t yet selected inputs, so we assume the outputs dominate the
/// actions.
///
/// 1. calc fee using only resolvedPayments to set a lower bound on the actual fee
///    • this also needs to know which pool change is going to, so it can determine what the fee is
///      with change _if_ there is change
/// 2. iterate over LimitToAmount until the updated fee (now including spends) matches the expected
///    fee
tl::expected<
    std::tuple<SpendableInputs, CAmount, std::optional<ChangeAddress>>,
    InputSelectionError>
WalletTxBuilder::IterateLimit(
        CWallet& wallet,
        const ZTXOSelector& selector,
        const TransactionStrategy& strategy,
        CAmount sendAmount,
        CAmount dustThreshold,
        const SpendableInputs& spendable,
        Payments& resolved,
        bool afterNU5,
        uint32_t consensusBranchId) const
{
    SpendableInputs spendableMut;

    auto previousFee = MINIMUM_FEE;
    auto updatedFee = GetConstrainedFee(wallet, std::nullopt, resolved.GetResolvedPayments(), std::nullopt, consensusBranchId);
    // This is used to increase the target amount just enough (generally by 0 or 1) to force
    // selection of additional notes.
    CAmount bumpTargetAmount{0};
    std::optional<ChangeAddress> changeAddr;
    CAmount changeAmount{0};
    CAmount targetAmount{0};

    do {
        // NB: This makes a fresh copy so that we start from the full set of notes when we re-limit.
        spendableMut = spendable;

        targetAmount = sendAmount + updatedFee;

        // TODO: the set of recipient pools is not quite sufficient information here; we should
        // probably perform note selection at the same time as we're performing resolved payment
        // construction above.
        bool foundSufficientFunds =
            spendableMut.LimitToAmount(
                    targetAmount + bumpTargetAmount,
                    dustThreshold,
                    resolved.GetRecipientPools());
        changeAmount = spendableMut.Total() - targetAmount;
        if (foundSufficientFunds) {
            // Don’t want to generate a change address if we don’t need one (because it could be
            // fresh) and once we generate it, hold onto it. But we still don’t have a guarantee
            // that we won’t end up discarding it.
            if (changeAmount > 0 && !changeAddr.has_value()) {
                auto maybeChangeAddr = GetChangeAddress(
                        wallet,
                        selector,
                        spendableMut,
                        resolved,
                        strategy,
                        afterNU5);

                if (maybeChangeAddr.has_value()) {
                    changeAddr = maybeChangeAddr.value();
                } else {
                    return tl::make_unexpected(maybeChangeAddr.error());
                }
            }
            previousFee = updatedFee;
            updatedFee = GetConstrainedFee(
                    wallet,
                    spendableMut,
                    resolved.GetResolvedPayments(),
                    changeAmount > 0 ? changeAddr : std::nullopt,
                    consensusBranchId);
        } else {
            return tl::make_unexpected(
                    ReportInvalidFunds(
                            spendableMut,
                            bumpTargetAmount != 0,
                            previousFee,
                            dustThreshold,
                            targetAmount,
                            changeAmount));
        }
        // This happens when we have exactly `MARGINAL_FEE` change, then add a change output that
        // causes the conventional fee to consume that change, leaving us with no change, which then
        // lowers the fee.
        if (updatedFee < previousFee) {
            // Bump the updated fee so that we don’t exit the loop, but should force us to take an
            // extra note (or fail) in the next `LimitToAmount`.
            bumpTargetAmount = 1;
        }
    } while (updatedFee != previousFee);

    if (changeAmount > 0) {
        assert(changeAddr.has_value());

        auto changeRes =
            AddChangePayment(spendableMut, resolved, changeAddr.value(), changeAmount, targetAmount);
        if (!changeRes.has_value()) {
            return tl::make_unexpected(changeRes.error());
        }
    }

    return std::make_tuple(spendableMut, updatedFee, changeAddr);
}

tl::expected<InputSelection, InputSelectionError>
WalletTxBuilder::ResolveInputsAndPayments(
        CWallet& wallet,
        const ZTXOSelector& selector,
        const SpendableInputs& spendable,
        const std::vector<Payment>& payments,
        const TransactionStrategy& strategy,
        const std::optional<CAmount>& fee,
        bool afterNU5,
        uint32_t consensusBranchId) const
{
    LOCK2(cs_main, wallet.cs_wallet);

    // This is a simple greedy algorithm to attempt to preserve requested
    // transactional privacy while moving as much value to the most recent pool
    // as possible.  This will also perform opportunistic shielding if the
    // transaction strategy permits.

    CAmount maxSaplingAvailable = spendable.GetSaplingTotal();
    CAmount maxOrchardAvailable = spendable.GetOrchardTotal();
    uint32_t orchardOutputs{0};

    // we can only select Orchard addresses if we’re not sending from Sprout, since there is no tx
    // version where both Sprout and Orchard are valid.
    bool canResolveOrchard = afterNU5 && !selector.SelectsSprout();
    std::vector<ResolvedPayment> resolvedPayments;
    std::optional<AddressResolutionError> resolutionError;
    for (const auto& payment : payments) {
        auto res = ResolvePayment(payment, canResolveOrchard, strategy, maxSaplingAvailable, maxOrchardAvailable, orchardOutputs);
        res.map([&](const ResolvedPayment& rpayment) { resolvedPayments.emplace_back(rpayment); });
        if (!res.has_value()) {
            return tl::make_unexpected(res.error());
        }
    }
    auto resolved = Payments(resolvedPayments);

    if (orchardOutputs > this->maxOrchardActions) {
        return tl::make_unexpected(
                ExcessOrchardActionsError(
                        ActionSide::Output,
                        orchardOutputs,
                        this->maxOrchardActions));
    }

    // Set the dust threshold so that we can select enough inputs to avoid
    // creating dust change amounts.
    CAmount dustThreshold{this->DefaultDustThreshold()};

    // Determine the target totals
    CAmount sendAmount{0};
    for (const auto& payment : payments) {
        sendAmount += payment.GetAmount();
    }

    SpendableInputs spendableMut;
    CAmount finalFee;
    CAmount targetAmount;
    std::optional<ChangeAddress> changeAddr;
    if (fee.has_value()) {
        spendableMut = spendable;
        finalFee = fee.value();
        targetAmount = sendAmount + finalFee;
        // TODO: the set of recipient pools is not quite sufficient information here; we should
        // probably perform note selection at the same time as we're performing resolved payment
        // construction above.
        bool foundSufficientFunds = spendableMut.LimitToAmount(
                targetAmount,
                dustThreshold,
                resolved.GetRecipientPools());
        CAmount changeAmount{spendableMut.Total() - targetAmount};
        if (!foundSufficientFunds) {
            return tl::make_unexpected(
                    ReportInvalidFunds(
                            spendableMut,
                            false,
                            finalFee,
                            dustThreshold,
                            targetAmount,
                            changeAmount));
        }
        if (changeAmount > 0) {
            auto maybeChangeAddr = GetChangeAddress(
                    wallet,
                    selector,
                    spendableMut,
                    resolved,
                    strategy,
                    afterNU5);

            if (maybeChangeAddr.has_value()) {
                changeAddr = maybeChangeAddr.value();
            } else {
                return tl::make_unexpected(maybeChangeAddr.error());
            }

            // TODO: This duplicates the check in the `else` branch of the containing `if`. Until we
            //       can add Sprout change to `Payments` (#5660), we need to check this before
            //       adding the change payment. We can remove this check and make the later one
            //       unconditional once that’s fixed.
            auto conventionalFee =
                CalcZIP317Fee(wallet, spendableMut, resolved.GetResolvedPayments(), changeAddr, consensusBranchId);
            if (finalFee > WEIGHT_RATIO_CAP * conventionalFee) {
                return tl::make_unexpected(AbsurdFeeError(conventionalFee, finalFee));
            }
            auto changeRes =
                AddChangePayment(spendableMut, resolved, changeAddr.value(), changeAmount, targetAmount);
            if (!changeRes.has_value()) {
                return tl::make_unexpected(changeRes.error());
            }
        } else {
            auto conventionalFee =
                CalcZIP317Fee(wallet, spendableMut, resolved.GetResolvedPayments(), std::nullopt, consensusBranchId);
            if (finalFee > WEIGHT_RATIO_CAP * conventionalFee) {
                return tl::make_unexpected(AbsurdFeeError(resolved.Total(), finalFee));
            }
        }
    } else {
        auto limitResult = IterateLimit(wallet, selector, strategy, sendAmount, dustThreshold, spendable, resolved, afterNU5, consensusBranchId);
        if (limitResult.has_value()) {
            std::tie(spendableMut, finalFee, changeAddr) = limitResult.value();
            targetAmount = sendAmount + finalFee;
        } else {
            return tl::make_unexpected(limitResult.error());
        }
    }

    // When spending transparent coinbase outputs they may only be sent to shielded recipients.
    if (spendableMut.HasTransparentCoinbase() && resolved.HasTransparentRecipient()) {
        return tl::make_unexpected(AddressResolutionError::TransparentRecipientNotAllowed);
    }

    if (spendableMut.orchardNoteMetadata.size() > this->maxOrchardActions) {
        return tl::make_unexpected(
                ExcessOrchardActionsError(
                        ActionSide::Input,
                        spendableMut.orchardNoteMetadata.size(),
                        this->maxOrchardActions));
    }

    return InputSelection(spendableMut, resolved, finalFee, changeAddr);
}

std::pair<uint256, uint256>
GetOVKsForUFVK(const UnifiedFullViewingKey& ufvk, const SpendableInputs& spendable)
{
    if (!spendable.orchardNoteMetadata.empty()) {
        auto fvk = ufvk.GetOrchardKey();
        // Orchard notes will not have been selected if the UFVK does not contain an Orchard key.
        assert(fvk.has_value());
        return std::make_pair(
                fvk.value().ToInternalOutgoingViewingKey(),
                fvk.value().ToExternalOutgoingViewingKey());
    } else if (!spendable.saplingNoteEntries.empty()) {
        auto dfvk = ufvk.GetSaplingKey();
        // Sapling notes will not have been selected if the UFVK does not contain a Sapling key.
        assert(dfvk.has_value());
        return dfvk.value().GetOVKs();
    } else if (!spendable.utxos.empty()) {
        // Transparent UTXOs will not have been selected if the UFVK does not contain a transparent
        // key.
        auto tfvk = ufvk.GetTransparentKey();
        assert(tfvk.has_value());
        return tfvk.value().GetOVKsForShielding();
    } else {
        // This should be unreachable.
        throw std::runtime_error("No spendable inputs.");
    }
}

std::pair<uint256, uint256> WalletTxBuilder::SelectOVKs(
        const CWallet& wallet,
        const ZTXOSelector& selector,
        const SpendableInputs& spendable) const
{
    return examine(selector.GetPattern(), match {
        [&](const CKeyID& keyId) {
            return wallet.GetLegacyAccountKey().ToAccountPubKey().GetOVKsForShielding();
        },
        [&](const CScriptID& keyId) {
            return wallet.GetLegacyAccountKey().ToAccountPubKey().GetOVKsForShielding();
        },
        [&](const libzcash::SproutPaymentAddress&) {
            return wallet.GetLegacyAccountKey().ToAccountPubKey().GetOVKsForShielding();
        },
        [&](const libzcash::SproutViewingKey&) {
            return wallet.GetLegacyAccountKey().ToAccountPubKey().GetOVKsForShielding();
        },
        [&](const libzcash::SaplingPaymentAddress& addr) {
            libzcash::SaplingExtendedSpendingKey extsk;
            assert(wallet.GetSaplingExtendedSpendingKey(addr, extsk));
            return extsk.ToXFVK().GetOVKs();
        },
        [](const libzcash::SaplingExtendedFullViewingKey& sxfvk) {
            return sxfvk.GetOVKs();
        },
        [&](const UnifiedAddress& ua) {
            auto ufvk = wallet.GetUFVKForAddress(ua);
            // This is safe because spending key checks will have ensured that we have a UFVK
            // corresponding to this address.
            assert(ufvk.has_value());
            return GetOVKsForUFVK(ufvk.value().ToFullViewingKey(), spendable);
        },
        [&](const UnifiedFullViewingKey& ufvk) {
            return GetOVKsForUFVK(ufvk, spendable);
        },
        [&](const AccountZTXOPattern& acct) {
            if (acct.GetAccountId() == ZCASH_LEGACY_ACCOUNT) {
                return wallet.GetLegacyAccountKey().ToAccountPubKey().GetOVKsForShielding();
            } else {
                auto ufvk = wallet.GetUnifiedFullViewingKeyByAccount(acct.GetAccountId());
                // By definition, we have a UFVK for every known non-legacy account.
                assert(ufvk.has_value());
                return GetOVKsForUFVK(ufvk.value().ToFullViewingKey(), spendable);
            }
        },
    });
}

PrivacyPolicy TransactionEffects::GetRequiredPrivacyPolicy() const
{
    if (!spendable.utxos.empty()) {
        std::set<CTxDestination> receivedAddrs;
        for (const auto& utxo : spendable.utxos) {
            if (utxo.destination.has_value()) {
                receivedAddrs.insert(utxo.destination.value());
            } else {
                throw std::runtime_error("Can’t spend a multisig UTXO via WalletTxBuilder.");
            }
        }

        if (receivedAddrs.size() > 1) {
            if (payments.HasTransparentRecipient()) {
                return PrivacyPolicy::NoPrivacy;
            } else {
                return PrivacyPolicy::AllowLinkingAccountAddresses;
            }
        } else if (payments.HasTransparentRecipient()) {
            return PrivacyPolicy::AllowFullyTransparent;
        } else {
            return PrivacyPolicy::AllowRevealedSenders;
        }
    } else if (payments.HasTransparentRecipient()) {
        return PrivacyPolicy::AllowRevealedRecipients;
    } else if (!spendable.orchardNoteMetadata.empty() && payments.HasSaplingRecipient()
               || !spendable.saplingNoteEntries.empty() && payments.HasOrchardRecipient()
               || !spendable.sproutNoteEntries.empty() && payments.HasSaplingRecipient()) {
        // TODO: This should only trigger when there is a non-zero valueBalance.
        return PrivacyPolicy::AllowRevealedAmounts;
    } else {
        return PrivacyPolicy::FullPrivacy;
    }
}

bool TransactionEffects::InvolvesOrchard() const
{
    return spendable.GetOrchardTotal() > 0 || payments.HasOrchardRecipient();
}

TransactionBuilderResult TransactionEffects::ApproveAndBuild(
        const CChainParams& params,
        const CWallet& wallet,
        const CChain& chain,
        const TransactionStrategy& strategy) const
{
    auto requiredPrivacy = this->GetRequiredPrivacyPolicy();
    if (!strategy.IsCompatibleWith(requiredPrivacy)) {
        return TransactionBuilderResult(strprintf(
            "The specified privacy policy, %s, does not permit the creation of "
            "the requested transaction. Select %s to allow this transaction "
            "to be constructed.",
            strategy.PolicyName(),
            TransactionStrategy::ToString(requiredPrivacy)
            + (requiredPrivacy == PrivacyPolicy::NoPrivacy ? "" : " or weaker")));
    }

    int nextBlockHeight = chain.Height() + 1;

    // Allow Orchard recipients by setting an Orchard anchor.
    std::optional<uint256> orchardAnchor;
    if (spendable.sproutNoteEntries.empty()
        && (InvolvesOrchard() || nPreferredTxVersion > ZIP225_MIN_TX_VERSION)
        && this->anchorConfirmations > 0)
    {
        LOCK(cs_main);
        auto anchorBlockIndex = chain[this->anchorHeight];
        assert(anchorBlockIndex != nullptr);
        orchardAnchor = anchorBlockIndex->hashFinalOrchardRoot;
    }

    auto builder = TransactionBuilder(params, nextBlockHeight, orchardAnchor, &wallet);
    builder.SetFee(fee);

    // Track the total of notes that we've added to the builder. This
    // shouldn't strictly be necessary, given `spendable.LimitToAmount`
    CAmount totalSpend = 0;

    // Create Sapling outpoints
    std::vector<SaplingOutPoint> saplingOutPoints;
    std::vector<SaplingNote> saplingNotes;
    std::vector<SaplingExtendedSpendingKey> saplingKeys;

    for (const auto& t : spendable.saplingNoteEntries) {
        saplingOutPoints.push_back(t.op);
        saplingNotes.push_back(t.note);

        libzcash::SaplingExtendedSpendingKey saplingKey;
        assert(wallet.GetSaplingExtendedSpendingKey(t.address, saplingKey));
        saplingKeys.push_back(saplingKey);

        totalSpend += t.note.value();
    }

    // Fetch Sapling anchor and witnesses, and Orchard Merkle paths.
    uint256 saplingAnchor;
    std::vector<std::optional<SaplingWitness>> witnesses;
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> orchardSpendInfo;
    {
        LOCK(wallet.cs_wallet);
        if (!wallet.GetSaplingNoteWitnesses(
                    saplingOutPoints,
                    anchorConfirmations,
                    witnesses,
                    saplingAnchor)) {
            // This error should not appear once we're nAnchorConfirmations blocks past
            // Sapling activation.
            return TransactionBuilderResult("Insufficient Sapling witnesses.");
        }

        if (orchardAnchor.has_value()) {
            orchardSpendInfo = wallet.GetOrchardSpendInfo(
                    spendable.orchardNoteMetadata,
                    anchorConfirmations,
                    orchardAnchor.value());
        }
    }

    // Add Orchard spends
    for (size_t i = 0; i < orchardSpendInfo.size(); i++) {
        auto spendInfo = std::move(orchardSpendInfo[i]);
        if (!builder.AddOrchardSpend(
            std::move(spendInfo.first),
            std::move(spendInfo.second)))
        {
            return TransactionBuilderResult(
                strprintf("Failed to add Orchard note to transaction (check %s for details)", GetDebugLogPath())
            );
        } else {
            totalSpend += spendInfo.second.Value();
        }
    }

    // Add Sapling spends
    for (size_t i = 0; i < saplingNotes.size(); i++) {
        if (!witnesses[i]) {
            return TransactionBuilderResult(strprintf(
                "Missing witness for Sapling note at outpoint %s",
                spendable.saplingNoteEntries[i].op.ToString()
            ));
        }

        builder.AddSaplingSpend(saplingKeys[i], saplingNotes[i], witnesses[i].value());
    }

    // Add outputs
    for (const auto& r : payments.GetResolvedPayments()) {
        std::optional<TransactionBuilderResult> result;
        examine(r.address, match {
            [&](const CKeyID& keyId) {
                if (r.memo.has_value()) {
                    result = TransactionBuilderResult("Memos cannot be sent to transparent addresses.");
                } else {
                    builder.AddTransparentOutput(keyId, r.amount);
                }
            },
            [&](const CScriptID& scriptId) {
                if (r.memo.has_value()) {
                    result = TransactionBuilderResult("Memos cannot be sent to transparent addresses.");
                } else {
                    builder.AddTransparentOutput(scriptId, r.amount);
                }
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                builder.AddSaplingOutput(
                        r.isInternal ? internalOVK : externalOVK,
                        addr,
                        r.amount,
                        r.memo);
            },
            [&](const libzcash::OrchardRawAddress& addr) {
                builder.AddOrchardOutput(
                        r.isInternal ? internalOVK : externalOVK,
                        addr,
                        r.amount,
                        r.memo);
            },
        });
        if (result.has_value()) {
            return result.value();
        }
    }

    // Add transparent utxos
    for (const auto& out : spendable.utxos) {
        const CTxOut& txOut = out.tx->vout[out.i];
        builder.AddTransparentInput(COutPoint(out.tx->GetHash(), out.i), txOut.scriptPubKey, txOut.nValue);

        totalSpend += txOut.nValue;
    }

    // Find Sprout witnesses
    // When spending notes, take a snapshot of note witnesses and anchors as the treestate will
    // change upon arrival of new blocks which contain joinsplit transactions.  This is likely
    // to happen as creating a chained joinsplit transaction can take longer than the block interval.
    // So, we need to take locks on cs_main and wallet.cs_wallet so that the witnesses aren't
    // updated.
    //
    // TODO: these locks would ideally be shared for selection of Sapling anchors and witnesses
    // as well.
    std::vector<std::optional<SproutWitness>> vSproutWitnesses;
    {
        LOCK2(cs_main, wallet.cs_wallet);
        std::vector<JSOutPoint> vOutPoints;
        for (const auto& t : spendable.sproutNoteEntries) {
            vOutPoints.push_back(t.jsop);
        }

        // inputAnchor is not needed by builder.AddSproutInput as it is for Sapling.
        uint256 inputAnchor;
        if (!wallet.GetSproutNoteWitnesses(vOutPoints, anchorConfirmations, vSproutWitnesses, inputAnchor)) {
            // This error should not appear once we're nAnchorConfirmations blocks past
            // Sprout activation.
            return TransactionBuilderResult("Insufficient Sprout witnesses.");
        }
    }

    // Add Sprout spends
    for (int i = 0; i < spendable.sproutNoteEntries.size(); i++) {
        const auto& t = spendable.sproutNoteEntries[i];
        libzcash::SproutSpendingKey sk;
        assert(wallet.GetSproutSpendingKey(t.address, sk));

        builder.AddSproutInput(sk, t.note, vSproutWitnesses[i].value());

        totalSpend += t.note.value();
    }

    // TODO: We currently can’t store Sprout change in `Payments`, so we only validate the
    //       spend/output balance in the case that `TransactionBuilder` doesn’t need to
    //       (re)calculate the change. In future, we shouldn’t rely on `TransactionBuilder` ever
    //       calculating change. (#5660)
    if (changeAddr.has_value()) {
        examine(changeAddr.value(), match {
            [&](const SproutPaymentAddress& addr) {
                builder.SendChangeToSprout(addr);
            },
            [&](const RecipientAddress&) {
                assert(totalSpend == payments.Total() + fee);
            }
        });
    }

    // Build the transaction
    auto result = builder.Build();

    if (result.IsTx()) {
        auto minRelayFee =
            ::minRelayTxFee.GetFeeForRelay(
                    ::GetSerializeSize(result.GetTxOrThrow(), SER_NETWORK, PROTOCOL_VERSION));
        // This should only be possible if a user has provided an explicit fee.
        if (fee < minRelayFee) {
            return TransactionBuilderResult(
                    strprintf(
                            "Fee (%s) is below the minimum relay fee for this transaction (%s)",
                            DisplayMoney(fee),
                            DisplayMoney(minRelayFee)));
        }
    }

    return result;
}

// TODO: Lock Orchard notes (#6226)
void TransactionEffects::LockSpendable(CWallet& wallet) const
{
    LOCK2(cs_main, wallet.cs_wallet);
    for (auto utxo : spendable.utxos) {
        COutPoint outpt(utxo.tx->GetHash(), utxo.i);
        wallet.LockCoin(outpt);
    }
    for (auto note : spendable.sproutNoteEntries) {
        wallet.LockNote(note.jsop);
    }
    for (auto note : spendable.saplingNoteEntries) {
        wallet.LockNote(note.op);
    }
}

// TODO: Unlock Orchard notes (#6226)
void TransactionEffects::UnlockSpendable(CWallet& wallet) const
{
    LOCK2(cs_main, wallet.cs_wallet);
    for (auto utxo : spendable.utxos) {
        COutPoint outpt(utxo.tx->GetHash(), utxo.i);
        wallet.UnlockCoin(outpt);
    }
    for (auto note : spendable.sproutNoteEntries) {
        wallet.UnlockNote(note.jsop);
    }
    for (auto note : spendable.saplingNoteEntries) {
        wallet.UnlockNote(note.op);
    }
}
