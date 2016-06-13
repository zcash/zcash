// Copyright (c) 2016 Jack Grigg
// Copyright (c) 2016 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "arith_uint256.h"
#include "crypto/sha256.h"
#include "crypto/equihash.h"
#include "test/test_bitcoin.h"
#include "uint256.h"

#include "sodium.h"

#include <sstream>
#include <set>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(equihash_tests, BasicTestingSetup)

void PrintSolution(std::stringstream &strm, std::vector<uint32_t> soln) {
    strm << "  {";
    const char* separator = "";
    for (uint32_t index : soln) {
        strm << separator << index;
        separator = ", ";
    }
    strm << "}";
}

void PrintSolutions(std::stringstream &strm, std::set<std::vector<uint32_t>> solns) {
    strm << "{";
    const char* soln_separator = "";
    for (std::vector<uint32_t> soln : solns) {
        strm << soln_separator << "\n";
        soln_separator = ",";
        PrintSolution(strm, soln);
    }
    strm << "\n}";
}

void TestEquihashSolvers(unsigned int n, unsigned int k, const std::string &I, const arith_uint256 &nonce, const std::set<std::vector<uint32_t>> &solns) {
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state);
    uint256 V = ArithToUint256(nonce);
    BOOST_TEST_MESSAGE("Running solver: n = " << n << ", k = " << k << ", I = " << I << ", V = " << V.GetHex());
    crypto_generichash_blake2b_update(&state, (unsigned char*)&I[0], I.size());
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());

    // First test the basic solver
    std::set<std::vector<uint32_t>> ret;
    EhBasicSolve(n, k, state, ret);
    BOOST_TEST_MESSAGE("[Basic] Number of solutions: " << ret.size());
    std::stringstream strm;
    PrintSolutions(strm, ret);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(ret == solns);

    // The optimised solver should have the exact same result
    std::set<std::vector<uint32_t>> retOpt;
    EhOptimisedSolve(n, k, state, retOpt);
    BOOST_TEST_MESSAGE("[Optimised] Number of solutions: " << retOpt.size());
    strm.str("");
    PrintSolutions(strm, retOpt);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(retOpt == solns);
    BOOST_CHECK(retOpt == ret);
}

void TestEquihashValidator(unsigned int n, unsigned int k, const std::string &I, const arith_uint256 &nonce, std::vector<uint32_t> soln, bool expected) {
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state);
    uint256 V = ArithToUint256(nonce);
    crypto_generichash_blake2b_update(&state, (unsigned char*)&I[0], I.size());
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());
    BOOST_TEST_MESSAGE("Running validator: n = " << n << ", k = " << k << ", I = " << I << ", V = " << V.GetHex() << ", expected = " << expected << ", soln =");
    std::stringstream strm;
    PrintSolution(strm, soln);
    BOOST_TEST_MESSAGE(strm.str());
    bool isValid;
    EhIsValidSolution(n, k, state, soln, isValid);
    BOOST_CHECK(isValid == expected);
}

