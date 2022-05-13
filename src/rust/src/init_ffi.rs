#[no_mangle]
pub extern "C" fn zcashd_init_rayon_threadpool() {
    rayon::ThreadPoolBuilder::new()
        .thread_name(|i| format!("zc-rayon-{}", i))
        .build_global()
        .expect("Only initialized once");
}
