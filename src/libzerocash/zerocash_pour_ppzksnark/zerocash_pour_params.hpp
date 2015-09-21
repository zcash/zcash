/** @file
 *****************************************************************************

 Declaration of various parameters used by the Pour gadget and Pour ppzkSNARK.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_POUR_PARAMS_HPP_
#define ZEROCASH_POUR_PARAMS_HPP_

namespace libzerocash {

const size_t sha256_block_len = 512;
const size_t sha256_digest_len = 256;
const size_t address_public_key_length = sha256_digest_len;
const size_t address_public_key_padding_length = 256;
const size_t address_secret_key_length = 256;
const size_t coin_commitment_length = sha256_digest_len;
const size_t coin_commitment_padding_length = 192;
const size_t truncated_coin_commitment_length = 128;
const size_t truncated_serial_number_length = 254;
const size_t serial_number_length = sha256_digest_len;
const size_t address_commitment_nonce_length = 384;
const size_t serial_number_nonce_length = 256;
const size_t coin_value_length = 64;
const size_t indexed_signature_public_key_hash_length = 254;

} // libzerocash

#endif // ZEROCASH_POUR_PARAMS_HPP_
