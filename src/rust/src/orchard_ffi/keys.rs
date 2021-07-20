use std::io::{Read, Write};

use orchard::keys::IncomingViewingKey;

use crate::orchard_ffi::{error, CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_clone(
    key: *const IncomingViewingKey,
) -> *mut IncomingViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(key.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_free(
    key: *mut IncomingViewingKey
) {
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
            error!("Stream failure reading bytes of Orchard incoming viewing key: {}", e);
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
