// Copyright (c) 2022 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#include "wallet/wallet_tx_builder.h"

using namespace libzcash;

int GetAnchorHeight(const CChain& chain, int anchorConfirmations)
{
    int nextBlockHeight = chain.Height() + 1;
    return nextBlockHeight - anchorConfirmations;
}

typedef std::variant<
    std::optional<ChangeAddress>,
    InputSelectionError> PossiblyChangeAddress;

PrepareTransactionResult WalletTxBuilder::PrepareTransaction(
        const CChainParams& params,
        const ZTXOSelector& selector,
        SpendableInputs& spendable,
        const std::vector<Payment>& payments,
        const CChain& chain,
        TransactionStrategy strategy,
        CAmount fee,
        uint32_t anchorConfirmations) const
{
    assert(fee < MAX_MONEY);

    bool isFromUa = std::holds_alternative<libzcash::UnifiedAddress>(selector.GetPattern());
    auto selected = ResolveInputsAndPayments(params, isFromUa, spendable, payments, chain, strategy, fee, anchorConfirmations);

    if (std::holds_alternative<InputSelectionError>(selected)) {
        return std::get<InputSelectionError>(selected);
    }

    auto resolvedSelection = std::get<InputSelection>(selected);
    auto resolvedPayments = resolvedSelection.GetPayments();

    // Determine the account we're sending from.
    auto sendFromAccount = wallet.FindAccountForSelector(selector).value_or(ZCASH_LEGACY_ACCOUNT);

    // We do not set a change address if there is no change.
    PossiblyChangeAddress changeAddr;
    auto changeAmount = spendable.Total() - resolvedPayments.Total() - fee;
    if (changeAmount > 0) {
        auto allowedChangeTypes = [&](const std::set<ReceiverType>& receiverTypes) -> std::set<OutputPool> {
            std::set<OutputPool> result{resolvedPayments.GetRecipientPools()};
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
                        int anchorHeight = GetAnchorHeight(chain, anchorConfirmations);
                        if (params.GetConsensus().NetworkUpgradeActive(anchorHeight, Consensus::UPGRADE_NU5)
                            && (!spendable.orchardNoteMetadata.empty() || strategy.AllowRevealedAmounts())) {
                            result.insert(OutputPool::Orchard);
                        }
                        break;
                }
            }
            return result;
        };

        changeAddr =
            std::visit(match {
                [&](const CKeyID& keyId) -> PossiblyChangeAddress {
                    auto sendTo = pwalletMain->GenerateChangeAddressForAccount(
                            sendFromAccount,
                            allowedChangeTypes({ReceiverType::P2PKH}));
                    if (sendTo.has_value()) {
                        resolvedPayments.AddPayment(
                                ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                        return sendTo.value();
                    } else {
                        return std::pair(std::set{AddressResolutionError::ChangeAddressSelectionError},
                                         PrivacyPolicy::AllowFullyTransparent);
                    }
                },
                [&](const CScriptID& scriptId) -> PossiblyChangeAddress {
                    auto sendTo = pwalletMain->GenerateChangeAddressForAccount(
                            sendFromAccount,
                            allowedChangeTypes({ReceiverType::P2SH}));
                    if (sendTo.has_value()) {
                        resolvedPayments.AddPayment(
                                ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                        return sendTo.value();
                    } else {
                        return std::pair(std::set{AddressResolutionError::ChangeAddressSelectionError},
                                         PrivacyPolicy::AllowFullyTransparent);
                    }
                },
                [](const libzcash::SproutPaymentAddress& addr) -> PossiblyChangeAddress {
                    // for Sprout, we return change to the originating address using the tx builder.
                    return addr;
                },
                [](const libzcash::SproutViewingKey& vk) -> PossiblyChangeAddress {
                    // for Sprout, we return change to the originating address using the tx builder.
                    return vk.address();
                },
                [&](const libzcash::SaplingPaymentAddress& addr) -> PossiblyChangeAddress {
                    // for Sapling, if using a legacy address, return change to the
                    // originating address; otherwise return it to the Sapling internal
                    // address corresponding to the UFVK.
                    std::optional<RecipientAddress> sendTo;
                    if (sendFromAccount == ZCASH_LEGACY_ACCOUNT) {
                        sendTo = addr;
                    } else {
                        sendTo = pwalletMain->GenerateChangeAddressForAccount(
                                sendFromAccount,
                                allowedChangeTypes({ReceiverType::Sapling}));
                    }
                    assert(sendTo.has_value());
                    resolvedPayments.AddPayment(
                            ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                    return sendTo.value();
                },
                [&](const libzcash::SaplingExtendedFullViewingKey& fvk) -> PossiblyChangeAddress {
                    // for Sapling, if using a legacy address, return change to the
                    // originating address; otherwise return it to the Sapling internal
                    // address corresponding to the UFVK.
                    std::optional<RecipientAddress> sendTo;
                    if (sendFromAccount == ZCASH_LEGACY_ACCOUNT) {
                        sendTo = fvk.DefaultAddress();
                    } else {
                        sendTo = pwalletMain->GenerateChangeAddressForAccount(
                                sendFromAccount,
                                allowedChangeTypes({ReceiverType::Sapling}));
                    }
                    assert(sendTo.has_value());
                    resolvedPayments.AddPayment(
                            ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                    return sendTo.value();
                },
                [&](const libzcash::UnifiedAddress& ua) -> PossiblyChangeAddress {
                    auto zufvk = pwalletMain->GetUFVKForAddress(ua);
                    if (zufvk.has_value()) {
                        auto sendTo = zufvk.value().GetChangeAddress(
                                allowedChangeTypes(ua.GetKnownReceiverTypes()));
                        if (sendTo.has_value()) {
                            resolvedPayments.AddPayment(
                                    ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                            return sendTo.value();
                        } else {
                            return std::pair(std::set{AddressResolutionError::ChangeAddressSelectionError},
                                             PrivacyPolicy::AllowRevealedAmounts);
                        }
                    } else {
                        return std::pair(std::set{AddressResolutionError::ChangeAddressSelectionError},
                                         PrivacyPolicy::AllowRevealedAmounts);
                    }
                },
                [&](const libzcash::UnifiedFullViewingKey& fvk) -> PossiblyChangeAddress {
                    auto zufvk = ZcashdUnifiedFullViewingKey::FromUnifiedFullViewingKey(params, fvk);
                    auto sendTo = zufvk.GetChangeAddress(
                            allowedChangeTypes(fvk.GetKnownReceiverTypes()));
                    if (sendTo.has_value()) {
                        resolvedPayments.AddPayment(
                                ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                        return sendTo.value();
                    } else {
                        return std::pair(std::set{AddressResolutionError::ChangeAddressSelectionError},
                                         PrivacyPolicy::AllowRevealedAmounts);
                    }
                },
                [&](const AccountZTXOPattern& acct) -> PossiblyChangeAddress {
                    auto sendTo = pwalletMain->GenerateChangeAddressForAccount(
                            acct.GetAccountId(),
                            allowedChangeTypes(acct.GetReceiverTypes()));
                    assert(sendTo.has_value());
                    resolvedPayments.AddPayment(
                            ResolvedPayment(std::nullopt, sendTo.value(), changeAmount, std::nullopt, true));
                    return sendTo.value();
                }
            }, selector.GetPattern());
    }

    auto ovks = SelectOVKs(selector, spendable);

    return
        std::visit(match {
            [&](const std::optional<ChangeAddress>& changeAddr) -> PrepareTransactionResult {
                auto effects = TransactionEffects(
                    sendFromAccount,
                    anchorConfirmations,
                    spendable,
                    resolvedPayments,
                    changeAddr,
                    fee,
                    ovks.first,
                    ovks.second,
                    resolvedSelection.GetOrchardAnchorHeight());

                effects.LockSpendable();

                return effects;
            },
            [](const InputSelectionError& error) -> PrepareTransactionResult {
                return error;
            }
        }, changeAddr);
}

Payments InputSelection::GetPayments() const {
    return this->payments;
}

int InputSelection::GetOrchardAnchorHeight() const {
    return this->orchardAnchorHeight;
}

CAmount WalletTxBuilder::DefaultDustThreshold() const {
    CKey secret{CKey::TestOnlyRandomKey(true)};
    CScript scriptPubKey = GetScriptForDestination(secret.GetPubKey().GetID());
    CTxOut txout(CAmount(1), scriptPubKey);
    return txout.GetDustThreshold(minRelayFee);
}

SpendableInputs WalletTxBuilder::FindAllSpendableInputs(
        const ZTXOSelector& selector,
        bool allowTransparentCoinbase,
        int32_t minDepth) const
{
    return wallet.FindSpendableInputs(selector, allowTransparentCoinbase, minDepth, std::nullopt);
}

bool WalletTxBuilder::AllowTransparentCoinbase(
        const std::vector<Payment>& payments,
        TransactionStrategy strategy)
{
    bool allowed = strategy.AllowRevealedSenders();
    for (const auto& payment : payments) {
        if (!allowed) break;
        std::visit(match {
            [&](const CKeyID& p2pkh) { allowed = false; },
            [&](const CScriptID& p2sh) { allowed = false; },
            [&](const SproutPaymentAddress& addr) { allowed = false; },
            [&](const SaplingPaymentAddress& addr) { },
            [&](const UnifiedAddress& ua) {
                allowed &= (ua.GetSaplingReceiver().has_value() || ua.GetOrchardReceiver().has_value());
            }
        }, payment.GetAddress());
    }
    return allowed;
}

InputSelectionResult WalletTxBuilder::ResolveInputsAndPayments(
        const CChainParams& params,
        bool isFromUa,
        SpendableInputs& spendableMut,
        const std::vector<Payment>& payments,
        const CChain& chain,
        TransactionStrategy strategy,
        CAmount fee,
        uint32_t anchorConfirmations) const
{
    LOCK2(cs_main, wallet.cs_wallet);

    // Determine the target totals
    CAmount sendAmount{0};
    for (const auto& payment : payments) {
        sendAmount += payment.GetAmount();
    }
    CAmount targetAmount = sendAmount + fee;

    // This is a simple greedy algorithm to attempt to preserve requested
    // transactional privacy while moving as much value to the most recent pool
    // as possible.  This will also perform opportunistic shielding if the
    // transaction strategy permits.

    CAmount maxSaplingAvailable = spendableMut.GetSaplingBalance();
    CAmount maxOrchardAvailable = spendableMut.GetOrchardBalance();
    uint32_t orchardOutputs{0};

    // we can only select Orchard addresses if there are sufficient non-Sprout
    // funds to cover the total payments + fee.
    bool canResolveOrchard = spendableMut.Total() - spendableMut.GetSproutBalance() >= targetAmount;
    std::vector<ResolvedPayment> resolvedPayments;
    std::set<AddressResolutionError> resolutionError;
    PrivacyPolicy maxPrivacy = PrivacyPolicy::FullPrivacy;
    int orchardAnchorHeight = GetAnchorHeight(chain, anchorConfirmations);
    for (const auto& payment : payments) {
        std::visit(match {
            [&](const CKeyID& p2pkh) {
                if (strategy.AllowRevealedRecipients()) {
                    resolvedPayments.emplace_back(
                            std::nullopt, p2pkh, payment.GetAmount(), payment.GetMemo(), payment.IsInternal());
                } else {
                    resolutionError.insert(AddressResolutionError::TransparentRecipientNotPermitted);
                    maxPrivacy = PrivacyPolicyMeet(maxPrivacy, PrivacyPolicy::AllowRevealedRecipients);
                }
            },
            [&](const CScriptID& p2sh) {
                if (strategy.AllowRevealedRecipients()) {
                    resolvedPayments.emplace_back(
                            std::nullopt, p2sh, payment.GetAmount(), payment.GetMemo(), payment.IsInternal());
                } else {
                    resolutionError.insert(AddressResolutionError::TransparentRecipientNotPermitted);
                    maxPrivacy = PrivacyPolicyMeet(maxPrivacy, PrivacyPolicy::AllowRevealedRecipients);
                }
            },
            [&](const SproutPaymentAddress& addr) {
                resolutionError.insert(AddressResolutionError::SproutRecipientNotPermitted);
            },
            [&](const SaplingPaymentAddress& addr) {
                if (strategy.AllowRevealedAmounts() || payment.GetAmount() < maxSaplingAvailable) {
                    resolvedPayments.emplace_back(
                            std::nullopt, addr, payment.GetAmount(), payment.GetMemo(), payment.IsInternal());
                    if (!strategy.AllowRevealedAmounts()) {
                        maxSaplingAvailable -= payment.GetAmount();
                    }
                } else {
                    resolutionError.insert(AddressResolutionError::InsufficientSaplingFunds);
                    maxPrivacy = PrivacyPolicyMeet(maxPrivacy, PrivacyPolicy::AllowRevealedAmounts);
                }
            },
            [&](const UnifiedAddress& ua) {
                bool resolved{false};
                if (params.GetConsensus().NetworkUpgradeActive(orchardAnchorHeight, Consensus::UPGRADE_NU5)
                    && canResolveOrchard
                    && ua.GetOrchardReceiver().has_value()
                    && (strategy.AllowRevealedAmounts() || payment.GetAmount() < maxOrchardAvailable)
                    ) {
                    resolvedPayments.emplace_back(
                        ua, ua.GetOrchardReceiver().value(), payment.GetAmount(), payment.GetMemo(), payment.IsInternal());
                    if (!strategy.AllowRevealedAmounts()) {
                        maxOrchardAvailable -= payment.GetAmount();
                    }
                    orchardOutputs += 1;
                    resolved = true;
                }

                if (!resolved && ua.GetSaplingReceiver().has_value()
                    && (strategy.AllowRevealedAmounts() || payment.GetAmount() < maxSaplingAvailable)
                    ) {
                    resolvedPayments.emplace_back(
                        ua, ua.GetSaplingReceiver().value(), payment.GetAmount(), payment.GetMemo(), payment.IsInternal());
                    if (!strategy.AllowRevealedAmounts()) {
                        maxSaplingAvailable -= payment.GetAmount();
                    }
                    resolved = true;
                }

                if (!resolved && ua.GetP2SHReceiver().has_value() && strategy.AllowRevealedRecipients()) {
                    resolvedPayments.emplace_back(
                        ua, ua.GetP2SHReceiver().value(), payment.GetAmount(), std::nullopt, payment.IsInternal());
                    resolved = true;
                }

                if (!resolved && ua.GetP2PKHReceiver().has_value() && strategy.AllowRevealedRecipients()) {
                    resolvedPayments.emplace_back(
                        ua, ua.GetP2PKHReceiver().value(), payment.GetAmount(), std::nullopt, payment.IsInternal());
                    resolved = true;
                }

                if (!resolved) {
                    resolutionError.insert(AddressResolutionError::UnifiedAddressResolutionError);
                    maxPrivacy = PrivacyPolicyMeet(maxPrivacy, PrivacyPolicy::AllowRevealedAmounts);
                }
            }
        }, payment.GetAddress());
    }

    auto resolved = Payments(resolvedPayments);

    if (spendableMut.GetTransparentBalance() && ! strategy.AllowRevealedSenders()) {
        resolutionError.insert(AddressResolutionError::TransparentSenderNotPermitted);
        maxPrivacy = PrivacyPolicyMeet(maxPrivacy, PrivacyPolicy::AllowRevealedSenders);
    }

    if (spendableMut.HasTransparentCoinbase() && resolved.HasTransparentRecipient()) {
        resolutionError.insert(AddressResolutionError::FullyTransparentCoinbaseNotPermitted);
    }

    if (!resolutionError.empty()) {
        return std::pair(resolutionError, maxPrivacy);
    }

    if (orchardOutputs > this->maxOrchardActions) {
        return ExcessOrchardActionsError(spendableMut.orchardNoteMetadata.size(), this->maxOrchardActions);
    }

    // Set the dust threshold so that we can select enough inputs to avoid
    // creating dust change amounts.
    CAmount dustThreshold{this->DefaultDustThreshold()};

    // TODO: the set of recipient pools is not quite sufficient information here; we should
    // probably perform note selection at the same time as we're performing resolved payment
    // construction above.
    if (!spendableMut.LimitToAmount(targetAmount, dustThreshold, resolved.GetRecipientPools())) {
        CAmount changeAmount{spendableMut.Total() - targetAmount};
        if (changeAmount > 0 && changeAmount < dustThreshold) {
            // TODO: we should provide the option for the caller to explicitly
            // forego change (definitionally an amount below the dust amount)
            // and send the extra to the recipient or the miner fee to avoid
            // creating dust change, rather than prohibit them from sending
            // entirely in this circumstance.
            // (Daira disagrees, as this could leak information to the recipient)
            return DustThresholdError(dustThreshold, spendableMut.Total(), changeAmount);
        } else {
            return InsufficientFundsError(spendableMut.Total(), targetAmount, isFromUa);
        }
    }

    // When spending transparent coinbase outputs, all inputs must be fully
    // consumed, and they may only be sent to shielded recipients.
    if (spendableMut.HasTransparentCoinbase() && spendableMut.Total() != targetAmount) {
        return ChangeNotAllowedError(spendableMut.Total(), targetAmount);
    }

    if (spendableMut.orchardNoteMetadata.size() > this->maxOrchardActions) {
        return ExcessOrchardActionsError(spendableMut.orchardNoteMetadata.size(), this->maxOrchardActions);
    }

    return InputSelection(resolved, orchardAnchorHeight);
}

std::pair<uint256, uint256> WalletTxBuilder::SelectOVKs(
        const ZTXOSelector& selector,
        const SpendableInputs& spendable) const
{
    uint256 internalOVK;
    uint256 externalOVK;
    if (!spendable.orchardNoteMetadata.empty()) {
        std::optional<OrchardFullViewingKey> fvk;
        std::visit(match {
            [&](const UnifiedAddress& ua) {
                auto ufvk = wallet.GetUFVKForAddress(ua);
                // This is safe because spending key checks will have ensured that we
                // have a UFVK corresponding to this address, and Orchard notes will
                // not have been selected if the UFVK does not contain an Orchard key.
                fvk = ufvk.value().GetOrchardKey().value();
            },
            [&](const UnifiedFullViewingKey& ufvk) {
                // Orchard notes will not have been selected if the UFVK does not contain
                // an Orchard key.
                fvk = ufvk.GetOrchardKey().value();
            },
            [&](const AccountZTXOPattern& acct) {
                // By definition, we have a UFVK for every known account.
                auto ufvk = wallet.GetUnifiedFullViewingKeyByAccount(acct.GetAccountId());
                // Orchard notes will not have been selected if the UFVK does not contain
                // an Orchard key.
                fvk = ufvk.value().GetOrchardKey().value();
            },
            [&](const auto& other) {
                throw std::runtime_error("SelectOVKs: Selector cannot select Orchard notes.");
            }
        }, selector.GetPattern());
        assert(fvk.has_value());

        internalOVK = fvk.value().ToInternalOutgoingViewingKey();
        externalOVK = fvk.value().ToExternalOutgoingViewingKey();
    } else if (!spendable.saplingNoteEntries.empty()) {
        std::optional<SaplingDiversifiableFullViewingKey> dfvk;
        std::visit(match {
            [&](const libzcash::SaplingPaymentAddress& addr) {
                libzcash::SaplingExtendedSpendingKey extsk;
                assert(pwalletMain->GetSaplingExtendedSpendingKey(addr, extsk));
                dfvk = extsk.ToXFVK();
            },
            [&](const UnifiedAddress& ua) {
                auto ufvk = pwalletMain->GetUFVKForAddress(ua);
                // This is safe because spending key checks will have ensured that we
                // have a UFVK corresponding to this address, and Sapling notes will
                // not have been selected if the UFVK does not contain a Sapling key.
                dfvk = ufvk.value().GetSaplingKey().value();
            },
            [&](const UnifiedFullViewingKey& ufvk) {
                // Sapling notes will not have been selected if the UFVK does not contain
                // a Sapling key.
                dfvk = ufvk.GetSaplingKey().value();
            },
            [&](const AccountZTXOPattern& acct) {
                // By definition, we have a UFVK for every known account.
                auto ufvk = pwalletMain->GetUnifiedFullViewingKeyByAccount(acct.GetAccountId());
                // Sapling notes will not have been selected if the UFVK does not contain
                // a Sapling key.
                dfvk = ufvk.value().GetSaplingKey().value();
            },
            [&](const auto& other) {
                throw std::runtime_error("SelectOVKs: Selector cannot select Sapling notes.");
            }
        }, selector.GetPattern());
        assert(dfvk.has_value());

        auto ovks = dfvk.value().GetOVKs();
        internalOVK = ovks.first;
        externalOVK = ovks.second;
    } else if (!spendable.utxos.empty()) {
        std::optional<transparent::AccountPubKey> tfvk;
        std::visit(match {
            [&](const CKeyID& keyId) {
                tfvk = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
            },
            [&](const CScriptID& keyId) {
                tfvk = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
            },
            [&](const UnifiedAddress& ua) {
                // This is safe because spending key checks will have ensured that we
                // have a UFVK corresponding to this address, and transparent UTXOs will
                // not have been selected if the UFVK does not contain a transparent key.
                auto ufvk = pwalletMain->GetUFVKForAddress(ua);
                tfvk = ufvk.value().GetTransparentKey().value();
            },
            [&](const UnifiedFullViewingKey& ufvk) {
                // Transparent UTXOs will not have been selected if the UFVK does not contain
                // a transparent key.
                tfvk = ufvk.GetTransparentKey().value();
            },
            [&](const AccountZTXOPattern& acct) {
                if (acct.GetAccountId() == ZCASH_LEGACY_ACCOUNT) {
                    tfvk = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
                } else {
                    // By definition, we have a UFVK for every known account.
                    auto ufvk = pwalletMain->GetUnifiedFullViewingKeyByAccount(acct.GetAccountId()).value();
                    // Transparent UTXOs will not have been selected if the UFVK does not contain
                    // a transparent key.
                    tfvk = ufvk.GetTransparentKey().value();
                }
            },
            [&](const auto& other) {
                throw std::runtime_error("SelectOVKs: Selector cannot select transparent UTXOs.");
            }
        }, selector.GetPattern());
        assert(tfvk.has_value());

        auto ovks = tfvk.value().GetOVKsForShielding();
        internalOVK = ovks.first;
        externalOVK = ovks.second;
    } else if (!spendable.sproutNoteEntries.empty()) {
        // use the legacy transparent account OVKs when sending from Sprout
        auto tfvk = pwalletMain->GetLegacyAccountKey().ToAccountPubKey();
        auto ovks = tfvk.GetOVKsForShielding();
        internalOVK = ovks.first;
        externalOVK = ovks.second;
    } else {
        // This should be unreachable; it is left in place as a guard to ensure
        // that when new input types are added to SpendableInputs in the future
        // that we do not accidentally return the all-zeros OVK.
        throw std::runtime_error("No spendable inputs.");
    }

    return std::make_pair(internalOVK, externalOVK);
}

bool TransactionEffects::InvolvesOrchard() const
{
    return spendable.GetOrchardBalance() > 0 || payments.HasOrchardRecipient();
}

TransactionBuilderResult TransactionEffects::ApproveAndBuild(
        const Consensus::Params& consensus,
        const CWallet& wallet,
        const CChain& chain) const
{
    int nextBlockHeight = chain.Height() + 1;

    // Allow Orchard recipients by setting an Orchard anchor.
    std::optional<uint256> orchardAnchor;
    if (spendable.sproutNoteEntries.empty()
        && (InvolvesOrchard() || nPreferredTxVersion > ZIP225_MIN_TX_VERSION)
        && this->anchorConfirmations > 0)
    {
        LOCK(cs_main);
        auto anchorBlockIndex = chain[this->orchardAnchorHeight];
        assert(anchorBlockIndex != nullptr);
        orchardAnchor = anchorBlockIndex->hashFinalOrchardRoot;
    }

    auto builder = TransactionBuilder(consensus, nextBlockHeight, orchardAnchor, &wallet);
    builder.SetFee(fee);

    // Track the total of notes that we've added to the builder. This
    // shouldn't strictly be necessary, given `spendable.LimitToAmount`
    CAmount sum = 0;
    CAmount targetAmount = payments.Total() + fee;

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

        sum += t.note.value();
        if (sum >= targetAmount) {
            break;
        }
    }

    // Fetch Sapling anchor and witnesses, and Orchard Merkle paths.
    uint256 anchor;
    std::vector<std::optional<SaplingWitness>> witnesses;
    std::vector<std::pair<libzcash::OrchardSpendingKey, orchard::SpendInfo>> orchardSpendInfo;
    {
        LOCK(wallet.cs_wallet);
        if (!wallet.GetSaplingNoteWitnesses(saplingOutPoints, anchorConfirmations, witnesses, anchor)) {
            UnlockSpendable();
            // This error should not appear once we're nAnchorConfirmations blocks past
            // Sapling activation.
            return TransactionBuilderResult(Sapling);
        }
        if (builder.GetOrchardAnchor().has_value()) {
            orchardSpendInfo = wallet.GetOrchardSpendInfo(spendable.orchardNoteMetadata, builder.GetOrchardAnchor().value());
        }
    }

    // Add Orchard spends
    for (size_t i = 0; i < orchardSpendInfo.size(); i++) {
        auto spendInfo = std::move(orchardSpendInfo[i]);
        if (!builder.AddOrchardSpend(
            std::move(spendInfo.first),
            std::move(spendInfo.second)))
        {
            UnlockSpendable();
            return TransactionBuilderResult(SimpleTransactionBuilderFailure::FailedToAddOrchardNote);
        }
    }

    // Add Sapling spends
    for (size_t i = 0; i < saplingNotes.size(); i++) {
        if (!witnesses[i]) {
            UnlockSpendable();
            return TransactionBuilderResult(spendable.saplingNoteEntries[i].op);
        }

        builder.AddSaplingSpend(saplingKeys[i].expsk, saplingNotes[i], anchor, witnesses[i].value());
    }

    // Add outputs
    for (const auto& r : payments.GetResolvedPayments()) {
        std::visit(match {
            [&](const CKeyID& keyId) {
                builder.AddTransparentOutput(keyId, r.amount);
            },
            [&](const CScriptID& scriptId) {
                builder.AddTransparentOutput(scriptId, r.amount);
            },
            [&](const libzcash::SaplingPaymentAddress& addr) {
                builder.AddSaplingOutput(
                        r.isInternal ? internalOVK : externalOVK, addr, r.amount,
                        r.memo.has_value() ? r.memo.value().ToBytes() : Memo::NoMemo().ToBytes());
            },
            [&](const libzcash::OrchardRawAddress& addr) {
                builder.AddOrchardOutput(
                        externalOVK, addr, r.amount,
                        r.memo.has_value() ? std::optional(r.memo.value().ToBytes()) : std::nullopt);
            }
        }, r.address);
    }

    // Add transparent utxos
    for (const auto& out : spendable.utxos) {
        const CTxOut& txOut = out.tx->vout[out.i];
        builder.AddTransparentInput(COutPoint(out.tx->GetHash(), out.i), txOut.scriptPubKey, txOut.nValue);

        sum += txOut.nValue;
        if (sum >= targetAmount) {
            break;
        }
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
            UnlockSpendable();
            // This error should not appear once we're nAnchorConfirmations blocks past
            // Sprout activation.
            return TransactionBuilderResult(Sprout);
        }
    }

    // Add Sprout spends
    for (int i = 0; i < spendable.sproutNoteEntries.size(); i++) {
        const auto& t = spendable.sproutNoteEntries[i];
        libzcash::SproutSpendingKey sk;
        assert(wallet.GetSproutSpendingKey(t.address, sk));

        builder.AddSproutInput(sk, t.note, vSproutWitnesses[i].value());

        sum += t.note.value();
        if (sum >= targetAmount) {
            break;
        }
    }

    if (changeAddr.has_value()) {
        std::visit(match {
            [&](const SproutPaymentAddress& addr) {
                builder.SendChangeToSprout(addr);
            },
            [&](const RecipientAddress& addr) {
                builder.SendChangeTo(addr, internalOVK);
            }
        }, changeAddr.value());
    }

    // Build the transaction
    auto result = builder.Build();
    if (result.IsError()) {
        UnlockSpendable();
    }
    return result;
}

// TODO: Lock Orchard notes
void TransactionEffects::LockSpendable() const
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : spendable.utxos) {
        COutPoint outpt(utxo.tx->GetHash(), utxo.i);
        pwalletMain->LockCoin(outpt);
    }
    for (auto note : spendable.sproutNoteEntries) {
        pwalletMain->LockNote(note.jsop);
    }
    for (auto note : spendable.saplingNoteEntries) {
        pwalletMain->LockNote(note.op);
    }
}

// TODO: Unlock Orchard notes
void TransactionEffects::UnlockSpendable() const
{
    LOCK2(cs_main, pwalletMain->cs_wallet);
    for (auto utxo : spendable.utxos) {
        COutPoint outpt(utxo.tx->GetHash(), utxo.i);
        pwalletMain->UnlockCoin(outpt);
    }
    for (auto note : spendable.sproutNoteEntries) {
        pwalletMain->UnlockNote(note.jsop);
    }
    for (auto note : spendable.saplingNoteEntries) {
        pwalletMain->UnlockNote(note.op);
    }
}
