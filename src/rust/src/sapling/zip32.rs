use zcash_primitives::{
    sapling::{keys::FullViewingKey, Diversifier},
    zip32::{self, sapling_address, sapling_derive_internal_fvk, sapling_find_address},
};

#[cxx::bridge]
mod ffi {
    struct FfiFullViewingKey {
        fvk: [u8; 96],
        dk: [u8; 32],
    }

    struct FfiPaymentAddress {
        j: [u8; 11],
        addr: [u8; 43],
    }

    #[namespace = "sapling::zip32"]
    extern "Rust" {
        fn xsk_master(seed: &[u8]) -> [u8; 169];
        fn xsk_derive(xsk_parent: &[u8; 169], i: u32) -> [u8; 169];
        fn xsk_derive_internal(xsk_external: &[u8; 169]) -> [u8; 169];
        fn xfvk_derive(xfvk_parent: &[u8; 169], i: u32) -> Result<[u8; 169]>;
        fn derive_internal_fvk(fvk: &[u8; 96], dk: [u8; 32]) -> FfiFullViewingKey;
        fn address(fvk: &[u8; 96], dk: [u8; 32], j: [u8; 11]) -> Result<[u8; 43]>;
        fn find_address(fvk: &[u8; 96], dk: [u8; 32], j: [u8; 11]) -> Result<FfiPaymentAddress>;
        fn diversifier_index(dk: [u8; 32], d: [u8; 11]) -> [u8; 11];
    }
}

/// Derives the master ExtendedSpendingKey from a seed.
fn xsk_master(seed: &[u8]) -> [u8; 169] {
    let xsk = zip32::ExtendedSpendingKey::master(seed);

    let mut xsk_master = [0; 169];
    xsk.write(&mut xsk_master[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
    xsk_master
}

/// Derive a child ExtendedSpendingKey from a parent.
fn xsk_derive(xsk_parent: &[u8; 169], i: u32) -> [u8; 169] {
    let xsk_parent =
        zip32::ExtendedSpendingKey::read(&xsk_parent[..]).expect("valid ExtendedSpendingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xsk = xsk_parent.derive_child(i);

    let mut xsk_i = [0; 169];
    xsk.write(&mut xsk_i[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
    xsk_i
}

/// Derive the Sapling internal spending key from the external extended
/// spending key
fn xsk_derive_internal(xsk_external: &[u8; 169]) -> [u8; 169] {
    let xsk_external =
        zip32::ExtendedSpendingKey::read(&xsk_external[..]).expect("valid ExtendedSpendingKey");

    let xsk_internal = xsk_external.derive_internal();

    let mut xsk_internal_ret = [0; 169];
    xsk_internal
        .write(&mut xsk_internal_ret[..])
        .expect("should be able to serialize an ExtendedSpendingKey");
    xsk_internal_ret
}

/// Derive a child ExtendedFullViewingKey from a parent.
fn xfvk_derive(xfvk_parent: &[u8; 169], i: u32) -> Result<[u8; 169], String> {
    let xfvk_parent = zip32::ExtendedFullViewingKey::read(&xfvk_parent[..])
        .expect("valid ExtendedFullViewingKey");
    let i = zip32::ChildIndex::from_index(i);

    let xfvk = xfvk_parent
        .derive_child(i)
        .map_err(|()| "Cannot derive hardened xfvk".to_string())?;

    let mut xfvk_i = [0; 169];
    xfvk.write(&mut xfvk_i[..])
        .expect("should be able to serialize an ExtendedFullViewingKey");
    Ok(xfvk_i)
}

/// Derive the Sapling internal full viewing key from the corresponding external full viewing key
fn derive_internal_fvk(fvk: &[u8; 96], dk: [u8; 32]) -> ffi::FfiFullViewingKey {
    let fvk = FullViewingKey::read(&fvk[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(dk);

    let (fvk_internal, dk_internal) = sapling_derive_internal_fvk(&fvk, &dk);

    ffi::FfiFullViewingKey {
        fvk: fvk_internal.to_bytes(),
        dk: *dk_internal.as_bytes(),
    }
}

/// Derive a PaymentAddress from an ExtendedFullViewingKey.
fn address(fvk: &[u8; 96], dk: [u8; 32], j: [u8; 11]) -> Result<[u8; 43], String> {
    let fvk = FullViewingKey::read(&fvk[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(dk);
    let j = zip32::DiversifierIndex(j);

    sapling_address(&fvk, &dk, j)
        .ok_or_else(|| "Diversifier index does not produce a valid diversifier".to_string())
        .map(|addr| addr.to_bytes())
}

/// Derive a PaymentAddress from an ExtendedFullViewingKey.
fn find_address(
    fvk: &[u8; 96],
    dk: [u8; 32],
    j: [u8; 11],
) -> Result<ffi::FfiPaymentAddress, String> {
    let fvk = FullViewingKey::read(&fvk[..]).expect("valid Sapling FullViewingKey");
    let dk = zip32::sapling::DiversifierKey::from_bytes(dk);
    let j = zip32::DiversifierIndex(j);

    sapling_find_address(&fvk, &dk, j)
        .ok_or_else(|| "No valid diversifiers at or above given index".to_string())
        .map(|(j, addr)| ffi::FfiPaymentAddress {
            j: j.0,
            addr: addr.to_bytes(),
        })
}

fn diversifier_index(dk: [u8; 32], d: [u8; 11]) -> [u8; 11] {
    let dk = zip32::sapling::DiversifierKey::from_bytes(dk);
    let diversifier = Diversifier(d);

    let j = dk.diversifier_index(&diversifier);
    j.0
}
