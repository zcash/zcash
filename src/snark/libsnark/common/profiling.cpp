/** @file
 *****************************************************************************

 Implementation of functions for profiling code blocks.

 See profiling.hpp .

 *****************************************************************************
 * @author     This file is part of libsnark, developed by SCIPR Lab
 *             and contributors (see AUTHORS).
 * @copyright  MIT license (see LICENSE file)
 *****************************************************************************/

#include "common/profiling.hpp"
#include <cassert>
#include <stdexcept>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <list>
#include <vector>
#include <ctime>
#include "common/default_types/ec_pp.hpp"
#include "common/utils.hpp"

#ifndef NO_PROCPS
#include <proc/readproc.h>
#endif

namespace libsnark {

int64_t get_nsec_time()
{
    auto timepoint = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(timepoint.time_since_epoch()).count();
}

/* Return total CPU time consumed by all threads of the process, in nanoseconds. */
int64_t get_nsec_cpu_time()
{
    ::timespec ts;
    if ( ::clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts) )
        throw ::std::runtime_error("clock_gettime(CLOCK_PROCESS_CPUTIME_ID) failed");
        // If we expected this to work, don't silently ignore failures, because that would hide the problem and incur an unnecessarily system-call overhead. So if we ever observe this exception, we should probably add a suitable #ifdef .
        //TODO: clock_gettime(CLOCK_PROCESS_CPUTIME_ID) is not supported by native Windows. What about Cygwin? Should we #ifdef on CLOCK_PROCESS_CPUTIME_ID or on __linux__?
    return ts.tv_sec * 1000000000ll + ts.tv_nsec;
}

static int64_t start_time;
static int64_t last_time;
static int64_t start_cpu_time;
static int64_t last_cpu_time;

void start_profiling()
{
    printf("Reset time counters for profiling\n");

    last_time = start_time = get_nsec_time();
    last_cpu_time = start_cpu_time = get_nsec_cpu_time();
}

std::map<std::string, size_t> invocation_counts;
static std::map<std::string, int64_t> enter_times;
std::map<std::string, int64_t> last_times;
std::map<std::string, int64_t> cumulative_times;
//TODO: Instead of analogous maps for time and cpu_time, use a single struct-valued map
static std::map<std::string, int64_t> enter_cpu_times;
static std::map<std::string, int64_t> last_cpu_times;
static std::map<std::pair<std::string, std::string>, int64_t> op_counts;
static std::map<std::pair<std::string, std::string>, int64_t> cumulative_op_counts; // ((msg, data_point), value)
    // TODO: Convert op_counts and cumulative_op_counts from pair to structs
static size_t indentation = 0;

static std::vector<std::string> block_names;

static std::list<std::pair<std::string, int64_t*> > op_data_points = {
#ifdef PROFILE_OP_COUNTS
    std::make_pair("Fradd", &Fr<default_ec_pp>::add_cnt),
    std::make_pair("Frsub", &Fr<default_ec_pp>::sub_cnt),
    std::make_pair("Frmul", &Fr<default_ec_pp>::mul_cnt),
    std::make_pair("Frinv", &Fr<default_ec_pp>::inv_cnt),
    std::make_pair("Fqadd", &Fq<default_ec_pp>::add_cnt),
    std::make_pair("Fqsub", &Fq<default_ec_pp>::sub_cnt),
    std::make_pair("Fqmul", &Fq<default_ec_pp>::mul_cnt),
    std::make_pair("Fqinv", &Fq<default_ec_pp>::inv_cnt),
    std::make_pair("G1add", &G1<default_ec_pp>::add_cnt),
    std::make_pair("G1dbl", &G1<default_ec_pp>::dbl_cnt),
    std::make_pair("G2add", &G2<default_ec_pp>::add_cnt),
    std::make_pair("G2dbl", &G2<default_ec_pp>::dbl_cnt)
#endif
};

