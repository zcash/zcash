use orchard::keys::IncomingViewingKey;

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_clone(
    key: *const IncomingViewingKey,
) -> *mut IncomingViewingKey {
    unsafe { key.as_ref() }
        .map(|key| Box::into_raw(Box::new(key.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_incoming_viewing_key_free(
    key: *mut IncomingViewingKey
) {
    if !key.is_null() {
        drop(unsafe { Box::from_raw(key) });
    }
}

