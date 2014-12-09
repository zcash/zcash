/** @file
 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_PARAMS_HPP_
#define ZEROCASH_PARAMS_HPP_

namespace libzerocash {

const size_t sha256_block_len = 512;
const size_t sha256_digest_len = 256;
const size_t addr_pk_len = sha256_digest_len;
const size_t addr_pk_pad_len = 256;
const size_t addr_sk_len = 256;
const size_t coincomm_len = sha256_digest_len;
const size_t coincomm_pad_len = 192;
const size_t coincomm_trunc = 128;
const size_t sn_trunc = 254;
const size_t h_trunc = 253;
const size_t sn_len = sha256_digest_len;
const size_t rand_len = 384;
const size_t nonce_len = 256;
const size_t value_len = 64;

} // libzerocash
#endif // ZEROCASH_PARAMS_HPP_