bool inhibit_profiling_info = true;
bool inhibit_profiling_counters = false;

void clear_profiling_counters()
{
    invocation_counts.clear();
    last_times.clear();
    last_cpu_times.clear();
    cumulative_times.clear();
}

void print_cumulative_time_entry(const std::string &key, const int64_t factor)
{
    const double total_ms = (cumulative_times.at(key) * 1e-6);
    const size_t cnt = invocation_counts.at(key);
    const double avg_ms = total_ms / cnt;
    printf("   %-45s: %12.5fms = %" PRId64 " * %0.5fms (%zu invocations, %0.5fms = %" PRId64 " * %0.5fms per invocation)\n", key.c_str(), total_ms, factor, total_ms/factor, cnt, avg_ms, factor, avg_ms/factor);
}

void print_cumulative_times(const int64_t factor)
{
    printf("Dumping times:\n");
    for (auto& kv : cumulative_times)
    {
        print_cumulative_time_entry(kv.first, factor);
    }
}

void print_cumulative_op_counts(const bool only_fq)
{
#ifdef PROFILE_OP_COUNTS
    printf("Dumping operation counts:\n");
    for (auto& msg : invocation_counts)
    {
        printf("  %-45s: ", msg.first.c_str());
        bool first = true;
        for (auto& data_point : op_data_points)
        {
            if (only_fq && data_point.first.compare(0, 2, "Fq") != 0)
            {
                continue;
            }

            if (!first)
            {
                printf(", ");
            }
            printf("%-5s = %7.0f (%3zu)",
                   data_point.first.c_str(),
                   1. * cumulative_op_counts[std::make_pair(msg.first, data_point.first)] / msg.second,
                   msg.second);
            first = false;
        }
        printf("\n");
    }
#else
    UNUSED(only_fq);
#endif
}

void print_op_profiling(const std::string &msg)
{
#ifdef PROFILE_OP_COUNTS
    printf("\n");
    print_indent();

    printf("(opcounts) = (");
    bool first = true;
    for (std::pair<std::string, int64_t*> p : op_data_points)
    {
        if (!first)
        {
            printf(", ");
        }

        printf("%s=%lld", p.first.c_str(), *(p.second)-op_counts[std::make_pair(msg, p.first)]);
        first = false;
    }
    printf(")");
#else
    UNUSED(msg);
#endif
}

static void print_times_from_last_and_start(int64_t     now, int64_t     last,
                                            int64_t cpu_now, int64_t cpu_last)
{
    int64_t time_from_start = now - start_time;
    int64_t time_from_last = now - last;

    int64_t cpu_time_from_start = cpu_now - start_cpu_time;
    int64_t cpu_time_from_last = cpu_now - cpu_last;

    if (time_from_last != 0) {
        double parallelism_from_last = 1.0 * cpu_time_from_last / time_from_last;
        printf("[%0.4fs x%0.2f]", time_from_last * 1e-9, parallelism_from_last);
    } else {
        printf("[             ]");
    }
    if (time_from_start != 0) {
        double parallelism_from_start = 1.0 * cpu_time_from_start / time_from_start;
        printf("\t(%0.4fs x%0.2f from start)", time_from_start * 1e-9, parallelism_from_start);
    }
}

void print_time(const char* msg)
{
    if (inhibit_profiling_info)
    {
        return;
    }

    int64_t now = get_nsec_time();
    int64_t cpu_now = get_nsec_cpu_time();

    printf("%-35s\t", msg);
    print_times_from_last_and_start(now, last_time, cpu_now, last_cpu_time);
#ifdef PROFILE_OP_COUNTS
    print_op_profiling(msg);
#endif
    printf("\n");

    fflush(stdout);
    last_time = now;
    last_cpu_time = cpu_now;
}

void print_header(const char *msg)
{
    printf("\n================================================================================\n");
    printf("%s\n", msg);
    printf("================================================================================\n\n");
}

