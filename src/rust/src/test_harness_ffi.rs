use group::{ff::Field, Group, GroupEncoding};
use jubjub::{ExtendedPoint, Fq};
use rand_core::OsRng;

#[no_mangle]
pub extern "C" fn zcash_test_harness_random_jubjub_base(ret: *mut [u8; 32]) {
    *unsafe { &mut *ret } = Fq::random(OsRng).to_bytes();
}

#[no_mangle]
pub extern "C" fn zcash_test_harness_random_jubjub_point(ret: *mut [u8; 32]) {
    *unsafe { &mut *ret } = ExtendedPoint::random(OsRng).to_bytes();
}
