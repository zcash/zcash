// Copyright (c) 2020 Jack Grigg
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

#ifndef TRACING_INCLUDE_H_
#define TRACING_INCLUDE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#ifdef WIN32
typedef uint16_t codeunit;
#else
typedef uint8_t codeunit;
#endif

struct TracingHandle;
typedef struct TracingHandle TracingHandle;

/// Initializes the tracing crate, returning a handle for the logging
/// component. The handle must be freed to close the logging component.
///
/// If log_path is NULL, logging is sent to standard output.
TracingHandle* tracing_init(
    const codeunit* log_path,
    size_t log_path_len,
    const char* initial_filter,
    bool log_timestamps);

/// Frees a tracing handle returned from `tracing_init`;
void tracing_free(TracingHandle* handle);

/// Reloads the tracing filter.
void tracing_reload(TracingHandle* handle, const char* new_filter);

struct TracingCallsite;
typedef struct TracingCallsite TracingCallsite;

/// Creates a tracing callsite.
///
/// You should usually call the `TracingLog` macro (or one of the helper
/// macros such as `TracingInfo`) instead of calling this directly.
///
/// This MUST ONLY be called to assign a `static void*`, and all string
/// arguments MUST be `static const char*`.
TracingCallsite* tracing_callsite(
    const char* name,
    const char* target,
    const char* level,
    const char* file,
    uint32_t line,
    const char** fields,
    size_t fields_len,
    bool is_span);

/// Logs a message for a callsite.
///
/// You should usually call the `TracingLog` macro (or one of the helper
/// macros such as `TracingInfo`) instead of calling this directly.
void tracing_log(
    const TracingCallsite* callsite,
    const char** field_values,
    size_t fields_len);

#ifdef __cplusplus
}
#endif

//
// Helper macros
//

#define T_DOUBLEESCAPE(a) #a
#define T_ESCAPEQUOTE(a) T_DOUBLEESCAPE(a)

// Computes the length of the given array. This is COUNT_OF from Chromium.
#define T_ARRLEN(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))

// Constructs a tracing callsite.
//
// name, target, and fields MUST be static constants, and the output of this
// macro MUST be stored as a static void*.
#define T_CALLSITE(name, target, level, fields, is_span) \
    tracing_callsite(                                    \
        name,                                            \
        target,                                          \
        level,                                           \
        __FILE__,                                        \
        __LINE__,                                        \
        fields,                                          \
        T_ARRLEN(fields),                                \
        is_span)

//
// Events
//

/// Constructs a new event.
#define TracingLog(level, target, message)                   \
    do {                                                     \
        static const char* NAME =                            \
            "event " __FILE__ ":" T_ESCAPEQUOTE(__LINE__);   \
        static const char* FIELDS[] = {"message"};           \
        const char* T_VALUES[] = {message};                  \
        static TracingCallsite* CALLSITE =                   \
            T_CALLSITE(NAME, target, level, FIELDS, false);  \
        tracing_log(CALLSITE, T_VALUES, T_ARRLEN(T_VALUES)); \
    } while (0)

/// Constructs an event at the error level.
#define TracingError(...) TracingLog("error", __VA_ARGS__)
/// Constructs an event at the warn level.
#define TracingWarn(...) TracingLog("warn", __VA_ARGS__)
/// Constructs an event at the info level.
#define TracingInfo(...) TracingLog("info", __VA_ARGS__)
/// Constructs an event at the debug level.
#define TracingDebug(...) TracingLog("debug", __VA_ARGS__)
/// Constructs an event at the trace level.
#define TracingTrace(...) TracingLog("trace", __VA_ARGS__)

#endif // TRACING_INCLUDE_H_
