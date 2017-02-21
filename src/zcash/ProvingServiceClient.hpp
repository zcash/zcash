// Copyright (c) 2017 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zcash/JoinSplit.hpp"
#include "zcash/Proof.hpp"

#include <boost/optional.hpp>
#include <vector>

using namespace libzcash;

class ProvingServiceClient
{
private:
    std::string log_id;

    std::string zmqUrl;
    char zmqServerKey[41];
    char zmqPublicKey[41];
    char zmqSecretKey[41];

public:
    ProvingServiceClient() {}

    /**
     * Create a ProvingServiceClient with a random client keypair.
     */
    ProvingServiceClient(std::string id, char* url, char* serverKey);

    boost::optional<std::vector<ZCProof>> get_proofs(std::vector<ZCJSProofWitness> witnesses);
};
