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
    Equihash eh {n, k};
    crypto_generichash_blake2b_state state;
    eh.InitialiseState(state);
    uint256 V = ArithToUint256(nonce);
    BOOST_TEST_MESSAGE("Running solver: n = " << n << ", k = " << k << ", I = " << I << ", V = " << V.GetHex());
    crypto_generichash_blake2b_update(&state, (unsigned char*)&I[0], I.size());
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());

    // First test the basic solver
    std::set<std::vector<uint32_t>> ret = eh.BasicSolve(state);
    BOOST_TEST_MESSAGE("[Basic] Number of solutions: " << ret.size());
    std::stringstream strm;
    PrintSolutions(strm, ret);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(ret == solns);
}

void TestEquihashValidator(unsigned int n, unsigned int k, const std::string &I, const arith_uint256 &nonce, std::vector<uint32_t> soln, bool expected) {
    Equihash eh {n, k};
    crypto_generichash_blake2b_state state;
    eh.InitialiseState(state);
    uint256 V = ArithToUint256(nonce);
    crypto_generichash_blake2b_update(&state, (unsigned char*)&I[0], I.size());
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());
    BOOST_TEST_MESSAGE("Running validator: n = " << n << ", k = " << k << ", I = " << I << ", V = " << V.GetHex() << ", expected = " << expected << ", soln =");
    std::stringstream strm;
    PrintSolution(strm, soln);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(eh.IsValidSolution(state, soln) == expected);
}

BOOST_AUTO_TEST_CASE(solver_testvectors) {
    TestEquihashSolvers(96, 5, "block header", 0, {
  {182, 100500, 71010, 81262, 11318, 81082, 84339, 106327, 25622, 123074, 50681, 128728, 27919, 122921, 33794, 39634, 3948, 33776, 39058, 39177, 35372, 67678, 81195, 120032, 5452, 128944, 110158, 118138, 37893, 65666, 49222, 126229}
                });
    TestEquihashSolvers(96, 5, "block header", 1, {
  {1510, 43307, 63800, 74710, 37892, 71424, 63310, 110898, 2260, 70172, 12353, 35063, 13433, 71777, 35871, 80964, 14030, 50499, 35055, 77037, 41990, 79370, 72784, 99843, 16721, 125719, 127888, 131048, 85492, 126861, 89702, 129167},
  {1623, 18648, 8014, 121335, 5288, 33890, 35968, 74704, 2909, 53346, 41954, 48211, 68872, 110549, 110905, 113986, 20660, 119394, 30054, 37492, 23025, 110409, 55861, 65351, 45769, 128708, 82357, 124990, 76854, 130060, 99713, 119536}
                });
    TestEquihashSolvers(96, 5, "block header", 2, {
  {17611, 81207, 44397, 50188, 43411, 119224, 90094, 99790, 21704, 122576, 34295, 98391, 22200, 82614, 108526, 114425, 20019, 69354, 28160, 34999, 31902, 103318, 49332, 65015, 60702, 107535, 76891, 81801, 69559, 83079, 125721, 129893}
                });
    TestEquihashSolvers(96, 5, "block header", 11, {
                });

    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0, {
  {2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
  {2742, 14130, 3738, 38739, 60817, 92878, 102087, 102882, 7493, 114098, 11019, 96605, 53351, 65844, 92194, 111605, 12488, 21213, 93833, 103682, 74551, 80813, 93325, 109313, 24782, 124251, 39372, 50621, 35398, 90386, 66867, 79277}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1, {
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 2, {
  {2219, 49740, 102167, 108576, 15546, 73320, 29506, 94663, 13900, 74954, 16748, 35617, 42643, 58400, 60768, 63883, 4677, 111178, 35802, 120953, 21542, 89457, 97759, 128494, 24444, 99755, 97152, 108239, 39816, 92800, 85532, 88575},
  {2258, 41741, 8329, 74706, 8166, 80151, 31480, 86606, 5417, 79683, 97197, 100351, 18608, 61819, 65689, 79940, 13038, 28092, 21997, 62813, 22268, 119557, 58111, 63811, 45789, 72308, 50865, 81180, 91695, 127084, 93402, 95676},
  {3279, 96607, 78609, 102949, 32765, 54059, 79472, 96147, 25943, 36652, 47276, 71714, 26590, 29892, 44598, 58988, 12323, 42327, 60194, 87786, 60951, 103949, 71481, 81826, 13535, 88167, 17392, 74652, 21924, 64941, 54660, 72151},
  {8970, 81710, 78816, 97295, 22433, 83703, 59463, 101258, 9014, 75982, 102935, 111574, 27277, 30040, 54221, 107719, 18593, 89276, 94385, 119768, 34013, 63600, 46240, 87288, 46573, 80865, 47845, 67566, 92645, 121901, 102751, 104818}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 11, {
  {3298, 28759, 56287, 109050, 13166, 122018, 75757, 109249, 7616, 83872, 103256, 119576, 43182, 121748, 81417, 120122, 23405, 129542, 68426, 117326, 56427, 118027, 73904, 77697, 41334, 118772, 89089, 130655, 107174, 128610, 107577, 118332}
                });
}

BOOST_AUTO_TEST_CASE(validator_testvectors) {
    // Original valid solution
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
                true);
    // Change one index
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2141, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
                false);
    // Swap two arbitrary indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {76507, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 2140, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
                false);
    // Reverse the first pair of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {64888, 2140, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
                false);
    // Swap the first and second pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {7062, 37067, 2140, 64888, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191},
                false);
    // Swap the second-to-last and last pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 43030, 129191, 22472, 74552},
                false);
    // Swap the first half and second half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {5425, 22827, 37743, 119459, 37587, 118338, 39127, 40622, 16812, 26417, 112391, 120791, 22472, 74552, 43030, 129191, 2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869},
                false);
    // Sort the indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2140, 5425, 6685, 7062, 11292, 16812, 18151, 22472, 22827, 26417, 27641, 37067, 37587, 37743, 39127, 40622, 43030, 53514, 55608, 64888, 70723, 73669, 74552, 76507, 84243, 88834, 112391, 118338, 119459, 120791, 125869, 129191},
                false);
    // Duplicate indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2140, 2140, 64888, 64888, 7062, 7062, 37067, 37067, 11292, 11292, 27641, 27641, 53514, 53514, 70723, 70723, 6685, 6685, 73669, 73669, 18151, 18151, 88834, 88834, 55608, 55608, 76507, 76507, 84243, 84243, 125869, 125869},
                false);
    // Duplicate first half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0,
  {2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869, 2140, 64888, 7062, 37067, 11292, 27641, 53514, 70723, 6685, 73669, 18151, 88834, 55608, 76507, 84243, 125869},
                false);
}

BOOST_AUTO_TEST_SUITE_END()
