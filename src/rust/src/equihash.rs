use tracing::error;
use zcash_primitives::block::equihash;

#[cxx::bridge]
mod ffi {
    #[namespace = "equihash"]
    extern "Rust" {
        fn is_valid(n: u32, k: u32, input: &[u8], nonce: &[u8], soln: &[u8]) -> bool;
    }
}

/// Validates the provided Equihash solution against the given parameters, input
/// and nonce.
fn is_valid(n: u32, k: u32, input: &[u8], nonce: &[u8], soln: &[u8]) -> bool {
    let expected_soln_len = (1 << k) * ((n / (k + 1)) as usize + 1) / 8;
    if (k >= n) || (n % 8 != 0) || (soln.len() != expected_soln_len) {
        error!(
            "equihash::is_valid: params wrong, n={}, k={}, soln_len={} expected={}",
            n,
            k,
            soln.len(),
            expected_soln_len,
        );
        return false;
    }
    if let Err(e) = equihash::is_valid_solution(n, k, input, nonce, soln) {
        error!("equihash::is_valid: is_valid_solution: {}", e);
        false
    } else {
        true
    }
}
