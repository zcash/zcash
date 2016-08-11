#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "crypto/equihash.h"

TEST(equihash_tests, is_probably_duplicate) {
    std::shared_ptr<eh_trunc> p1 (new eh_trunc[4] {0, 1, 2, 3}, std::default_delete<eh_trunc[]>());
    std::shared_ptr<eh_trunc> p2 (new eh_trunc[4] {0, 1, 1, 3}, std::default_delete<eh_trunc[]>());
    std::shared_ptr<eh_trunc> p3 (new eh_trunc[4] {3, 1, 1, 3}, std::default_delete<eh_trunc[]>());

    ASSERT_FALSE(IsProbablyDuplicate<4>(p1, 4));
    ASSERT_FALSE(IsProbablyDuplicate<4>(p2, 4));
    ASSERT_TRUE(IsProbablyDuplicate<4>(p3, 4));
}

TEST(equihash_tests, check_basic_solver_cancelled) {
    Equihash<48,5> Eh48_5;
    crypto_generichash_blake2b_state state;
    Eh48_5.InitialiseState(state);

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](std::vector<eh_index> soln) {
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

    {
        ASSERT_NO_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == FinalColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](std::vector<eh_index> soln) {
            return false;
        }, [](EhSolverCancelCheck pos) {
            return pos == PartialEnd;
        }), EhSolverCancelledException);
    }
}
