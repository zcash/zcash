use std::io::{Read, Write};
use std::slice;
use tracing::error;

use orchard::keys::{DiversifierIndex, FullViewingKey, IncomingViewingKey, SpendingKey};
use orchard::Address;

use crate::{
    streams_ffi::{CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb},
    zcashd_orchard::OrderedAddress,
};

//
// Addresses
//

#[no_mangle]
pub extern "C" fn orchard_address_clone(addr: *const Address) -> *mut Address {
    unsafe { addr.as_ref() }
        .map(|addr| Box::into_raw(Box::new(*addr)))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_address_free(addr: *mut Address) {
    if !addr.is_null() {
        drop(unsafe { Box::from_raw(addr) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_raw_address_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut Address {
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut buf = [0u8; 43];
    match reader.read_exact(&mut buf) {
        Err(e) => {
            error!("Stream failure reading bytes of Orchard raw address: {}", e);
            std::ptr::null_mut()
        }
        Ok(()) => {
            let read = Address::from_raw_address_bytes(&buf);
            if read.is_some().into() {
                Box::into_raw(Box::new(read.unwrap()))
            } else {
                error!("Failed to parse Orchard raw address.");
                std::ptr::null_mut()
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_raw_address_serialize(
    key: *const Address,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let key = unsafe { key.as_ref() }.expect("Orchard raw address pointer may not be null.");

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match writer.write_all(&key.to_raw_address_bytes()) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard raw address: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_address_eq(a0: *const Address, a1: *const Address) -> bool {
    let a0 = unsafe { a0.as_ref() };
    let a1 = unsafe { a1.as_ref() };
    a0 == a1
}

#[no_mangle]
pub extern "C" fn orchard_address_lt(a0: *const Address, a1: *const Address) -> bool {
    let a0 = unsafe { a0.as_ref() };
    let a1 = unsafe { a1.as_ref() };
    a0.map(|a| OrderedAddress::new(*a)) < a1.map(|a| OrderedAddress::new(*a))
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
pub extern "C" fn orchard_incoming_viewing_key_to_address(
    key: *const IncomingViewingKey,
    diversifier_index: *const [u8; 11],
) -> *mut Address {
    let key =
        unsafe { key.as_ref() }.expect("Orchard incoming viewing key pointer may not be null.");

    let diversifier_index = DiversifierIndex::from(unsafe { *diversifier_index });
    Box::into_raw(Box::new(key.address_at(diversifier_index)))
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_serialize(
    key: *const IncomingViewingKey,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let key =
        unsafe { key.as_ref() }.expect("Orchard incoming viewing key pointer may not be null.");

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match writer.write_all(&key.to_bytes()) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard incoming viewing key: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_lt(
    k0: *const IncomingViewingKey,
    k1: *const IncomingViewingKey,
) -> bool {
    let k0 = unsafe { k0.as_ref() };
    let k1 = unsafe { k1.as_ref() };
    k0 < k1
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_eq(
    k0: *const IncomingViewingKey,
    k1: *const IncomingViewingKey,
) -> bool {
    let k0 = unsafe { k0.as_ref() };
    let k1 = unsafe { k1.as_ref() };
    k0 == k1
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
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

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
    let key = unsafe { key.as_ref() }.expect("Orchard full viewing key pointer may not be null.");

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match key.write(&mut writer) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard full viewing key: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_to_incoming_viewing_key(
    key: *const FullViewingKey,
) -> *mut IncomingViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(IncomingViewingKey::from(key))))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_full_viewing_key_eq(
    k0: *const FullViewingKey,
    k1: *const FullViewingKey,
) -> bool {
    let k0 = unsafe { k0.as_ref() };
    let k1 = unsafe { k1.as_ref() };
    k0 == k1
}

//
// Spending keys
//

#[no_mangle]
pub extern "C" fn orchard_spending_key_for_account(
    seed: *const u8,
    seed_len: usize,
    bip44_coin_type: u32,
    account_id: u32,
) -> *mut SpendingKey {
    let seed = unsafe { slice::from_raw_parts(seed, seed_len) };
    SpendingKey::from_zip32_seed(seed, bip44_coin_type, account_id)
        .map(|key| Box::into_raw(Box::new(key)))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_spending_key_clone(key: *const SpendingKey) -> *mut SpendingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(*key)))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_spending_key_free(key: *mut SpendingKey) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_spending_key_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut SpendingKey {
    let mut reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    let mut buf = [0u8; 32];
    match reader.read_exact(&mut buf) {
        Err(e) => {
            error!(
                "Stream failure reading bytes of Orchard spending key: {}",
                e
            );
            std::ptr::null_mut()
        }
        Ok(()) => {
            let sk_opt = SpendingKey::from_bytes(buf);
            if sk_opt.is_some().into() {
                Box::into_raw(Box::new(sk_opt.unwrap()))
            } else {
                error!("Failed to parse Orchard spending key.");
                std::ptr::null_mut()
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_spending_key_serialize(
    key: *const SpendingKey,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let key = unsafe { key.as_ref() }.expect("Orchard spending key pointer may not be null.");

    let mut writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match writer.write_all(key.to_bytes()) {
        Ok(()) => true,
        Err(e) => {
            error!("Stream failure writing Orchard spending key: {}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_spending_key_to_full_viewing_key(
    key: *const SpendingKey,
) -> *mut FullViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(FullViewingKey::from(key))))
        .unwrap_or(std::ptr::null_mut())
}
