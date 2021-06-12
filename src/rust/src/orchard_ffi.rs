use std::ptr;

use orchard::{bundle::Authorized, Bundle};
use tracing::error;
use zcash_primitives::transaction::components::{orchard as orchard_serialization, Amount};

use crate::streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

#[no_mangle]
pub extern "C" fn orchard_bundle_clone(
    bundle: *const Bundle<Authorized, Amount>,
) -> *mut Bundle<Authorized, Amount> {
    unsafe { bundle.as_ref() }
        .map(|bundle| Box::into_raw(Box::new(bundle.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_bundle_free(bundle: *mut Bundle<Authorized, Amount>) {
    if !bundle.is_null() {
        drop(unsafe { Box::from_raw(bundle) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
    bundle_ret: *mut *mut Bundle<Authorized, Amount>,
) -> bool {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match orchard_serialization::read_v5_bundle(reader) {
        Ok(parsed) => {
            unsafe {
                *bundle_ret = if let Some(bundle) = parsed {
                    Box::into_raw(Box::new(bundle))
                } else {
                    ptr::null_mut::<Bundle<Authorized, Amount>>()
                };
            };
            true
        }
        Err(e) => {
            error!("Failed to parse Orchard bundle: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_serialize(
    bundle: *const Bundle<Authorized, Amount>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let bundle = unsafe { bundle.as_ref() };
    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    match orchard_serialization::write_v5_bundle(bundle, writer) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}
