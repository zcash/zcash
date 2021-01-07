use libc::{c_char, c_double};
use metrics::{try_recorder, GaugeValue, Key, KeyData, Label};
use metrics_exporter_prometheus::PrometheusBuilder;
use std::ffi::CStr;
use std::net::{IpAddr, SocketAddr};
use std::ptr;
use std::slice;
use tracing::error;

mod prometheus;

#[no_mangle]
pub extern "C" fn metrics_run(
    bind_address: *const c_char,
    allow_ips: *const *const c_char,
    allow_ips_len: usize,
    prometheus_port: u16,
) -> bool {
    // Parse any allowed IPs.
    let allow_ips = unsafe { slice::from_raw_parts(allow_ips, allow_ips_len) };
    let mut allow_ips: Vec<ipnet::IpNet> = match allow_ips
        .iter()
        .map(|&p| unsafe { CStr::from_ptr(p) })
        .map(|s| {
            s.to_str().ok().and_then(|s| {
                s.parse()
                    .map_err(|e| {
                        error!("Invalid -metricsallowip argument '{}': {}", s, e);
                    })
                    .ok()
            })
        })
        .collect()
    {
        Some(ips) => ips,
        None => {
            return false;
        }
    };
    // We always allow localhost.
    allow_ips.extend(&["127.0.0.0/8".parse().unwrap(), "::1/128".parse().unwrap()]);

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

    prometheus::install(bind_address, PrometheusBuilder::new(), allow_ips).is_ok()
}

pub struct FfiCallsite {
    key_data: KeyData,
}

#[no_mangle]
pub extern "C" fn metrics_callsite(name: *const c_char) -> *mut FfiCallsite {
    let name = unsafe { CStr::from_ptr(name) }.to_str().unwrap();
    Box::into_raw(Box::new(FfiCallsite {
        key_data: KeyData::from_name(name),
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
            inner: Key::Owned(KeyData::from_parts(name, labels)),
        }))
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_increment_counter(callsite: *const FfiCallsite, value: u64) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.increment_counter(Key::Borrowed(&callsite.key_data), value);
    }
}

#[no_mangle]
pub extern "C" fn metrics_increment_counter(key: *mut FfiKey, value: u64) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.increment_counter(key.inner, value);
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_update_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Absolute(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_update_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.update_gauge(key.inner, GaugeValue::Absolute(value));
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_increment_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Increment(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_increment_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.update_gauge(key.inner, GaugeValue::Increment(value));
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_decrement_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Decrement(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_decrement_gauge(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.update_gauge(key.inner, GaugeValue::Decrement(value));
        }
    }
}

#[no_mangle]
pub extern "C" fn metrics_static_record_histogram(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.record_histogram(Key::Borrowed(&callsite.key_data), value);
    }
}

#[no_mangle]
pub extern "C" fn metrics_record_histogram(key: *mut FfiKey, value: c_double) {
    if let Some(recorder) = try_recorder() {
        if !key.is_null() {
            let key = unsafe { Box::from_raw(key) };
            recorder.record_histogram(key.inner, value);
        }
    }
}
