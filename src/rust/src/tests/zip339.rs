use libc::c_char;
use std::{
    convert::{TryFrom, TryInto},
    ffi::CStr,
    ptr,
};

use crate::zip339_ffi::{
    zip339_entropy_to_phrase, zip339_free_phrase, zip339_phrase_to_seed, zip339_validate_phrase,
    Language,
};
use zcash_primitives::zip339;

#[test]
fn test_try_from_language() {
    assert_eq!(Language(0).try_into(), Ok(zip339::Language::English));
    assert!(zip339::Language::try_from(Language(1234)).is_err());
}

#[test]
#[should_panic]
fn test_null_entropy_to_phrase_panics() {
    zip339_entropy_to_phrase(Language(0), ptr::null(), 0);
}

#[test]
fn test_free_null_phrase_is_noop() {
    zip339_free_phrase(ptr::null_mut());
}

#[test]
#[should_panic]
fn test_validate_null_phrase_panics() {
    zip339_validate_phrase(Language(0), ptr::null());
}

#[test]
#[should_panic]
fn test_null_phrase_to_seed_panics() {
    zip339_phrase_to_seed(Language(0), ptr::null(), ptr::NonNull::dangling().as_ptr());
}

#[test]
#[should_panic]
fn test_phrase_to_seed_with_null_buffer_panics() {
    zip339_phrase_to_seed(
        Language(0),
        ptr::NonNull::dangling().as_ptr(),
        ptr::null_mut(),
    );
}

#[test]
fn test_known_answers() {
    let mut entropy = [0u8; 32];

    // English and another language with non-Latin script.
    let expected_phrase_en = "abandon abandon abandon abandon abandon abandon abandon abandon \
        abandon abandon abandon abandon abandon abandon abandon abandon \
        abandon abandon abandon abandon abandon abandon abandon art";
    let expected_phrase_ko = "가격 가격 가격 가격 가격 가격 가격 가격 \
        가격 가격 가격 가격 가격 가격 가격 가격 \
        가격 가격 가격 가격 가격 가격 가격 계단";

    let expected_seed_en = [
        0x40, 0x8B, 0x28, 0x5C, 0x12, 0x38, 0x36, 0x00, 0x4F, 0x4B, 0x88, 0x42, 0xC8, 0x93, 0x24,
        0xC1, 0xF0, 0x13, 0x82, 0x45, 0x0C, 0x0D, 0x43, 0x9A, 0xF3, 0x45, 0xBA, 0x7F, 0xC4, 0x9A,
        0xCF, 0x70, 0x54, 0x89, 0xC6, 0xFC, 0x77, 0xDB, 0xD4, 0xE3, 0xDC, 0x1D, 0xD8, 0xCC, 0x6B,
        0xC9, 0xF0, 0x43, 0xDB, 0x8A, 0xDA, 0x1E, 0x24, 0x3C, 0x4A, 0x0E, 0xAF, 0xB2, 0x90, 0xD3,
        0x99, 0x48, 0x08, 0x40,
    ];
    let expected_seed_ko = [
        0xD6, 0x93, 0x5F, 0x34, 0x3A, 0xC6, 0x66, 0x97, 0x58, 0x06, 0xDF, 0xA7, 0xBD, 0x0E, 0x45,
        0xF8, 0x3A, 0x6F, 0x0A, 0x1A, 0x9F, 0x48, 0x54, 0xF4, 0x40, 0xEC, 0x09, 0x0B, 0xE3, 0xEF,
        0x16, 0xEA, 0x09, 0x42, 0x03, 0xEC, 0x07, 0xA1, 0x7D, 0x8B, 0x43, 0x9B, 0xBD, 0x2F, 0x89,
        0x3D, 0xB8, 0xB8, 0x3D, 0x3E, 0x43, 0xD4, 0x59, 0xA3, 0x6C, 0x29, 0xDF, 0xAB, 0xC2, 0xF9,
        0xF5, 0xB2, 0x47, 0x72,
    ];

    for &(expected_phrase, expected_seed, language) in [
        (expected_phrase_en, expected_seed_en, Language(0)),
        (expected_phrase_ko, expected_seed_ko, Language(7)),
    ]
    .iter()
    {
        // Testing these all together simplifies memory management in the test code.

        // test zip339_entropy_to_phrase
        let phrase = zip339_entropy_to_phrase(language, entropy[..].as_mut_ptr(), 32);
        assert_eq!(
            unsafe { CStr::from_ptr(phrase) }.to_str().unwrap(),
            expected_phrase
        );

        // test zip339_validate_phrase
        assert!(zip339_validate_phrase(language, phrase));
        assert!(!zip339_validate_phrase(Language(1), phrase));
        assert!(!zip339_validate_phrase(Language(1234), phrase));

        // test zip339_phrase_to_seed
        let mut seed = [0u8; 64];
        assert!(zip339_phrase_to_seed(
            language,
            phrase,
            seed[..].as_mut_ptr()
        ));
        assert_eq!(seed, expected_seed);

        // test that zip339_phrase_to_seed zeros buffer on failure
        let mut seed = [0xFFu8; 64];
        assert!(!zip339_phrase_to_seed(
            Language(1),
            phrase,
            seed[..].as_mut_ptr()
        ));
        assert_eq!(seed, [0u8; 64]);

        zip339_free_phrase(phrase as *mut c_char);
    }
}
