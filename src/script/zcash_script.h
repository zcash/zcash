// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_SCRIPT_ZCASH_SCRIPT_H
#define ZCASH_SCRIPT_ZCASH_SCRIPT_H

#include <stdint.h>

#if defined(BUILD_BITCOIN_INTERNAL) && defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
  #if defined(_WIN32)
    #if defined(DLL_EXPORT)
      #if defined(HAVE_FUNC_ATTRIBUTE_DLLEXPORT)
        #define EXPORT_SYMBOL __declspec(dllexport)
      #else
        #define EXPORT_SYMBOL
      #endif
    #endif
  #elif defined(HAVE_FUNC_ATTRIBUTE_VISIBILITY)
    #define EXPORT_SYMBOL __attribute__ ((visibility ("default")))
  #endif
#elif defined(MSC_VER) && !defined(STATIC_LIBZCASHCONSENSUS)
  #define EXPORT_SYMBOL __declspec(dllimport)
#endif

#ifndef EXPORT_SYMBOL
  #define EXPORT_SYMBOL
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ZCASH_SCRIPT_API_VER 3

typedef enum zcash_script_error_t
{
    zcash_script_ERR_OK = 0,
    zcash_script_ERR_TX_INDEX,
    zcash_script_ERR_TX_SIZE_MISMATCH,
    zcash_script_ERR_TX_DESERIALIZE,
    // Defined since API version 3.
    zcash_script_ERR_TX_VERSION,
} zcash_script_error;

/** Script verification flags */
enum
{
    zcash_script_SCRIPT_FLAGS_VERIFY_NONE                = 0,
    zcash_script_SCRIPT_FLAGS_VERIFY_P2SH                = (1U << 0), // evaluate P2SH (BIP16) subscripts
    zcash_script_SCRIPT_FLAGS_VERIFY_CHECKLOCKTIMEVERIFY = (1U << 9), // enable CHECKLOCKTIMEVERIFY (BIP65)
};

/// Deserializes the given transaction and precomputes values to improve
/// script verification performance.
///
/// This API cannot be used for v5+ transactions, and will return an error.
///
/// Returns a pointer to the precomputed transaction. Free this with
/// zcash_script_free_precomputed_tx once you are done.
/// The precomputed transaction is guaranteed to not keep a reference to any
/// part of the input buffers, so they can be safely overwritten or deallocated
/// after this function returns.
///
/// If not NULL, err will contain an error/success code for the operation.
void* zcash_script_new_precomputed_tx(
    const unsigned char* txTo,
    unsigned int txToLen,
    zcash_script_error* err);

/// Deserializes the given transaction and precomputes values to improve
/// script verification performance. Must be used for V5 transactions;
/// may also be used for previous versions.
///
/// allPrevOutputs must point to the encoding of the vector of all outputs
/// from previous transactions that are spent by the inputs of the given transaction.
/// The outputs must be encoded as specified by Bitcoin. The full encoding will thus
/// consist of a CompactSize prefix containing the number of outputs, followed by
/// the concatenated outputs, each of them encoded as (value, CompactSize, pk_script).
///
/// Returns a pointer to the precomputed transaction. Free this with
/// zcash_script_free_precomputed_tx once you are done.
///
/// If not NULL, err will contain an error/success code for the operation.
void* zcash_script_new_precomputed_tx_v5(
    const unsigned char* txTo,
    unsigned int txToLen,
    const unsigned char* allPrevOutputs,
    unsigned int allPrevOutputsLen,
    zcash_script_error* err);

/// Frees a precomputed transaction previously created with
/// zcash_script_new_precomputed_tx.
void zcash_script_free_precomputed_tx(void* preTx);

/// Returns 1 if the input nIn of the precomputed transaction pointed to by
/// preTx correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
///
/// If not NULL, err will contain an error/success code for the operation.
/// Note that script verification failure is indicated by err being set to
/// zcash_script_ERR_OK and a return value of 0.
EXPORT_SYMBOL int zcash_script_verify_precomputed(
    const void* preTx,
    unsigned int nIn,
    const unsigned char* scriptPubKey,
    unsigned int scriptPubKeyLen,
    int64_t amount,
    unsigned int flags,
    uint32_t consensusBranchId,
    zcash_script_error* err);

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the scriptPubKey pointed to by scriptPubKey under
/// the additional constraints specified by flags.
///
/// This API cannot be used for v5+ transactions, and will return an error.
///
/// If not NULL, err will contain an error/success code for the operation.
/// Note that script verification failure is indicated by err being set to
/// zcash_script_ERR_OK and a return value of 0.
EXPORT_SYMBOL int zcash_script_verify(
    const unsigned char *scriptPubKey, unsigned int scriptPubKeyLen,
    int64_t amount,
    const unsigned char *txTo, unsigned int txToLen,
    unsigned int nIn, unsigned int flags,
    uint32_t consensusBranchId,
    zcash_script_error* err);

/// Returns 1 if the input nIn of the serialized transaction pointed to by
/// txTo correctly spends the matching output in allPrevOutputs under
/// the additional constraints specified by flags. Must be used for V5 transactions;
/// may also be used for previous versions.
///
/// allPrevOutputs must point to the encoding of the vector of all outputs
/// from previous transactions that are spent by the inputs of the given transaction.
/// The outputs must be encoded as specified by Bitcoin. The full encoding will thus
/// consist of a CompactSize prefix containing the number of outputs, followed by
/// the concatenated outputs, each of them encoded as (value, CompactSize, pk_script).
///
/// If not NULL, err will contain an error/success code for the operation.
/// Note that script verification failure is indicated by err being set to
/// zcash_script_ERR_OK and a return value of 0.
EXPORT_SYMBOL int zcash_script_verify_v5(
    const unsigned char* txTo,
    unsigned int txToLen,
    const unsigned char* allPrevOutputs,
    unsigned int allPrevOutputsLen,
    unsigned int nIn,
    unsigned int flags,
    uint32_t consensusBranchId,
    zcash_script_error* err);

/// Returns the number of transparent signature operations in the
/// transparent inputs and outputs of the precomputed transaction
/// pointed to by preTx.
///
/// Returns UINT_MAX on error, so that invalid transactions don't pass the Zcash consensus rules.
/// If not NULL, err will contain an error/success code for the operation.
EXPORT_SYMBOL unsigned int zcash_script_legacy_sigop_count_precomputed(
    const void* preTx,
    zcash_script_error* err);

/// Returns the number of transparent signature operations in the
/// transparent inputs and outputs of the serialized transaction
/// pointed to by txTo.
///
/// Returns UINT_MAX on error.
/// If not NULL, err will contain an error/success code for the operation.
EXPORT_SYMBOL unsigned int zcash_script_legacy_sigop_count(
    const unsigned char *txTo,
    unsigned int txToLen,
    zcash_script_error* err);

/// Returns the current version of the zcash_script library.
EXPORT_SYMBOL unsigned int zcash_script_version();

#ifdef __cplusplus
} // extern "C"
#endif

#undef EXPORT_SYMBOL

#endif // ZCASH_SCRIPT_ZCASH_SCRIPT_H
