#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "crypto/equihash.h"

TEST(equihash_tests, check_basic_solver_cancelled) {
    Equihash<48,5> Eh48_5;
    crypto_generichash_blake2b_state state;
    Eh48_5.InitialiseState(state);
    std::set<std::vector<unsigned int>> solns;

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == FinalColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == StartCulling;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }));
    }

    {
        ASSERT_NO_THROW(Eh48_5.BasicSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialEnd;
        }));
    }
}

TEST(equihash_tests, check_optimised_solver_cancelled) {
    Equihash<48,5> Eh48_5;
    crypto_generichash_blake2b_state state;
    Eh48_5.InitialiseState(state);
    std::set<std::vector<unsigned int>> solns;

    {
        ASSERT_NO_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return false;
        }));
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == ListColliding;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == RoundEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == FinalSorting;
        }), EhSolverCancelledException);
    }

    {
        // More state required here, because in OptimisedSolve() the
        // FinalColliding cancellation check can't throw because it will leak
        // memory, and it can't goto because that steps over initialisations.
        bool triggered = false;
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [=](EhSolverCancelCheck pos) mutable {
            if (triggered)
                return pos == StartCulling;
            if (pos == FinalColliding) {
                triggered = true;
                return true;
            }
            return false;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == StartCulling;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialGeneration;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialSorting;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialSubtreeEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialIndexEnd;
        }), EhSolverCancelledException);
    }

    {
        ASSERT_THROW(Eh48_5.OptimisedSolve(state, [](EhSolverCancelCheck pos) {
            return pos == PartialEnd;
        }), EhSolverCancelledException);
    }
}
