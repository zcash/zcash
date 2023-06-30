#[cxx::bridge]
mod ffi {
    #[namespace = "init"]
    extern "Rust" {
        fn rayon_threadpool();
    }
}

fn rayon_threadpool() {
    rayon::ThreadPoolBuilder::new()
        .thread_name(|i| format!("zc-rayon-{}", i))
        .build_global()
        .expect("Only initialized once");
}
