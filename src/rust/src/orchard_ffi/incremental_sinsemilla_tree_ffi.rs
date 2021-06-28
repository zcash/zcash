use bincode;
use incrementalmerkletree::{
    bridgetree::{self, BridgeTree},
    Altitude, Frontier, Hashable, Tree,
};
use std::mem::size_of;
use std::ptr;

use orchard::{bundle::Authorized, tree::OrchardIncrementalTreeDigest};

use zcash_primitives::transaction::components::Amount;

use crate::orchard_ffi::{error, CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

pub const MERKLE_DEPTH: u8 = 32;
pub const MAX_CHECKPOINTS: usize = 100;

//
// Operations on Merkle frontiers.
//

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_empty(
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    let empty_tree = bridgetree::Frontier::<OrchardIncrementalTreeDigest, MERKLE_DEPTH>::new();
    Box::into_raw(Box::new(empty_tree))
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_clone(
    tree: *const bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    unsafe { tree.as_ref() }
        .map(|tree| Box::into_raw(Box::new(tree.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_free(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) {
    if !tree.is_null() {
        drop(unsafe { Box::from_raw(tree) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match bincode::deserialize_from(reader) {
        Ok(parsed) => Box::into_raw(Box::new(parsed)),
        Err(e) => {
            error!("Failed to parse Orchard bundle: {}", e);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_serialize(
    tree: *const bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match bincode::serialize_into(writer, tree) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_append_bundle(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    bundle: *const orchard::Bundle<Authorized, Amount>,
) -> bool {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard note commitment tree pointer may not be null.")
    };
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        for action in bundle.actions().iter() {
            if !tree.append(&OrchardIncrementalTreeDigest::from_cmx(action.cmx())) {
                error!("Orchard note commitment tree is full.");
                return false;
            }
        }
    }
    
    true
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_root(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    root_ret: *mut [u8; 32],
) -> bool {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };

    if let Some(root) = tree.root().to_bytes() {
        root_ret.copy_from_slice(&root);
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_num_leaves(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) -> usize {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    tree.position().map_or(0, |p| <usize>::from(p) + 1)
}


#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_dynamic_mem_usage(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) -> usize {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    size_of::<Box<bridgetree::Frontier<OrchardIncrementalTreeDigest, MERKLE_DEPTH>>>()
        + tree.dynamic_memory_usage()
}

//
// Operations on incremental merkle trees with interstitial
// witnesses.
//

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_empty(
) -> *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    let empty_tree = BridgeTree::<OrchardIncrementalTreeDigest, MERKLE_DEPTH>::new(MAX_CHECKPOINTS);
    Box::into_raw(Box::new(empty_tree))
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_clone(
    tree: *const BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) -> *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    unsafe { tree.as_ref() }
        .map(|tree| Box::into_raw(Box::new(tree.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_free(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) {
    if !tree.is_null() {
        drop(unsafe { Box::from_raw(tree) });
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH> {
    let reader = CppStreamReader::from_raw_parts(stream, read_cb.unwrap());

    match bincode::deserialize_from(reader) {
        Ok(parsed) => Box::into_raw(Box::new(parsed)),
        Err(e) => {
            error!("Failed to parse Orchard bundle: {}", e);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_serialize(
    tree: *const BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    let writer = CppStreamWriter::from_raw_parts(stream, write_cb.unwrap());
    match bincode::serialize_into(writer, tree) {
        Ok(()) => true,
        Err(e) => {
            error!("{}", e);
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_append_bundle(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    bundle: *const orchard::Bundle<Authorized, Amount>,
) -> bool {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard note commitment tree pointer may not be null.")
    };
    if let Some(bundle) = unsafe { bundle.as_ref() } {
        for action in bundle.actions().iter() {
            if !tree.append(&OrchardIncrementalTreeDigest::from_cmx(action.cmx())) {
                error!("Orchard note commitment tree is full.");
                return false;
            }
        }
    } 

    true
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_checkpoint(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    tree.checkpoint()
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_rewind(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
) -> bool {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    tree.rewind()
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_root(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest, MERKLE_DEPTH>,
    root_ret: *mut [u8; 32],
) -> bool {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard note commitment tree pointer may not be null.")
    };

    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };

    if let Some(root) = tree.root().to_bytes() {
        root_ret.copy_from_slice(&root);
        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_empty_root(root_ret: *mut [u8; 32]) {
    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };

    let altitude = Altitude::from(MERKLE_DEPTH);

    let digest = OrchardIncrementalTreeDigest::empty_root(altitude)
        .to_bytes()
        .unwrap();

    root_ret.copy_from_slice(&digest);
}
