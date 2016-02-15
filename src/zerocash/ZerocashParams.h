/** @file
 *****************************************************************************

 Declaration of interfaces for the class ZerocashParams.

 *****************************************************************************
 * @author     This file is part of libzerocash, developed by the Zerocash
 *             project and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#ifndef PARAMS_H_
#define PARAMS_H_

#include "Zerocash.h"
#include "libsnark/common/default_types/r1cs_ppzksnark_pp.hpp"
#include "libsnark/zk_proof_systems/ppzksnark/r1cs_ppzksnark/r1cs_ppzksnark.hpp"
#include "zerocash_pour_ppzksnark.hpp"

namespace libzerocash {

class ZerocashParams {

public:
    typedef default_r1cs_ppzksnark_pp zerocash_pp;

    ZerocashParams(
        const unsigned int tree_depth,
        zerocash_pour_keypair<ZerocashParams::zerocash_pp> *keypair
    );

    ZerocashParams(
        const unsigned int tree_depth,
        zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* p_pk_1,
        zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* p_vk_1
    );

    const zerocash_pour_proving_key<zerocash_pp>& getProvingKey();
    const zerocash_pour_verification_key<zerocash_pp>& getVerificationKey();
    int getTreeDepth();
    ~ZerocashParams();

    static const size_t numPourInputs = 2;
    static const size_t numPourOutputs = 2;

    static zerocash_pour_keypair<ZerocashParams::zerocash_pp> GenerateNewKeyPair(const unsigned int tree_depth);

    static void SaveProvingKeyToFile(const zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* p_pk_1, std::string path);
    static void SaveVerificationKeyToFile(const zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* p_vk_1, std::string path);
    static zerocash_pour_proving_key<ZerocashParams::zerocash_pp> LoadProvingKeyFromFile(std::string path, const unsigned int tree_depth);
    static zerocash_pour_verification_key<ZerocashParams::zerocash_pp> LoadVerificationKeyFromFile(std::string path, const unsigned int tree_depth);
private:
    int treeDepth;
    zerocash_pour_proving_key<ZerocashParams::zerocash_pp>* params_pk_v1;
    zerocash_pour_verification_key<ZerocashParams::zerocash_pp>* params_vk_v1;
};

} /* namespace libzerocash */

#endif /* PARAMS_H_ */
