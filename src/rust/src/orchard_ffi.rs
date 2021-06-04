use std::io;

use libc::{c_uchar, size_t};
use tracing::error;
use zcash_primitives::serialize::Vector;

use crate::streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

#[no_mangle]
pub extern "C" fn orchard_bundle_clone(
    bundle: *const orchard::Bundle<Authorized>,
) -> *mut orchard::Bundle<Authorized> {
    unsafe { bundle.as_ref() }
        .map(|bundle| Box::into_raw(Box::new(bundle.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_bundle_free(orchard_bundle: *mut orchard::Bundle<Authorized>) {
    if !orchard_bundle.is_null() {
        drop(unsafe { Box::from_raw(orchard_bundle) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_bundle_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut orchard::Bundle<Authorized> {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let bundle = ();
    // TODO: If error occurs, log it and return nullptr.

    Box::into_raw(Box::new(bundle));
}

#[no_mangle]
pub extern "C" fn orchard_bundle_serialize(
    orchard_bundle: *const orchard::Bundle<Authorized>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());

    match if let Some(bundle) = unsafe { orchard_bundle.as_ref() } {
        // TODO: Serialize bundle.
        Err(io::Error::new(io::ErrorKind::Other, "Unimplemented"))
    } else {
        // No bundle; serialize an empty vector.
        Vector::write(writer, &[], |_, _| Ok(()))
    } {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}
