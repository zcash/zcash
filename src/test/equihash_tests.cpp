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
    size_t cBitLen { n/(k+1) };
    crypto_generichash_blake2b_state state;
    EhInitialiseState(n, k, state);
    uint256 V = ArithToUint256(nonce);
    BOOST_TEST_MESSAGE("Running solver: n = " << n << ", k = " << k << ", I = " << I << ", V = " << V.GetHex());
    crypto_generichash_blake2b_update(&state, (unsigned char*)&I[0], I.size());
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());

    // First test the basic solver
    std::set<std::vector<uint32_t>> ret;
    std::function<bool(std::vector<unsigned char>)> validBlock =
            [&ret, cBitLen](std::vector<unsigned char> soln) {
        ret.insert(GetIndicesFromMinimal(soln, cBitLen));
        return false;
    };
    EhBasicSolveUncancellable(n, k, state, validBlock);
    BOOST_TEST_MESSAGE("[Basic] Number of solutions: " << ret.size());
    std::stringstream strm;
    PrintSolutions(strm, ret);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(ret == solns);

    // The optimised solver should have the exact same result
    std::set<std::vector<uint32_t>> retOpt;
    std::function<bool(std::vector<unsigned char>)> validBlockOpt =
            [&retOpt, cBitLen](std::vector<unsigned char> soln) {
        retOpt.insert(GetIndicesFromMinimal(soln, cBitLen));
        return false;
    };
    EhOptimisedSolveUncancellable(n, k, state, validBlockOpt);
    BOOST_TEST_MESSAGE("[Optimised] Number of solutions: " << retOpt.size());
    strm.str("");
    PrintSolutions(strm, retOpt);
    BOOST_TEST_MESSAGE(strm.str());
    BOOST_CHECK(retOpt == solns);
    BOOST_CHECK(retOpt == ret);
}

void TestEquihashValidator(unsigned int n, unsigned int k, const std::string &I, const arith_uint256 &nonce, std::vector<uint32_t> soln, bool expected) {
    size_t cBitLen { n/(k+1) };
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
    EhIsValidSolution(n, k, state, GetMinimalFromIndices(soln, cBitLen), isValid);
    BOOST_CHECK(isValid == expected);
}

