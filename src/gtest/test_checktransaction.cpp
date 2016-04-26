#include <gtest/gtest.h>

#include "main.h"
#include "primitives/transaction.h"
#include "consensus/validation.h"

TEST(checktransaction_tests, check_vpub_not_both_nonzero) {
    CMutableTransaction tx;
    tx.nVersion = 2;

    {
        // Ensure that values within the pour are well-formed.
        CMutableTransaction newTx(tx);
        CValidationState state;
        state.SetPerformPourVerification(false); // don't verify the snark

        newTx.vpour.push_back(CPourTx());

        CPourTx *pourtx = &newTx.vpour[0];
        pourtx->vpub_old = 1;
        pourtx->vpub_new = 1;

        EXPECT_FALSE(CheckTransaction(newTx, state));
        EXPECT_EQ(state.GetRejectReason(), "bad-txns-vpubs-both-nonzero");
    }
}