BOOST_AUTO_TEST_CASE(solver_testvectors) {
    TestEquihashSolvers(96, 5, "block header", 0, {
                });
    TestEquihashSolvers(96, 5, "block header", 1, {
                });
    TestEquihashSolvers(96, 5, "block header", 2, {
  {3389, 110764, 37520, 58346, 4112, 61459, 47776, 84587, 11643, 34988, 36560, 98422, 36242, 47864, 76737, 80053, 3422, 74285, 77922, 101376, 58602, 104312, 64513, 89638, 10240, 76326, 27584, 36949, 43637, 75295, 56666, 91601}
                });
    TestEquihashSolvers(96, 5, "block header", 10, {
  {787, 20674, 53516, 73404, 4022, 110690, 35427, 58606, 22749, 129878, 34185, 112292, 56949, 100033, 100182, 115894, 13225, 23627, 94405, 114446, 14243, 118738, 36358, 79934, 49517, 78196, 85137, 85376, 57430, 77040, 102235, 114826},
  {2656, 33964, 2683, 87167, 19223, 113046, 67505, 101388, 12585, 77102, 18807, 117333, 70932, 106281, 85381, 118430, 6664, 12926, 6868, 33372, 15227, 128690, 89250, 96792, 14322, 23199, 32286, 57355, 54637, 130050, 70335, 99067},
  {4207, 21880, 85981, 113070, 16301, 41187, 88537, 103201, 6295, 86241, 21605, 56786, 28030, 80680, 52120, 79774, 7875, 56055, 25882, 112870, 9719, 40271, 35223, 50883, 27959, 92599, 70158, 106739, 31838, 117463, 69735, 83367},
  {9637, 51478, 44285, 93559, 76796, 108515, 123998, 124708, 17379, 29371, 21401, 48583, 62725, 80279, 109465, 111074, 16793, 128680, 42090, 42327, 34750, 101600, 64379, 84300, 48256, 49313, 82752, 87659, 67566, 117002, 78981, 122103}
                });
    TestEquihashSolvers(96, 5, "block header", 11, {
  {1638, 116919, 4749, 45156, 58749, 103900, 92294, 109359, 16076, 89395, 21938, 121398, 18847, 43685, 53116, 114427, 7067, 69901, 23179, 73689, 33890, 103453, 66168, 129978, 57522, 115912, 81791, 123826, 76090, 96629, 120289, 123662},
  {2957, 38313, 18116, 83967, 10458, 51007, 13244, 61860, 16311, 113118, 76034, 90819, 43134, 61561, 68365, 93667, 7626, 86183, 62381, 109415, 90075, 114836, 93702, 131024, 19175, 124662, 20036, 34896, 33427, 60491, 103672, 107450}
                });

    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0, {
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1, {
  {2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
  {3521, 83631, 86264, 106366, 62729, 102245, 74046, 114174, 45281, 59655, 45686, 60328, 71798, 123267, 83891, 121660, 12375, 83210, 94890, 120434, 35140, 109028, 65151, 89820, 18962, 24744, 55758, 116061, 63695, 125324, 98242, 125805}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 2, {
  {6310, 126030, 19266, 92728, 22993, 43617, 59500, 110969, 8633, 95173, 11769, 69347, 21455, 114538, 67360, 77234, 7538, 84336, 27001, 79803, 33408, 111870, 42328, 48938, 19045, 48081, 55314, 86688, 24992, 93296, 68568, 106618}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 10, {
  {6768, 10445, 80746, 128923, 28583, 50486, 47353, 58892, 35052, 45980, 61445, 103307, 67117, 94090, 78715, 109244, 20795, 102820, 31354, 91894, 50174, 126488, 77522, 80142, 28219, 74825, 66159, 73984, 60786, 121859, 70144, 120379},
  {7865, 119271, 33055, 103984, 19519, 65954, 36562, 123493, 10038, 60327, 10645, 98001, 10748, 108967, 73961, 99283, 20538, 21631, 41159, 81213, 71041, 74642, 97906, 107612, 47736, 74711, 75451, 117319, 53428, 73882, 73362, 125084}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 11, {
  {637, 78032, 97478, 118268, 16058, 44395, 19029, 39150, 1566, 66582, 4084, 107252, 59619, 116281, 67957, 128728, 30916, 69051, 90422, 102716, 51905, 66753, 60509, 78066, 38568, 119630, 75839, 113134, 54356, 70996, 63085, 83048},
  {4130, 71826, 46248, 50447, 4281, 129092, 23122, 103196, 9305, 34797, 111094, 127775, 82662, 120386, 109738, 124765, 24770, 125174, 83477, 102473, 45209, 79062, 84764, 125929, 31689, 95554, 66614, 127658, 31756, 55684, 53670, 53776}
                });
}

BOOST_AUTO_TEST_CASE(validator_testvectors) {
    // Original valid solution
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
                true);
    // Change one index
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2155, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
                false);
    // Swap two arbitrary indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {62307, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 2154, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
                false);
    // Reverse the first pair of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {87055, 2154, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
                false);
    // Swap the first and second pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {7922, 12920, 2154, 87055, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278},
                false);
    // Swap the second-to-last and last pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 126764, 128278, 68078, 129665},
                false);
    // Swap the first half and second half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {19673, 24483, 33984, 91500, 38171, 85505, 94625, 106140, 31530, 60861, 59391, 117337, 68078, 129665, 126764, 128278, 2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891},
                false);
    // Sort the indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2154, 2432, 3912, 7922, 10987, 12920, 19673, 24483, 31530, 33984, 38171, 45189, 48178, 48280, 49783, 59391, 60861, 62307, 67880, 68078, 85505, 87055, 91500, 93891, 94625, 106140, 117337, 122795, 124296, 126764, 128278, 129665},
                false);
    // Duplicate indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2154, 2154, 87055, 87055, 7922, 7922, 12920, 12920, 45189, 45189, 49783, 49783, 122795, 122795, 124296, 124296, 2432, 2432, 48178, 48178, 48280, 48280, 67880, 67880, 3912, 3912, 62307, 62307, 10987, 10987, 93891, 93891},
                false);
    // Duplicate first half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891, 2154, 87055, 7922, 12920, 45189, 49783, 122795, 124296, 2432, 48178, 48280, 67880, 3912, 62307, 10987, 93891},
                false);
}

BOOST_AUTO_TEST_SUITE_END()
