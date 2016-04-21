/** @file
 *****************************************************************************

 Declaration of exceptions and constants for Zerocash.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef ZEROCASH_H_
#define ZEROCASH_H_

#include <stdexcept>
#include <vector>

/* The version of this library. */
#define ZEROCASH_VERSION_STRING             "0.1"
#define ZEROCASH_VERSION_INT				1
#define ZEROCASH_PROTOCOL_VERSION           "1"
#define ZEROCASH_DEFAULT_TREE_SIZE          64

#define ZC_A_PK_SIZE       32
#define ZC_PK_ENC_SIZE     311
#define ZC_SIG_PK_SIZE		32
#define ZC_ADDR_PK_SIZE    (ZC_A_PK_SIZE+ZC_PK_ENC_SIZE)

#define ZC_A_SK_SIZE       32
#define ZC_SK_ENC_SIZE     287
#define ZC_ADDR_SK_SIZE    (ZC_A_SK_SIZE+ZC_SK_ENC_SIZE)

#define ZC_V_SIZE          8
#define ZC_RHO_SIZE        32
#define ZC_R_SIZE          48
#define ZC_MEMO_SIZE       128
#define ZC_S_SIZE          0
#define ZC_K_SIZE          32
#define ZC_CM_SIZE         32
#define ZC_COIN_SIZE       (ZC_ADDR_PK_SIZE+ZC_V_SIZE+ZC_RHO_SIZE+ZC_R_SIZE+ZC_S_SIZE+ZC_CM_SIZE)
#define ZC_TX_MINT_SIZE    (ZC_CM_SIZE+ZC_V_SIZE+ZC_K_SIZE+ZC_S_SIZE)

#define ZC_ROOT_SIZE       32
#define ZC_SN_SIZE         32
#define ZC_PK_SIG_SIZE     66
#define ZC_H_SIZE          32
#define ZC_POUR_PROOF_SIZE 288
#define ZC_C_SIZE          173
#define ZC_SIGMA_SIZE      72
#define ZC_TX_POUR_SIZE    (ZC_ROOT_SIZE+(2*ZC_SN_SIZE)+(2*ZC_CM_SIZE)+ZC_V_SIZE+ZC_PK_SIG_SIZE+(2*ZC_H_SIZE)+ZC_POUR_PROOF_SIZE+(2*ZC_C_SIZE)+ZC_SIGMA_SIZE)

#define SNARK

typedef std::vector<unsigned char> MerkleRootType;

namespace libsnark {
};

namespace libzerocash {
    using namespace libsnark;
};

#include "zerocash/utils/util.h"

#endif /* ZEROCASH_H_ */
