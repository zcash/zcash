// Copyright (c) 2020 The Zcash developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php .

use ed25519_zebra::{Signature, SigningKey, VerificationKey};

#[cfg(feature = "wasm")]
#[allow(non_camel_case_types)]
#[cfg(feature = "wasm")]
type c_uchar = u8;
#[cfg(feature = "wasm")]
#[allow(non_camel_case_types)]
type size_t = usize;

#[cfg(not(feature = "wasm"))]
use libc::{c_uchar, size_t};

use rand_core::OsRng;
use std::convert::TryFrom;
use std::slice;

#[no_mangle]
pub extern "C" fn ed25519_generate_keypair(sk: *mut [u8; 32], vk: *mut [u8; 32]) {
    let sk = unsafe { sk.as_mut() }.unwrap();
    let vk = unsafe { vk.as_mut() }.unwrap();

    let signing_key = SigningKey::new(OsRng);

    *sk = signing_key.into();
    *vk = VerificationKey::from(&signing_key).into();
}

#[no_mangle]
pub extern "C" fn ed25519_sign(
    sk: *const [u8; 32],
    msg: *const c_uchar,
    msg_len: size_t,
    signature: *mut [u8; 64],
) -> bool {
    let sk = SigningKey::from(*unsafe { sk.as_ref() }.unwrap());

    let msg = unsafe { slice::from_raw_parts(msg, msg_len) };

    let signature = unsafe {
        match signature.as_mut() {
            Some(sig) => sig,
            None => return false,
        }
    };

    *signature = sk.sign(msg).into();

    true
}

#[no_mangle]
pub extern "C" fn ed25519_verify(
    vk: *const [u8; 32],
    signature: *const [u8; 64],
    msg: *const c_uchar,
    msg_len: size_t,
) -> bool {
    let vk = match VerificationKey::try_from(*unsafe { vk.as_ref() }.unwrap()) {
        Ok(vk) => vk,
        Err(_) => return false,
    };

    let signature = Signature::from(*unsafe {
        match signature.as_ref() {
            Some(sig) => sig,
            None => return false,
        }
    });

    let msg = unsafe { slice::from_raw_parts(msg, msg_len) };

    vk.verify(&signature, msg).is_ok()
}
