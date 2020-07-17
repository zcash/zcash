#ifndef BENCHMARKS_H
#define BENCHMARKS_H

#include <sys/time.h>
#include <stdlib.h>

extern double benchmark_sleep();
extern double benchmark_create_joinsplit();
extern std::vector<double> benchmark_create_joinsplit_threaded(int nThreads);
extern double benchmark_solve_equihash();
extern std::vector<double> benchmark_solve_equihash_threaded(int nThreads);
extern double benchmark_verify_joinsplit(const JSDescription &joinsplit);
extern double benchmark_verify_equihash();
extern double benchmark_large_tx(size_t nInputs);
extern double benchmark_try_decrypt_sprout_notes(size_t nAddrs);
extern double benchmark_try_decrypt_sapling_notes(size_t nAddrs);
extern double benchmark_increment_sprout_note_witnesses(size_t nTxs);
extern double benchmark_increment_sapling_note_witnesses(size_t nTxs);
extern double benchmark_connectblock_slow();
extern double benchmark_sendtoaddress(CAmount amount);
extern double benchmark_loadwallet();
extern double benchmark_listunspent();
extern double benchmark_create_sapling_spend();
extern double benchmark_create_sapling_output();
extern double benchmark_verify_sapling_spend();
extern double benchmark_verify_sapling_output();

#endif
