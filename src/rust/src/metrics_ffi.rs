use libc::{c_char, c_double};
use metrics::{try_recorder, Key, Label};
use metrics_exporter_prometheus::{BuildError, PrometheusBuilder};
use metrics_util::layers::{FilterLayer, Stack};

use std::ffi::CStr;
use std::net::{IpAddr, SocketAddr};
use std::ptr;
use std::slice;
use std::thread;

use tracing::error;

/// Builds the recorder and exporter, applies the given filters to the recorder, and
/// installs both of them globally.
fn metrics_install(builder: PrometheusBuilder, filters: &[&str]) -> Result<(), BuildError> {
    let runtime = tokio::runtime::Builder::new_current_thread()
        .enable_all()
        .build()
        .map_err(|e| BuildError::FailedToCreateRuntime(e.to_string()))?;

    let (recorder, exporter) = {
        let _g = runtime.enter();
        builder.build()?
    };

    thread::Builder::new()
        .name("zc-metrics".to_string())
        .spawn(move || runtime.block_on(exporter))
        .map_err(|e| BuildError::FailedToCreateRuntime(e.to_string()))?;

    Stack::new(recorder)
        .push(FilterLayer::from_patterns(filters))
        .install()?;

    Ok(())
}

#[no_mangle]
pub extern "C" fn metrics_run(
    bind_address: *const c_char,
    allow_ips: *const *const c_char,
    allow_ips_len: usize,
    prometheus_port: u16,
    debug_metrics: bool,
) -> bool {
    // Convert the C string IPs to Rust strings.
    let allow_ips = unsafe { slice::from_raw_parts(allow_ips, allow_ips_len) };
    let mut allow_ips: Vec<&str> = match allow_ips
        .iter()
        .map(|&p| unsafe { CStr::from_ptr(p) })
        .map(|s| s.to_str().ok())
        .collect()
    {
        Some(ips) => ips,
        None => {
            return false;
        }
    };
    // We always allow localhost.
    allow_ips.extend(["127.0.0.0/8", "::1/128"]);

    // Parse the address to bind to.
    let bind_address = SocketAddr::new(
        if allow_ips.is_empty() {
            // Default to loopback if not allowing external IPs.
            "127.0.0.1".parse::<IpAddr>().unwrap()
        } else if bind_address.is_null() {
            // No specific bind address specified, bind to any.
            "0.0.0.0".parse::<IpAddr>().unwrap()
        } else {
            match unsafe { CStr::from_ptr(bind_address) }
                .to_str()
                .ok()
                .and_then(|s| s.parse::<IpAddr>().ok())
            {
                Some(addr) => addr,
                None => {
                    error!("Invalid -metricsbind argument");
                    return false;
                }
            }
        },
        prometheus_port,
    );

    // Metrics matching any of these filters will be discarded.
    let filters = if debug_metrics {
        &[][..]
    } else {
        &["zcashd.debug."]
    };

    allow_ips
        .into_iter()
        .try_fold(
            PrometheusBuilder::new().with_http_listener(bind_address),
            |builder, subnet| {
                builder.add_allowed_address(subnet).map_err(|e| {
                    error!("Invalid -metricsallowip argument '{}': {}", subnet, e);
                    e
                })
            },
        )
        .and_then(|builder| {
            metrics_install(builder, filters).map_err(|e| {
                error!("Could not install Prometheus exporter: {}", e);
                e
            })
        })
        .is_ok()
}

pub struct FfiCallsite {
    key: Key,
}

#[no_mangle]
pub extern "C" fn metrics_callsite(
    name: *const c_char,
    label_names: *const *const c_char,
    label_values: *const *const c_char,
    labels_len: usize,
) -> *mut FfiCallsite {
    let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
    let labels = unsafe { slice::from_raw_parts(label_names, labels_len) };
    let values = unsafe { slice::from_raw_parts(label_values, labels_len) };

    let stringify = |s: &[_]| {
        s.iter()
            .map(|&p| unsafe { CStr::from_ptr(p) })
            .map(|cs| cs.to_string_lossy().into_owned())
            .collect::<Vec<_>>()
    };
    let labels = stringify(labels);
    let values = stringify(values);

    let labels: Vec<_> = labels
        .into_iter()
        .zip(values.into_iter())
        .map(|(name, value)| Label::new(name, value))
        .collect();

    Box::into_raw(Box::new(FfiCallsite {
        key: Key::from_parts(name, labels),
    }))
}

pub struct FfiKey {
    inner: Key,
}

#[no_mangle]
pub extern "C" fn metrics_key(
    name: *const c_char,
    label_names: *const *const c_char,
    label_values: *const *const c_char,
    labels_len: usize,
) -> *mut FfiKey {
    if try_recorder().is_none() {
        // No recorder is currently installed, so don't genenerate a key. We check for
        // null inside each API that consumes an FfiKey, just in case a recorder was
        // installed in a racy way.
        ptr::null_mut()
    } else {
        let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
        let labels = unsafe { slice::from_raw_parts(label_names, labels_len) };
        let values = unsafe { slice::from_raw_parts(label_values, labels_len) };

        let stringify = |s: &[_]| {
            s.iter()
                .map(|&p| unsafe { CStr::from_ptr(p) })
                .map(|cs| cs.to_string_lossy().into_owned())
                .collect::<Vec<_>>()
        };
        let labels = stringify(labels);
        let values = stringify(values);

        let labels: Vec<_> = labels
            .into_iter()
            .zip(values.into_iter())
            .map(|(name, value)| Label::new(name, value))
            .collect();

        Box::into_raw(Box::new(FfiKey {
            inner: Key::from_parts(name, labels),
        }))
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_increment_counter(callsite: *const FfiCallsite, value: u64) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.register_counter(&callsite.key).increment(value);
    }
}

#[no_mangle]
pub extern "C" fn metrics_increment_counter(key: *mut FfiKey, value: u64) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.register_counter(&key.inner).increment(value);
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_update_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.register_gauge(&callsite.key).set(value as f64);
    }
}

#[no_mangle]
pub extern "C" fn metrics_update_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.register_gauge(&key.inner).set(value as f64);
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_increment_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder
            .register_gauge(&callsite.key)
            .increment(value as f64);
    }
}

#[no_mangle]
pub extern "C" fn metrics_increment_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.register_gauge(&key.inner).increment(value as f64);
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_decrement_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder
            .register_gauge(&callsite.key)
            .decrement(value as f64);
    }
}

#[no_mangle]
pub extern "C" fn metrics_decrement_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.register_gauge(&key.inner).decrement(value as f64);
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_record_histogram(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder
            .register_histogram(&callsite.key)
            .record(value as f64);
    }
}

#[no_mangle]
pub extern "C" fn metrics_record_histogram(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.register_histogram(&key.inner).record(value as f64);
        }
    }
}
