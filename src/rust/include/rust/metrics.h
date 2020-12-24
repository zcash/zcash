// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef ZCASH_RUST_INCLUDE_RUST_METRICS_H
#define ZCASH_RUST_INCLUDE_RUST_METRICS_H

#include "rust/helpers.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initializes the metrics runtime and runs the Prometheus exporter in a new
/// thread.
///
/// listen_address is a string like <host_name>:<port>.
///
/// Returns false if listen_address was not a valid SocketAddr.
bool metrics_run(const char* listen_address);

struct MetricsCallsite;
typedef struct MetricsCallsite MetricsCallsite;

struct MetricsKey;
typedef struct MetricsKey MetricsKey;

/// Creates a metrics callsite.
///
/// You should usually call one of the helper macros such as `MetricsCounter`
/// instead of calling this directly.
///
/// This MUST ONLY be called to assign a `static MetricsCallsite*`, and all
/// string arguments MUST be static `const char*` constants, and MUST be valid
/// UTF-8.
MetricsCallsite* metrics_callsite(const char* name);

/// Creates a metrics key.
///
/// This API supports labels that may not have static values, and is intended
/// to be called for each metrics callsite invocation. As such, it returns null
/// if a metrics recorder is not installed, to save on construction costs.
///
/// You should usually call one of the helper macros such as `MetricsCounter`
/// instead of calling this directly.
///
/// API requirements:
/// - label_names and label_values, if not null, MUST be the same length.
/// - All string arguments MUST be valid UTF-8.
MetricsKey* metrics_key(
    const char* name,
    const char* const* label_names,
    const char* const* label_values,
    size_t labels_len);

/// Increments a counter.
///
/// Counters represent a single monotonic value, which means the value can only
/// be incremented, not decremented, and always starts out with an initial value
/// of zero.
void metrics_static_increment_counter(const MetricsCallsite* callsite, uint64_t value);

/// Increments a counter.
///
/// Counters represent a single monotonic value, which means the value can only
/// be incremented, not decremented, and always starts out with an initial value
/// of zero.
void metrics_increment_counter(MetricsKey* key, uint64_t value);

/// Updates a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_static_update_gauge(const MetricsCallsite* callsite, double value);

/// Updates a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_update_gauge(MetricsKey* callsite, double value);

/// Increments a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_static_increment_gauge(const MetricsCallsite* callsite, double value);

/// Increments a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_increment_gauge(MetricsKey* callsite, double value);

/// Decrements a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_static_decrement_gauge(const MetricsCallsite* callsite, double value);

/// Decrements a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
void metrics_decrement_gauge(MetricsKey* callsite, double value);

/// Records a histogram.
///
/// Histograms measure the distribution of values for a given set of
/// measurements, and start with no initial values.
void metrics_static_record_histogram(const MetricsCallsite* callsite, double value);

/// Records a histogram.
///
/// Histograms measure the distribution of values for a given set of
/// measurements, and start with no initial values.
void metrics_record_histogram(MetricsKey* callsite, double value);

#ifdef __cplusplus
}
#endif

//
// Helper macros
//

#ifdef __cplusplus
// Constructs a metrics callsite.
//
// The 'static constexpr' hack ensures that name is a compile-time constant
// with static storage duration. The output of this macro MUST be stored as a
// static MetricsCallsite*.
#define M_CALLSITE(name) ([&] {                  \
    static constexpr const char* _m_name = name; \
    return metrics_callsite(_m_name);            \
}())
#else
// Constructs a metrics callsite.
//
// name MUST be a static constant, and the output of this macro MUST be stored
// as a static MetricsCallsite*.
#define M_CALLSITE(name) \
    metrics_callsite(name)
#endif

// Constructs a metrics key.
#define M_KEY(name, labels, values) \
    metrics_key(                    \
        name,                       \
        labels,                     \
        values,                     \
        T_ARRLEN(labels))

//
// Metrics
//

