#ifndef BENCHMARKS_H
#define BENCHMARKS_H

#include <sys/time.h>
#include <stdlib.h>

extern double benchmark_sleep();
extern double benchmark_parameter_loading();
extern double benchmark_create_joinsplit();
extern double benchmark_solve_equihash();

#endif
