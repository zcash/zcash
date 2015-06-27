/**
* @file       Zerocash.h
*
* @brief      Exceptions and constants for Zerocash
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       April 2014
*
* @copyright  Copyright 2014 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/

#ifndef ZEROCASH_H_
#define ZEROCASH_H_

#include <stdexcept>
#include <vector>
#include "libzerocash/pour_ppzksnark/hashes/hash_io.hpp"

#define ZEROCASH_VERSION_STRING             "0.1"
#define ZEROCASH_VERSION_INT				1
#define ZEROCASH_PROTOCOL_VERSION           "1"
#define ZEROCASH_DEFAULT_TREE_SIZE          64

#define a_pk_size       32
#define pk_enc_size     311
#define sig_pk_size		32
#define addr_pk_size    a_pk_size+pk_enc_size

#define a_sk_size       32
#define sk_enc_size     287
#define addr_sk_size    a_sk_size+sk_enc_size

#define v_size          8
#define rho_size        32
#define zc_r_size       48 // renaming to avoid conflict with BOOST names
#define s_size          0
#define k_size          32
#define cm_size         32
#define coin_size       addr_pk_size+v_size+rho_size+r_size+s_size+cm_size
#define tx_mint_size    cm_size+v_size+k_size+s_size

#define root_size       32
#define sn_size         32
#define pk_sig_size     66
#define h_size          32
#define pour_proof_size 288
#define C_size          173
#define sigma_size      72
#define tx_pour_size    root_size+(2*sn_size)+(2*cm_size)+v_size+pk_sig_size+(2*h_size)+pour_proof_size+(2*C_size)+sigma_size

#define SNARK

typedef std::vector<unsigned char> MerkleRootType;

// Errors thrown by the Zerocash library

class ZerocashException : public std::runtime_error
{
public:
	explicit ZerocashException(const std::string& str) : std::runtime_error(str) {}
};

namespace libzerocash {
    using namespace libsnark; // TODO: get rid of this hack.
};

#include "utils/util.h"

#endif /* ZEROCASH_H_ */