/// Increments a counter.
///
/// Counters represent a single monotonic value, which means the value can only
/// be incremented, not decremented, and always starts out with an initial value
/// of zero.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsCounter(name, value, ...)                             \
    do {                                                             \
        IFE(__VA_ARGS__)                                             \
        (static MetricsCallsite* CALLSITE = M_CALLSITE(name);        \
         metrics_static_increment_counter(CALLSITE, value);)         \
            IFN(__VA_ARGS__)(const char* M_LABELS[] =                \
                                 {T_FIELD_NAMES(__VA_ARGS__)};       \
                             const char* M_VALUES[] =                \
                                 {T_FIELD_VALUES(__VA_ARGS__)};      \
                             MetricsKey* KEY =                       \
                                 M_KEY(name, M_LABELS, M_VALUES);    \
                             metrics_increment_counter(KEY, value);) \
    } while (0)

/// Increments a counter by one.
///
/// Counters represent a single monotonic value, which means the value can only
/// be incremented, not decremented, and always starts out with an initial value
/// of zero.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsIncrementCounter(name, ...) MetricsCounter(name, 1, __VA_ARGS__)

/// Updates a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsGauge(name, value, ...)                            \
    do {                                                          \
        IFE(__VA_ARGS__)                                          \
        (static MetricsCallsite* CALLSITE = M_CALLSITE(name);     \
         metrics_static_update_gauge(CALLSITE, value);)           \
            IFN(__VA_ARGS__)(const char* M_LABELS[] =             \
                                 {T_FIELD_NAMES(__VA_ARGS__)};    \
                             const char* M_VALUES[] =             \
                                 {T_FIELD_VALUES(__VA_ARGS__)};   \
                             MetricsKey* KEY =                    \
                                 M_KEY(name, M_LABELS, M_VALUES); \
                             metrics_update_gauge(KEY, value);)   \
    } while (0)

/// Increments a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsIncrementGauge(name, value, ...)                    \
    do {                                                           \
        IFE(__VA_ARGS__)                                           \
        (static MetricsCallsite* CALLSITE = M_CALLSITE(name);      \
         metrics_static_increment_gauge(CALLSITE, value);)         \
            IFN(__VA_ARGS__)(const char* M_LABELS[] =              \
                                 {T_FIELD_NAMES(__VA_ARGS__)};     \
                             const char* M_VALUES[] =              \
                                 {T_FIELD_VALUES(__VA_ARGS__)};    \
                             MetricsKey* KEY =                     \
                                 M_KEY(name, M_LABELS, M_VALUES);  \
                             metrics_increment_gauge(KEY, value);) \
    } while (0)

/// Decrements a gauge.
///
/// Gauges represent a single value that can go up or down over time, and always
/// starts out with an initial value of zero.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsDecrementGauge(name, value, ...)                    \
    do {                                                           \
        IFE(__VA_ARGS__)                                           \
        (static MetricsCallsite* CALLSITE = M_CALLSITE(name);      \
         metrics_static_decrement_gauge(CALLSITE, value);)         \
            IFN(__VA_ARGS__)(const char* M_LABELS[] =              \
                                 {T_FIELD_NAMES(__VA_ARGS__)};     \
                             const char* M_VALUES[] =              \
                                 {T_FIELD_VALUES(__VA_ARGS__)};    \
                             MetricsKey* KEY =                     \
                                 M_KEY(name, M_LABELS, M_VALUES);  \
                             metrics_decrement_gauge(KEY, value);) \
    } while (0)

/// Records a histogram.
///
/// Histograms measure the distribution of values for a given set of
/// measurements, and start with no initial values.
///
/// name MUST be a static constant, and all strings MUST be valid UTF-8.
#define MetricsHistogram(name, value, ...)                          \
    do {                                                            \
        IFE(__VA_ARGS__)                                            \
        (static MetricsCallsite* CALLSITE = M_CALLSITE(name);       \
         metrics_static_record_histogram(CALLSITE, value);)         \
            IFN(__VA_ARGS__)(const char* M_LABELS[] =               \
                                 {T_FIELD_NAMES(__VA_ARGS__)};      \
                             const char* M_VALUES[] =               \
                                 {T_FIELD_VALUES(__VA_ARGS__)};     \
                             MetricsKey* KEY =                      \
                                 M_KEY(name, M_LABELS, M_VALUES);   \
                             metrics_record_histogram(KEY, value);) \
    } while (0)

#endif // ZCASH_RUST_INCLUDE_RUST_METRICS_H
