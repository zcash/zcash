use getrandom::getrandom;

#[cxx::bridge]
mod ffi {
    #[namespace = "rust"]
    extern "Rust" {
        fn getrandom(dest: &mut [u8]) -> Result<()>;
    }
}