void print_indent()
{
    for (size_t i = 0; i < indentation; ++i)
    {
        printf("  ");
    }
}

void op_profiling_enter(const std::string &msg)
{
    for (std::pair<std::string, int64_t*> p : op_data_points)
    {
        op_counts[std::make_pair(msg, p.first)] = *(p.second);
    }
}

void enter_block(const std::string &msg, const bool indent)
{
    if (inhibit_profiling_counters)
    {
        return;
    }

    block_names.emplace_back(msg);
    int64_t t = get_nsec_time();
    enter_times[msg] = t;
    int64_t cpu_t = get_nsec_cpu_time();
    enter_cpu_times[msg] = cpu_t;

    if (inhibit_profiling_info)
    {
        return;
    }

#ifdef MULTICORE
#pragma omp critical
#endif
    {
        op_profiling_enter(msg);

        print_indent();
        printf("(enter) %-35s\t", msg.c_str());
        print_times_from_last_and_start(t, t, cpu_t, cpu_t);
        printf("\n");
        fflush(stdout);

        if (indent)
        {
            ++indentation;
        }
    }
}

void leave_block(const std::string &msg, const bool indent)
{
    if (inhibit_profiling_counters)
    {
        return;
    }

#ifndef MULTICORE
    assert(*(--block_names.end()) == msg);
#endif
    block_names.pop_back();

    ++invocation_counts[msg];

    int64_t t = get_nsec_time();
    last_times[msg] = (t - enter_times[msg]);
    cumulative_times[msg] += (t - enter_times[msg]);

    int64_t cpu_t = get_nsec_cpu_time();
    last_cpu_times[msg] = (cpu_t - enter_cpu_times[msg]);

#ifdef PROFILE_OP_COUNTS
    for (std::pair<std::string, int64_t*> p : op_data_points)
    {
        cumulative_op_counts[std::make_pair(msg, p.first)] += *(p.second)-op_counts[std::make_pair(msg, p.first)];
    }
#endif

    if (inhibit_profiling_info)
    {
        return;
    }

#ifdef MULTICORE
#pragma omp critical
#endif
    {
        if (indent)
        {
            --indentation;
        }

        print_indent();
        printf("(leave) %-35s\t", msg.c_str());
        print_times_from_last_and_start(t, enter_times[msg], cpu_t, enter_cpu_times[msg]);
        print_op_profiling(msg);
        printf("\n");
        fflush(stdout);
    }
}

void print_mem(const std::string &s)
{
#ifndef NO_PROCPS
    struct proc_t usage;
    look_up_our_self(&usage);
    if (s.empty())
    {
        printf("* Peak vsize (physical memory+swap) in mebibytes: %lu\n", usage.vsize >> 20);
    }
    else
    {
        printf("* Peak vsize (physical memory+swap) in mebibytes (%s): %lu\n", s.c_str(), usage.vsize >> 20);
    }
#else
    printf("* Memory profiling not supported in NO_PROCPS mode\n");
#endif
}

void print_compilation_info()
{
#ifdef __GNUC__
    printf("g++ version: %s\n", __VERSION__);
    //printf("Compiled on %s %s\n", __DATE__, __TIME__);
#endif
#ifdef STATIC
    printf("STATIC: yes\n");
#else
    printf("STATIC: no\n");
#endif
#ifdef MULTICORE
    printf("MULTICORE: yes\n");
#else
    printf("MULTICORE: no\n");
#endif
#ifdef DEBUG
    printf("DEBUG: yes\n");
#else
    printf("DEBUG: no\n");
#endif
#ifdef PROFILE_OP_COUNTS
    printf("PROFILE_OP_COUNTS: yes\n");
#else
    printf("PROFILE_OP_COUNTS: no\n");
#endif
#ifdef _GLIBCXX_DEBUG
    printf("_GLIBCXX_DEBUG: yes\n");
#else
    printf("_GLIBCXX_DEBUG: no\n");
#endif
}

} // libsnark
