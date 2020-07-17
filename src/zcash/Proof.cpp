#include "Proof.hpp"

#include "crypto/common.h"

#include <boost/static_assert.hpp>
#include <mutex>

namespace libzcash {

ProofVerifier ProofVerifier::Strict() {
    return ProofVerifier(true);
}

ProofVerifier ProofVerifier::Disabled() {
    return ProofVerifier(false);
}

}
