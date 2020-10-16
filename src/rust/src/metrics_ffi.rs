use libc::{c_char, c_double};
use metrics::{try_recorder, GaugeValue, Key, KeyData};
use std::ffi::CStr;

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

#[no_mangle]
pub extern "C" fn metrics_increment_counter(callsite: *const FfiCallsite, value: u64) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.increment_counter(Key::Borrowed(&callsite.key_data), value);
    }
}

#[no_mangle]
pub extern "C" fn metrics_update_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Absolute(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_increment_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Increment(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_decrement_gauge(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.update_gauge(
            Key::Borrowed(&callsite.key_data),
            GaugeValue::Decrement(value),
        );
    }
}

#[no_mangle]
pub extern "C" fn metrics_record_histogram(callsite: *const FfiCallsite, value: c_double) {
    if let Some(recorder) = try_recorder() {
        let callsite = unsafe { callsite.as_ref().unwrap() };
        recorder.record_histogram(Key::Borrowed(&callsite.key_data), value);
    }
}
