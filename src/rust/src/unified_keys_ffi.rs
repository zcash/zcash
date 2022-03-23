use libc::c_char;
use std::ffi::{CStr, CString};
use tracing::error;

use zcash_address::unified::{Container, Encoding, Fvk, Ufvk};
use zcash_primitives::legacy::keys::AccountPubKey;

use crate::address_ffi::network_from_cstr;

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_free(key: *mut Ufvk) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_clone(key: *const Ufvk) -> *mut Ufvk {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(key.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_parse(
    network: *const c_char,
    encoded: *const c_char,
) -> *mut Ufvk {
    let network = match network_from_cstr(network) {
        Some(n) => n,
        None => {
            return std::ptr::null_mut();
        }
    };

    match unsafe { CStr::from_ptr(encoded) }.to_str() {
        Ok(encoded) => match Ufvk::decode(encoded) {
            Ok((parsed_network, fvk)) if parsed_network == network => {
                // ZIP 316: Consumers MUST reject Unified Addresses/Viewing Keys in which
                // any constituent Item does not meet the validation requirements of its
                // encoding.
                for item in fvk.items() {
                    match item {
                        Fvk::Orchard(data) => {
                            if orchard::keys::FullViewingKey::from_bytes(&data).is_none() {
                                error!("Unified FVK contains invalid Orchard FVK");
                                return std::ptr::null_mut();
                            }
                        }
                        Fvk::Sapling(data) => {
                            // The last 32 bytes is the diversifier key, which is opaque.
                            // The remaining 96 bytes should be a valid Sapling FVK.
                            if zcash_primitives::sapling::keys::FullViewingKey::read(&data[..96])
                                .is_err()
                            {
                                error!("Unified FVK contains invalid Sapling FVK");
                                return std::ptr::null_mut();
                            }
                        }
                        Fvk::P2pkh(data) => {
                            // The first 32 bytes is the chaincode, which is opaque.
                            // The remaining 33 bytes should be the compressed encoding of
                            // a secp256k1 point.
                            if let Err(e) = secp256k1::PublicKey::from_slice(&data[32..]) {
                                error!("Unified FVK contains invalid transparent FVK: {}", e);
                                return std::ptr::null_mut();
                            }
                        }
                        // Can't check anything for unknown typecodes.
                        Fvk::Unknown { .. } => (),
                    }
                }
                Box::into_raw(Box::new(fvk))
            }
            Ok((parsed_network, _)) => {
                error!(
                    "Key was encoded for a different network ({:?}) than what was requested ({:?})",
                    parsed_network, network,
                );
                std::ptr::null_mut()
            }
            Err(e) => {
                error!("Failure decoding unified full viewing key: {}", e);
                std::ptr::null_mut()
            }
        },
        Err(e) => {
            error!("Failure reading bytes of unified full viewing key: {}", e);
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_serialize(
    network: *const c_char,
    key: *const Ufvk,
) -> *mut c_char {
    let key = unsafe { key.as_ref() }.expect("Unified full viewing key pointer may not be null.");
    match network_from_cstr(network) {
        Some(n) => CString::new(key.encode(&n)).unwrap().into_raw(),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_read_transparent(
    key: *const Ufvk,
    out: *mut [u8; 65],
) -> bool {
    let key = unsafe { key.as_ref() }.expect("Unified full viewing key pointer may not be null.");
    let out = unsafe { &mut *out };

    for r in &key.items() {
        if let Fvk::P2pkh(data) = r {
            *out = *data;
            return true;
        }
    }

    false
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_read_sapling(
    key: *const Ufvk,
    out: *mut [u8; 128],
) -> bool {
    let key = unsafe { key.as_ref() }.expect("Unified full viewing key pointer may not be null.");
    let out = unsafe { &mut *out };

    for r in &key.items() {
        if let Fvk::Sapling(data) = r {
            *out = *data;
            return true;
        }
    }

    false
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_read_orchard(
    key: *const Ufvk,
    out: *mut [u8; 96],
) -> bool {
    let key = unsafe { key.as_ref() }.expect("Unified full viewing key pointer may not be null.");
    let out = unsafe { &mut *out };

    for r in &key.items() {
        if let Fvk::Orchard(data) = r {
            *out = *data;
            return true;
        }
    }

    false
}

#[no_mangle]
pub extern "C" fn unified_full_viewing_key_from_components(
    t_key: *const [u8; 65],
    sapling_key: *const [u8; 128],
    orchard_key: *const [u8; 96],
) -> *mut Ufvk {
    let t_key = unsafe { t_key.as_ref() };
    let sapling_key = unsafe { sapling_key.as_ref() };
    let orchard_key = unsafe { orchard_key.as_ref() };

    let mut items = vec![];
    if let Some(t_bytes) = t_key {
        items.push(Fvk::P2pkh(*t_bytes));
    }
    if let Some(sapling_bytes) = sapling_key {
        items.push(Fvk::Sapling(*sapling_bytes));
    }
    if let Some(orchard_bytes) = orchard_key {
        items.push(Fvk::Orchard(*orchard_bytes));
    }

    match Ufvk::try_from_items(items) {
        Ok(ufvk) => Box::into_raw(Box::new(ufvk)),
        Err(e) => {
            error!(
                "An error occurred constructing the unified full viewing key: {:?}",
                e
            );
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn transparent_key_ovks(
    t_key: *const [u8; 65],
    internal_ovk_ret: *mut [u8; 32],
    external_ovk_ret: *mut [u8; 32],
) -> bool {
    let key_bytes = unsafe { t_key.as_ref() }.expect("Transparent FVK pointer may not be null.");
    let internal_ovk = unsafe { &mut *internal_ovk_ret };
    let external_ovk = unsafe { &mut *external_ovk_ret };
    match AccountPubKey::deserialize(key_bytes) {
        Ok(epubkey) => {
            let (internal, external) = epubkey.ovks_for_shielding();
            *internal_ovk = internal.as_bytes();
            *external_ovk = external.as_bytes();
            true
        }
        Err(e) => {
            error!(
                "An error occurred parsing the transparent full viewing key: {:?}",
                e
            );
            false
        }
    }
}
