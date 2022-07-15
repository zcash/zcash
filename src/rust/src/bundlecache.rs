use std::{
    convert::TryInto,
    sync::{Once, RwLock, RwLockReadGuard, RwLockWriteGuard},
};

use rand_core::{OsRng, RngCore};

#[cxx::bridge]
mod ffi {
    #[namespace = "libzcash"]
    unsafe extern "C++" {
        include!("zcash/cache.h");

        type BundleValidityCache;

        fn NewBundleValidityCache(kind: &str, bytes: usize) -> UniquePtr<BundleValidityCache>;
        fn insert(self: Pin<&mut BundleValidityCache>, entry: [u8; 32]);
        fn contains(&self, entry: &[u8; 32], erase: bool) -> bool;
    }
    #[namespace = "bundlecache"]
    extern "Rust" {
        fn init(cache_bytes: usize);
    }
}

pub(crate) struct CacheEntry([u8; 32]);

pub(crate) enum CacheEntries {
    Storing(Vec<CacheEntry>),
    NotStoring,
}

impl CacheEntries {
    pub(crate) fn new(cache_store: bool) -> Self {
        if cache_store {
            CacheEntries::Storing(vec![])
        } else {
            CacheEntries::NotStoring
        }
    }
}

pub(crate) struct BundleValidityCache {
    hasher: blake2b_simd::State,
    cache: cxx::UniquePtr<ffi::BundleValidityCache>,
}

impl BundleValidityCache {
    fn new(kind: &'static str, personalization: &[u8; 16], cache_bytes: usize) -> Self {
        // Use BLAKE2b to produce entries from bundles. It has a block size of 128 bytes,
        // into which we put:
        // - 32 byte nonce
        // - 64 bytes of bundle commitments
        // - 32 byte sighash
        let mut hasher = blake2b_simd::Params::new()
            .hash_length(32)
            .personal(personalization)
            .to_state();

        // Pre-load the hasher with a per-instance nonce. This ensures that cache entries
        // are deterministic but also unique per node.
        let mut nonce = [0; 32];
        OsRng.fill_bytes(&mut nonce);
        hasher.update(&nonce);

        Self {
            hasher,
            cache: ffi::NewBundleValidityCache(kind, cache_bytes),
        }
    }

    pub(crate) fn compute_entry(
        &self,
        bundle_commitment: &[u8; 32],
        bundle_authorizing_commitment: &[u8; 32],
        sighash: &[u8; 32],
    ) -> CacheEntry {
        self.hasher
            .clone()
            .update(bundle_commitment)
            .update(bundle_authorizing_commitment)
            .update(sighash)
            .finalize()
            .as_bytes()
            .try_into()
            .map(CacheEntry)
            .expect("BLAKE2b configured with hash length of 32 so conversion cannot fail")
    }

    pub(crate) fn insert(&mut self, queued_entries: CacheEntries) {
        if let CacheEntries::Storing(cache_entries) = queued_entries {
            for cache_entry in cache_entries {
                self.cache.pin_mut().insert(cache_entry.0);
            }
        }
    }

    pub(crate) fn contains(&self, entry: CacheEntry, queued_entries: &mut CacheEntries) -> bool {
        if self
            .cache
            .contains(&entry.0, matches!(queued_entries, CacheEntries::NotStoring))
        {
            true
        } else {
            if let CacheEntries::Storing(cache_entries) = queued_entries {
                cache_entries.push(entry);
            }
            false
        }
    }
}

static BUNDLE_CACHES_LOADED: Once = Once::new();
static mut SAPLING_BUNDLE_VALIDITY_CACHE: Option<RwLock<BundleValidityCache>> = None;
static mut ORCHARD_BUNDLE_VALIDITY_CACHE: Option<RwLock<BundleValidityCache>> = None;

fn init(cache_bytes: usize) {
    BUNDLE_CACHES_LOADED.call_once(|| unsafe {
        SAPLING_BUNDLE_VALIDITY_CACHE = Some(RwLock::new(BundleValidityCache::new(
            "Sapling",
            b"SaplingVeriCache",
            cache_bytes,
        )));
        ORCHARD_BUNDLE_VALIDITY_CACHE = Some(RwLock::new(BundleValidityCache::new(
            "Orchard",
            b"OrchardVeriCache",
            cache_bytes,
        )));
    });
}

pub(crate) fn sapling_bundle_validity_cache() -> RwLockReadGuard<'static, BundleValidityCache> {
    unsafe { SAPLING_BUNDLE_VALIDITY_CACHE.as_ref() }
        .expect("bundlecache::init() should have been called")
        .read()
        .unwrap()
}

pub(crate) fn sapling_bundle_validity_cache_mut() -> RwLockWriteGuard<'static, BundleValidityCache>
{
    unsafe { SAPLING_BUNDLE_VALIDITY_CACHE.as_mut() }
        .expect("bundlecache::init() should have been called")
        .write()
        .unwrap()
}

pub(crate) fn orchard_bundle_validity_cache() -> RwLockReadGuard<'static, BundleValidityCache> {
    unsafe { ORCHARD_BUNDLE_VALIDITY_CACHE.as_ref() }
        .expect("bundlecache::init() should have been called")
        .read()
        .unwrap()
}

pub(crate) fn orchard_bundle_validity_cache_mut() -> RwLockWriteGuard<'static, BundleValidityCache>
{
    unsafe { ORCHARD_BUNDLE_VALIDITY_CACHE.as_mut() }
        .expect("bundlecache::init() should have been called")
        .write()
        .unwrap()
}
