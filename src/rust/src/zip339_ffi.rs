use libc::{c_char, size_t};
use std::{
    borrow::Cow,
    ffi::{CStr, CString},
    ptr, slice,
};
use zeroize::Zeroize;

// It's safer to use a wrapper type here than an enum. We can't stop a C caller from passing
// an unrecognized value as a `language` parameter; if it were an enum on the Rust side,
// then that would immediately be UB, whereas this way we can return null or false.
#[repr(C)]
#[derive(Copy, Clone)]
pub struct Language(pub u32);

impl Language {
    #[allow(clippy::too_many_arguments)]
    fn handle<Ctx, T>(
        self,
        ctx: Ctx,
        en: impl FnOnce(Ctx) -> Option<T>,
        zh_cn: impl FnOnce(Ctx) -> Option<T>,
        zh_tw: impl FnOnce(Ctx) -> Option<T>,
        cs: impl FnOnce(Ctx) -> Option<T>,
        fr: impl FnOnce(Ctx) -> Option<T>,
        it: impl FnOnce(Ctx) -> Option<T>,
        ja: impl FnOnce(Ctx) -> Option<T>,
        ko: impl FnOnce(Ctx) -> Option<T>,
        pt: impl FnOnce(Ctx) -> Option<T>,
        es: impl FnOnce(Ctx) -> Option<T>,
    ) -> Option<T> {
        // These must match `src/rust/include/zip339.h`.
        match self {
            Language(0) => en(ctx),
            Language(1) => zh_cn(ctx),
            Language(2) => zh_tw(ctx),
            Language(3) => cs(ctx),
            Language(4) => fr(ctx),
            Language(5) => it(ctx),
            Language(6) => ja(ctx),
            Language(7) => ko(ctx),
            Language(8) => pt(ctx),
            Language(9) => es(ctx),
            Language(_) => None,
        }
    }

    fn with_mnemonic_phrase_from_entropy<E: Into<Vec<u8>>, T>(
        self,
        entropy: E,
        f: impl FnOnce(&str) -> Option<T>,
    ) -> Option<T> {
        self.handle(
            (entropy, f),
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::English>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::ChineseSimplified>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::ChineseTraditional>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Czech>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::French>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Italian>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Japanese>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Korean>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Portuguese>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
            |(entropy, f)| {
                bip0039::Mnemonic::<bip0039::Spanish>::from_entropy(entropy)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.phrase()))
            },
        )
    }

    fn with_seed_from_mnemonic_phrase<'a, P: Into<Cow<'a, str>>, T>(
        self,
        phrase: P,
        passphrase: &str,
        f: impl FnOnce([u8; 64]) -> Option<T>,
    ) -> Option<T> {
        self.handle(
            (phrase, passphrase, f),
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::English>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::ChineseSimplified>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::ChineseTraditional>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Czech>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::French>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Italian>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Japanese>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Korean>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Portuguese>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
            |(phrase, passphrase, f)| {
                bip0039::Mnemonic::<bip0039::Spanish>::from_phrase(phrase)
                    .ok()
                    .and_then(|mnemonic| f(mnemonic.to_seed(passphrase)))
            },
        )
    }

    fn validate_mnemonic<'a, P: Into<Cow<'a, str>>>(self, phrase: P) -> Option<()> {
        self.handle(
            phrase,
            |phrase| bip0039::Mnemonic::<bip0039::English>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::ChineseSimplified>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::ChineseTraditional>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Czech>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::French>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Italian>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Japanese>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Korean>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Portuguese>::validate(phrase).ok(),
            |phrase| bip0039::Mnemonic::<bip0039::Spanish>::validate(phrase).ok(),
        )
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

    let entropy = unsafe { slice::from_raw_parts(entropy, entropy_len) }.to_vec();
    language
        .with_mnemonic_phrase_from_entropy(entropy, |phrase| {
            CString::new(phrase)
                .ok()
                .map(|phrase| phrase.into_raw() as *const c_char)
        })
        .unwrap_or(ptr::null())
}

/// Frees a phrase returned by `zip339_entropy_to_phrase`.
#[no_mangle]
pub extern "C" fn zip339_free_phrase(phrase: *const c_char) {
    if !phrase.is_null() {
        unsafe {
            // It is correct to cast away const here; the memory is not actually immutable.
            CString::from_raw(phrase as *mut c_char)
                .into_bytes()
                .zeroize();
        }
    }
}

/// Returns `true` if the given string is a valid mnemonic phrase in the given `Language`.
#[no_mangle]
pub extern "C" fn zip339_validate_phrase(language: Language, phrase: *const c_char) -> bool {
    assert!(!phrase.is_null());

    if let Ok(phrase) = unsafe { CStr::from_ptr(phrase) }.to_str() {
        return language.validate_mnemonic(phrase).is_some();
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

    let phrase = unsafe { CStr::from_ptr(phrase) };
    phrase
        .to_str()
        .ok()
        .and_then(|phrase| {
            // Use the empty passphrase.
            language.with_seed_from_mnemonic_phrase(phrase, "", |seed| {
                unsafe {
                    ptr::copy(seed.as_ptr(), buf, 64);
                }
                Some(true)
            })
        })
        .unwrap_or_else(|| {
            unsafe {
                ptr::write_bytes(buf, 0, 64);
            }
            false
        })
}
