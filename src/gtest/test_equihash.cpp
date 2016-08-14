#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "crypto/equihash.h"
#include "uint256.h"

void TestExpandAndCompress(const std::string &scope, size_t bit_len, size_t byte_pad,
                           std::vector<unsigned char> compact,
                           std::vector<unsigned char> expanded)
{
    SCOPED_TRACE(scope);

    std::vector<unsigned char> out(expanded.size());
    ExpandArray(compact.data(), compact.size(),
                out.data(), out.size(), bit_len, byte_pad);
    EXPECT_EQ(expanded, out);

    out.resize(compact.size());
    CompressArray(expanded.data(), expanded.size(),
                  out.data(), out.size(), bit_len, byte_pad);
    EXPECT_EQ(compact, out);
}

TEST(equihash_tests, expand_and_contract_arrays) {
    TestExpandAndCompress("8 11-bit chunks, all-ones", 11, 0,
                          ParseHex("ffffffffffffffffffffff"),
                          ParseHex("07ff07ff07ff07ff07ff07ff07ff07ff"));
    TestExpandAndCompress("8 21-bit chunks, alternating 1s and 0s", 21, 0,
                          ParseHex("aaaaad55556aaaab55555aaaaad55556aaaab55555"),
                          ParseHex("155555155555155555155555155555155555155555155555"));
    TestExpandAndCompress("16 14-bit chunks, alternating 11s and 00s", 14, 0,
                          ParseHex("cccf333cccf333cccf333cccf333cccf333cccf333cccf333cccf333"),
                          ParseHex("3333333333333333333333333333333333333333333333333333333333333333"));

    TestExpandAndCompress("8 11-bit chunks, all-ones, 2-byte padding", 11, 2,
                          ParseHex("ffffffffffffffffffffff"),
                          ParseHex("000007ff000007ff000007ff000007ff000007ff000007ff000007ff000007ff"));
}

TEST(equihash_tests, is_probably_duplicate) {
    std::shared_ptr<eh_trunc> p1 (new eh_trunc[4] {0, 1, 2, 3});
    std::shared_ptr<eh_trunc> p2 (new eh_trunc[4] {0, 1, 1, 3});
    std::shared_ptr<eh_trunc> p3 (new eh_trunc[4] {3, 1, 1, 3});

    ASSERT_FALSE(IsProbablyDuplicate<4>(p1, 4));
    ASSERT_FALSE(IsProbablyDuplicate<4>(p2, 4));
    ASSERT_TRUE(IsProbablyDuplicate<4>(p3, 4));
}

TEST(equihash_tests, check_basic_solver_cancelled) {
    Equihash<48,5> Eh48_5;
    crypto_generichash_blake2b_state state;
    Eh48_5.InitialiseState(state);
    uint256 V = uint256S("0x00");
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialEnd;
        }));
    }
}

TEST(equihash_tests, check_optimised_solver_cancelled) {
    Equihash<48,5> Eh48_5;
    crypto_generichash_blake2b_state state;
    Eh48_5.InitialiseState(state);
    uint256 V = uint256S("0x00");
    crypto_generichash_blake2b_update(&state, V.begin(), V.size());

    {
        ASSERT_NO_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<unsigned char> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialEnd;
        }), EhSolverCancelledException);
    }
}
