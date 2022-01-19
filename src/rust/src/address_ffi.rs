use std::{
    convert::TryInto,
    ffi::{CStr, CString},
    ptr::{self, NonNull},
};

use libc::{c_char, c_void};
use zcash_address::{
    unified::{self, Container, Encoding},
    FromAddress, Network, ToAddress, ZcashAddress,
};
use zcash_primitives::sapling;

pub type UnifiedAddressObj = NonNull<c_void>;
pub type AddReceiverCb =
    unsafe extern "C" fn(ua: Option<UnifiedAddressObj>, raw: *const u8) -> bool;
pub type UnknownReceiverCb = unsafe extern "C" fn(
    ua: Option<UnifiedAddressObj>,
    typecode: u32,
    data: *const u8,
    len: usize,
) -> bool;
pub type GetTypecodeCb = unsafe extern "C" fn(ua: Option<UnifiedAddressObj>, index: usize) -> u32;
pub type GetReceiverLenCb =
    unsafe extern "C" fn(ua: Option<UnifiedAddressObj>, index: usize) -> usize;
pub type GetReceiverDataCb =
    unsafe extern "C" fn(ua: Option<UnifiedAddressObj>, index: usize, data: *mut u8, length: usize);

pub(crate) fn network_from_cstr(network: *const c_char) -> Option<Network> {
    match unsafe { CStr::from_ptr(network) }.to_str().unwrap() {
        "main" => Some(Network::Main),
        "test" => Some(Network::Test),
        "regtest" => Some(Network::Regtest),
        s => {
            tracing::error!("Unsupported network type string '{}'", s);
            None
        }
    }
}

struct UnifiedAddressHelper {
    net: Network,
    ua: unified::Address,
}

impl FromAddress for UnifiedAddressHelper {
    fn from_unified(
        net: Network,
        ua: unified::Address,
    ) -> Result<Self, zcash_address::UnsupportedAddress> {
        Ok(Self { net, ua })
    }
}

impl UnifiedAddressHelper {
    fn into_cpp(
        self,
        network: Network,
        ua_obj: Option<UnifiedAddressObj>,
        sapling_cb: Option<AddReceiverCb>,
        p2sh_cb: Option<AddReceiverCb>,
        p2pkh_cb: Option<AddReceiverCb>,
        unknown_cb: Option<UnknownReceiverCb>,
    ) -> bool {
        if self.net != network {
            tracing::error!(
                "Unified Address is for {:?} but node is on {:?}",
                self.net,
                network
            );
            return false;
        }

        self.ua
            .items()
            .into_iter()
            .map(|receiver| match receiver {
                unified::Receiver::Orchard(data) => {
                    // ZIP 316: Consumers MUST reject Unified Addresses/Viewing Keys in
                    // which any constituent Item does not meet the validation
                    // requirements of its encoding.
                    if orchard::Address::from_raw_address_bytes(&data)
                        .is_none()
                        .into()
                    {
                        tracing::error!("Unified Address contains invalid Orchard receiver");
                        false
                    } else {
                        unsafe {
                            // TODO: Replace with Orchard support.
                            (unknown_cb.unwrap())(ua_obj, 0x03, data.as_ptr(), data.len())
                        }
                    }
                }
                unified::Receiver::Sapling(data) => {
                    // ZIP 316: Consumers MUST reject Unified Addresses/Viewing Keys in
                    // which any constituent Item does not meet the validation
                    // requirements of its encoding.
                    if sapling::PaymentAddress::from_bytes(&data).is_none() {
                        tracing::error!("Unified Address contains invalid Sapling receiver");
                        false
                    } else {
                        unsafe { (sapling_cb.unwrap())(ua_obj, data.as_ptr()) }
                    }
                }
                unified::Receiver::P2sh(data) => unsafe {
                    (p2sh_cb.unwrap())(ua_obj, data.as_ptr())
                },
                unified::Receiver::P2pkh(data) => unsafe {
                    (p2pkh_cb.unwrap())(ua_obj, data.as_ptr())
                },
                unified::Receiver::Unknown { typecode, data } => unsafe {
                    (unknown_cb.unwrap())(ua_obj, typecode, data.as_ptr(), data.len())
                },
            })
            .all(|b| b)
    }
}

