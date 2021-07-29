use std::io::{Read, Write};

use orchard::keys::{FullViewingKey, IncomingViewingKey};
use orchard::Address;

use crate::orchard_ffi::{error, CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

//
// Addresses
//

#[no_mangle]
pub extern "C" fn orchard_address_clone(addr: *const Address) -> *mut Address {
    unsafe { addr.as_ref() }
        .map(|addr| Box::into_raw(Box::new(addr.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_address_free(addr: *mut Address) {
    if !addr.is_null() {
        drop(unsafe { Box::from_raw(addr) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_address_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut Address {
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut buf = [0u8; 43];
    match reader.read_exact(&mut buf) {
        Err(e) => {
            error!("Stream failure reading bytes of Orchard address: {}", e);
            std::ptr::null_mut()
        }
        Ok(()) => {
            let read = Address::from_raw_address_bytes(&buf);
            if read.is_some().into() {
                Box::into_raw(Box::new(read.unwrap()))
            } else {
                error!("Failed to parse Orchard address.");
                std::ptr::null_mut()
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_address_serialize(
    addr: *const Address,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let addr = unsafe {
        addr.as_ref()
            .expect("Orchard address pointer may not be null.")
    };

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match writer.write_all(&addr.to_raw_address_bytes()) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard address: {}", e);
            false
        }
    }
}

//
// Incoming viewing keys
//

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_clone(
    key: *const IncomingViewingKey,
) -> *mut IncomingViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(key.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_free(key: *mut IncomingViewingKey) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut IncomingViewingKey {
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut buf = [0u8; 64];
    match reader.read_exact(&mut buf) {
        Err(e) => {
            error!(
                "Stream failure reading bytes of Orchard incoming viewing key: {}",
                e
            );
            std::ptr::null_mut()
        }
        Ok(()) => {
            let read = IncomingViewingKey::from_bytes(&buf);
            if read.is_some().into() {
                Box::into_raw(Box::new(read.unwrap()))
            } else {
                error!("Failed to parse Orchard incoming viewing key.");
                std::ptr::null_mut()
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_serialize(
    key: *const IncomingViewingKey,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let key = unsafe {
        key.as_ref()
            .expect("Orchard incoming viewing key pointer may not be null.")
    };

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match writer.write_all(&key.to_bytes()) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard incoming viewing key: {}", e);
            false
        }
    }
}

//
// Full viewing keys
//

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_clone(
    key: *const FullViewingKey,
) -> *mut FullViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(key.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_free(key: *mut FullViewingKey) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut FullViewingKey {
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut buf = [0u8; 96];
    match FullViewingKey::read(reader) {
        Err(e) => {
            error!(
                "Stream failure reading bytes of Orchard full viewing key: {}",
                e
            );
            std::ptr::null_mut()
        }
        Ok(fvk) => Box::into_raw(Box::new(fvk)),
    }
}

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_serialize(
    key: *const FullViewingKey,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let key = unsafe {
        key.as_ref()
            .expect("Orchard full viewing key pointer may not be null.")
    };

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match key.write(&mut writer) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard full viewing key: {}", e);
            false
        }
    }
}
