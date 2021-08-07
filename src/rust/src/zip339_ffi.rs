use libc::{c_char, size_t};
use std::{
    convert::{TryFrom, TryInto},
    ffi::{CStr, CString},
    ptr, slice,
};

use zcash_primitives::zip339;

// It's safer to use a wrapper type here than an enum. We can't stop a C caller from passing
// an unrecognized value as a `language` parameter; if it were an enum on the Rust side,
// then that would immediately be UB, whereas this way we can return null or false.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct Language(pub u32);

impl TryFrom<Language> for zip339::Language {
    type Error = ();

    fn try_from(language: Language) -> Result<Self, ()> {
        // These must match `src/rust/include/zip339.h`.
        match language {
            Language(0) => Ok(zip339::Language::English),
            Language(1) => Ok(zip339::Language::SimplifiedChinese),
            Language(2) => Ok(zip339::Language::TraditionalChinese),
            Language(3) => Ok(zip339::Language::Czech),
            Language(4) => Ok(zip339::Language::French),
            Language(5) => Ok(zip339::Language::Italian),
            Language(6) => Ok(zip339::Language::Japanese),
            Language(7) => Ok(zip339::Language::Korean),
            Language(8) => Ok(zip339::Language::Portuguese),
            Language(9) => Ok(zip339::Language::Spanish),
            Language(_) => Err(()),
        }
    }
}

/// Creates a phrase with the given entropy, in the given `Language`. The phrase is represented as
/// a pointer to a null-terminated C string that must not be written to, and must be freed by
/// `zip339_free_phrase`.
#[no_mangle]
pub extern "C" fn zip339_entropy_to_phrase(
    language: Language,
    entropy: *const u8,
    entropy_len: size_t,
) -> *const c_char {
    assert!(!entropy.is_null());

    if let Ok(language) = language.try_into() {
        let entropy = unsafe { slice::from_raw_parts(entropy, entropy_len) }.to_vec();
        if let Ok(mnemonic) = zip339::Mnemonic::from_entropy_in(language, entropy) {
            if let Ok(phrase) = CString::new(mnemonic.phrase()) {
                return phrase.into_raw();
            }
        }
    }
    ptr::null()
}

/// Frees a phrase returned by `zip339_entropy_to_phrase`.
#[no_mangle]
pub extern "C" fn zip339_free_phrase(phrase: *mut c_char) {
    if !phrase.is_null() {
        unsafe {
            CString::from_raw(phrase);
        }
    }
}

/// Returns `true` if the given string is a valid mnemonic phrase in the given `Language`.
#[no_mangle]
pub extern "C" fn zip339_validate_phrase(language: Language, phrase: *const c_char) -> bool {
    assert!(!phrase.is_null());

    if let Ok(language) = language.try_into() {
        if let Ok(phrase) = unsafe { CStr::from_ptr(phrase) }.to_str() {
            return zip339::Mnemonic::validate_in(language, phrase).is_ok();
        }
    }
    false
}

/// Copies the seed for the given phrase into the 64-byte buffer `buf`.
/// Returns true if successful, false on error. In case of error, `buf` is zeroed.
#[no_mangle]
pub extern "C" fn zip339_phrase_to_seed(
    language: Language,
    phrase: *const c_char,
    buf: *mut u8,
) -> bool {
    assert!(!phrase.is_null());
    assert!(!buf.is_null());

    if let Ok(language) = language.try_into() {
        if let Ok(phrase) = unsafe { CStr::from_ptr(phrase) }.to_str() {
            if let Ok(mnemonic) = zip339::Mnemonic::from_phrase_in(language, phrase) {
                // Use the empty passphrase.
                let seed = mnemonic.to_seed("");
                unsafe {
                    ptr::copy(seed.as_ptr(), buf, 64);
                }
                return true;
            }
        }
    }
    unsafe {
        ptr::write_bytes(buf, 0, 64);
    }
    false
}