#[no_mangle]
pub extern "C" fn zcash_address_parse_unified(
    encoded: *const c_char,
    network: *const c_char,
    ua_obj: Option<UnifiedAddressObj>,
    sapling_cb: Option<AddReceiverCb>,
    p2sh_cb: Option<AddReceiverCb>,
    p2pkh_cb: Option<AddReceiverCb>,
    unknown_cb: Option<UnknownReceiverCb>,
) -> bool {
    let network = match network_from_cstr(network) {
        Some(net) => net,
        None => return false,
    };
    let encoded = unsafe { CStr::from_ptr(encoded) }.to_str().unwrap();

    let addr = match ZcashAddress::try_from_encoded(encoded) {
        Ok(addr) => addr,
        Err(e) => {
            tracing::error!("{}", e);
            return false;
        }
    };

    let ua: UnifiedAddressHelper = match addr.convert() {
        Ok(ua) => ua,
        Err(_) => {
            // `KeyIO::DecodePaymentAddress` handles the rest of the address kinds.
            return false;
        }
    };

    ua.into_cpp(network, ua_obj, sapling_cb, p2sh_cb, p2pkh_cb, unknown_cb)
}

#[no_mangle]
pub extern "C" fn zcash_address_serialize_unified(
    network: *const c_char,
    ua_obj: Option<UnifiedAddressObj>,
    receivers_len: usize,
    typecode_cb: Option<GetTypecodeCb>,
    receiver_len_cb: Option<GetReceiverLenCb>,
    receiver_cb: Option<GetReceiverDataCb>,
) -> *mut c_char {
    let network = match network_from_cstr(network) {
        Some(net) => net,
        None => return ptr::null_mut(),
    };

    let receivers: Vec<unified::Receiver> = match (0..receivers_len)
        .map(|i| {
            Ok(
                match unsafe { (typecode_cb.unwrap())(ua_obj, i) }.try_into()? {
                    unified::Typecode::Orchard => {
                        // TODO: Replace with Orchard support.
                        let data_len = unsafe { (receiver_len_cb.unwrap())(ua_obj, i) };
                        let mut data = vec![0; data_len];
                        unsafe { (receiver_cb.unwrap())(ua_obj, i, data.as_mut_ptr(), data_len) };
                        unified::Receiver::Unknown {
                            typecode: 0x03,
                            data,
                        }
                    }
                    unified::Typecode::Sapling => {
                        let mut data = [0; 43];
                        unsafe { (receiver_cb.unwrap())(ua_obj, i, data.as_mut_ptr(), data.len()) };
                        unified::Receiver::Sapling(data)
                    }
                    unified::Typecode::P2sh => {
                        let mut data = [0; 20];
                        unsafe { (receiver_cb.unwrap())(ua_obj, i, data.as_mut_ptr(), data.len()) };
                        unified::Receiver::P2sh(data)
                    }
                    unified::Typecode::P2pkh => {
                        let mut data = [0; 20];
                        unsafe { (receiver_cb.unwrap())(ua_obj, i, data.as_mut_ptr(), data.len()) };
                        unified::Receiver::P2pkh(data)
                    }
                    unified::Typecode::Unknown(typecode) => {
                        let data_len = unsafe { (receiver_len_cb.unwrap())(ua_obj, i) };
                        let mut data = vec![0; data_len];
                        unsafe { (receiver_cb.unwrap())(ua_obj, i, data.as_mut_ptr(), data_len) };
                        unified::Receiver::Unknown { typecode, data }
                    }
                },
            )
        })
        .collect::<Result<_, unified::ParseError>>()
    {
        Ok(receivers) => receivers,
        Err(e) => {
            tracing::error!("{}", e);
            return ptr::null_mut();
        }
    };

    let ua = match unified::Address::try_from_items(receivers) {
        Ok(ua) => ua,
        Err(e) => {
            tracing::error!("{}", e);
            return ptr::null_mut();
        }
    };

    let addr = ZcashAddress::from_unified(network, ua);

    CString::new(addr.to_string()).unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn zcash_address_string_free(encoded: *mut c_char) {
    let _ = unsafe { CString::from_raw(encoded) };
}