BOOST_AUTO_TEST_CASE(solver_testvectors) {
    TestEquihashSolvers(96, 5, "block header", 0, {
  {976, 126621, 100174, 123328, 38477, 105390, 38834, 90500, 6411, 116489, 51107, 129167, 25557, 92292, 38525, 56514, 1110, 98024, 15426, 74455, 3185, 84007, 24328, 36473, 17427, 129451, 27556, 119967, 31704, 62448, 110460, 117894},
  {1008, 18280, 34711, 57439, 3903, 104059, 81195, 95931, 58336, 118687, 67931, 123026, 64235, 95595, 84355, 122946, 8131, 88988, 45130, 58986, 59899, 78278, 94769, 118158, 25569, 106598, 44224, 96285, 54009, 67246, 85039, 127667},
  {1278, 107636, 80519, 127719, 19716, 130440, 83752, 121810, 15337, 106305, 96940, 117036, 46903, 101115, 82294, 118709, 4915, 70826, 40826, 79883, 37902, 95324, 101092, 112254, 15536, 68760, 68493, 125640, 67620, 108562, 68035, 93430},
  {3976, 108868, 80426, 109742, 33354, 55962, 68338, 80112, 26648, 28006, 64679, 130709, 41182, 126811, 56563, 129040, 4013, 80357, 38063, 91241, 30768, 72264, 97338, 124455, 5607, 36901, 67672, 87377, 17841, 66985, 77087, 85291},
  {5970, 21862, 34861, 102517, 11849, 104563, 91620, 110653, 7619, 52100, 21162, 112513, 74964, 79553, 105558, 127256, 21905, 112672, 81803, 92086, 43695, 97911, 66587, 104119, 29017, 61613, 97690, 106345, 47428, 98460, 53655, 109002}
                });
    TestEquihashSolvers(96, 5, "block header", 1, {
  {1911, 96020, 94086, 96830, 7895, 51522, 56142, 62444, 15441, 100732, 48983, 64776, 27781, 85932, 101138, 114362, 4497, 14199, 36249, 41817, 23995, 93888, 35798, 96337, 5530, 82377, 66438, 85247, 39332, 78978, 83015, 123505}
                });
    TestEquihashSolvers(96, 5, "block header", 2, {
  {165, 27290, 87424, 123403, 5344, 35125, 49154, 108221, 8882, 90328, 77359, 92348, 54692, 81690, 115200, 121929, 18968, 122421, 32882, 128517, 56629, 88083, 88022, 102461, 35665, 62833, 95988, 114502, 39965, 119818, 45010, 94889}
                });
    TestEquihashSolvers(96, 5, "block header", 10, {
  {1855, 37525, 81472, 112062, 11831, 38873, 45382, 82417, 11571, 47965, 71385, 119369, 13049, 64810, 26995, 34659, 6423, 67533, 88972, 105540, 30672, 80244, 39493, 94598, 17858, 78496, 35376, 118645, 50186, 51838, 70421, 103703},
  {3671, 125813, 31502, 78587, 25500, 83138, 74685, 98796, 8873, 119842, 21142, 55332, 25571, 122204, 31433, 80719, 3955, 49477, 4225, 129562, 11837, 21530, 75841, 120644, 4653, 101217, 19230, 113175, 16322, 24384, 21271, 96965}
                });
    TestEquihashSolvers(96, 5, "block header", 11, {
  {2570, 20946, 61727, 130667, 16426, 62291, 107177, 112384, 18464, 125099, 120313, 127545, 35035, 73082, 118591, 120800, 13800, 32837, 23607, 86516, 17339, 114578, 22053, 85510, 14913, 42826, 25168, 121262, 33673, 114773, 77592, 83471}
                });

    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 0, {
  {3130, 83179, 30454, 107686, 71240, 88412, 109700, 114639, 10024, 32706, 38019, 113013, 18399, 92942, 21094, 112263, 4146, 30807, 10631, 73192, 22216, 90216, 45581, 125042, 11256, 119455, 93603, 110112, 59851, 91545, 97403, 111102},
  {3822, 35317, 47508, 119823, 37652, 117039, 69087, 72058, 13147, 111794, 65435, 124256, 22247, 66272, 30298, 108956, 13157, 109175, 37574, 50978, 31258, 91519, 52568, 107874, 14999, 103687, 27027, 109468, 36918, 109660, 42196, 100424}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1, {
  {2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
  {16700, 46276, 21232, 43153, 22398, 58511, 47922, 71816, 23370, 26222, 39248, 40137, 65375, 85794, 69749, 73259, 23599, 72821, 42250, 52383, 35267, 75893, 52152, 57181, 27137, 101117, 45804, 92838, 29548, 29574, 37737, 113624}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 2, {
  {6005, 59843, 55560, 70361, 39140, 77856, 44238, 57702, 32125, 121969, 108032, 116542, 37925, 75404, 48671, 111682, 6937, 93582, 53272, 77545, 13715, 40867, 73187, 77853, 7348, 70313, 24935, 24978, 25967, 41062, 58694, 110036}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 10, {
  {968, 90691, 70664, 112581, 17233, 79239, 66772, 92199, 27801, 44198, 58712, 122292, 28227, 126747, 70925, 118108, 2876, 76082, 39335, 113764, 26643, 60579, 50853, 70300, 19640, 31848, 28672, 87870, 33574, 50308, 40291, 61593},
  {1181, 61261, 75793, 96302, 36209, 113590, 79236, 108781, 8275, 106510, 11877, 74550, 45593, 80595, 71247, 95783, 2991, 99117, 56413, 71287, 10235, 68286, 22016, 104685, 51588, 53344, 56822, 63386, 63527, 75772, 93100, 108542},
  {2229, 30387, 14573, 115700, 20018, 124283, 84929, 91944, 26341, 64220, 69433, 82466, 29778, 101161, 59334, 79798, 2533, 104985, 50731, 111094, 10619, 80909, 15555, 119911, 29028, 42966, 51958, 86784, 34561, 97709, 77126, 127250},
  {15465, 59017, 93851, 112478, 24940, 128791, 26154, 107289, 24050, 78626, 51948, 111573, 35117, 113754, 36317, 67606, 21508, 91486, 28293, 126983, 23989, 39722, 60567, 97243, 26720, 56243, 60444, 107530, 40329, 56467, 91943, 93737}
                });
    TestEquihashSolvers(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 11, {
  {1120, 77433, 58243, 76860, 11411, 96068, 13150, 35878, 15049, 88928, 20101, 104706, 29215, 73328, 39498, 83529, 9233, 124174, 66731, 97423, 10823, 92444, 25647, 127742, 12207, 46292, 22018, 120758, 14411, 46485, 21828, 57591}
                });

    TestEquihashSolvers(96, 5, "Test case with 3+-way collision in the final round.", 0x07f0, {
  {1162, 129543, 57488, 82745, 18311, 115612, 20603, 112899, 5635, 103373, 101651, 125986, 52160, 70847, 65152, 101720, 5810, 43165, 64589, 105333, 11347, 63836, 55495, 96392, 40767, 81019, 53976, 94184, 41650, 114374, 45109, 57038},
  {2321, 121781, 36792, 51959, 21685, 67596, 27992, 59307, 13462, 118550, 37537, 55849, 48994, 58515, 78703, 100100, 11189, 98120, 45242, 116128, 33260, 47351, 61550, 116649, 11927, 20590, 35907, 107966, 28779, 57407, 54793, 104108},
  {2321, 121781, 36792, 51959, 21685, 67596, 27992, 59307, 13462, 118550, 37537, 55849, 48994, 78703, 58515, 100100, 11189, 98120, 45242, 116128, 33260, 47351, 61550, 116649, 11927, 20590, 35907, 107966, 28779, 57407, 54793, 104108},
  {2321, 121781, 36792, 51959, 21685, 67596, 27992, 59307, 13462, 118550, 37537, 55849, 48994, 100100, 58515, 78703, 11189, 98120, 45242, 116128, 33260, 47351, 61550, 116649, 11927, 20590, 35907, 107966, 28779, 57407, 54793, 104108},
  {4488, 83544, 24912, 62564, 43206, 62790, 68462, 125162, 6805, 8886, 46937, 54588, 15509, 126232, 19426, 27845, 5959, 56839, 38806, 102580, 11255, 63258, 23442, 39750, 13022, 22271, 24110, 52077, 17422, 124996, 35725, 101509},
  {8144, 33053, 33933, 77498, 21356, 110495, 42805, 116575, 27360, 48574, 100682, 102629, 50754, 64608, 96899, 120978, 11924, 74422, 49240, 106822, 12787, 68290, 44314, 50005, 38056, 49716, 83299, 95307, 41798, 82309, 94504, 96161}
                });
}

BOOST_AUTO_TEST_CASE(validator_testvectors) {
    // Original valid solution
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
                true);
    // Change one index
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2262, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
                false);
    // Swap two arbitrary indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {45858, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 2261, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
                false);
    // Reverse the first pair of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {15185, 2261, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
                false);
    // Swap the first and second pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {36112, 104243, 2261, 15185, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568},
                false);
    // Swap the second-to-last and last pairs of indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 104973, 122568, 69567, 114474},
                false);
    // Swap the first half and second half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {15972, 115059, 85191, 90330, 68190, 122819, 81830, 91132, 23460, 49807, 52426, 80391, 69567, 114474, 104973, 122568, 2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026},
                false);
    // Sort the indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2261, 15185, 15972, 23460, 23779, 32642, 36112, 45858, 49807, 52426, 68190, 69567, 69878, 76925, 80080, 80391, 81830, 85191, 90330, 91132, 92842, 104243, 104973, 111026, 114474, 115059, 116805, 118332, 118390, 122568, 122819, 130041},
                false);
    // Duplicate indices
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2261, 2261, 15185, 15185, 36112, 36112, 104243, 104243, 23779, 23779, 118390, 118390, 118332, 118332, 130041, 130041, 32642, 32642, 69878, 69878, 76925, 76925, 80080, 80080, 45858, 45858, 116805, 116805, 92842, 92842, 111026, 111026},
                false);
    // Duplicate first half
    TestEquihashValidator(96, 5, "Equihash is an asymmetric PoW based on the Generalised Birthday problem.", 1,
  {2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026, 2261, 15185, 36112, 104243, 23779, 118390, 118332, 130041, 32642, 69878, 76925, 80080, 45858, 116805, 92842, 111026},
                false);
}

BOOST_AUTO_TEST_SUITE_END()
