use bincode;
use incrementalmerkletree::{
    bridgetree::{self, BridgeTree},
    Altitude, Frontier, Hashable, Tree,
};
use std::ptr;

use orchard::{bundle::Authorized, tree::OrchardIncrementalTreeDigest};

use zcash_primitives::transaction::components::Amount;

use crate::orchard_ffi::{error, CppStreamReader, CppStreamWriter, ReadCb, StreamObj, WriteCb};

//
// Operations on Merkle frontiers.
//

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_empty(
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest> {
    let empty_tree = bridgetree::Frontier::new(32);
    Box::into_raw(Box::new(empty_tree))
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_clone(
    tree: *const bridgetree::Frontier<OrchardIncrementalTreeDigest>,
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest> {
    unsafe { tree.as_ref() }
        .map(|tree| Box::into_raw(Box::new(tree.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_free(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest>,
) {
    if !tree.is_null() {
        drop(unsafe { Box::from_raw(tree) });
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut bridgetree::Frontier<OrchardIncrementalTreeDigest> {
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
    tree: *const bridgetree::Frontier<OrchardIncrementalTreeDigest>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard tree pointer may not be null.")
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
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest>,
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

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_root(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest>,
    root_ret: *mut [u8; 32],
) -> bool {
    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };
    if let Some(tree) = unsafe { tree.as_ref() } {
        if let Some(root) = tree.root().to_bytes() {
            root_ret.copy_from_slice(&root);
            true
        } else {
            false
        }
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn orchard_merkle_frontier_num_leaves(
    tree: *mut bridgetree::Frontier<OrchardIncrementalTreeDigest>,
) -> usize {
    unsafe { tree.as_ref() }
        .and_then(|t| t.position())
        .map_or(0, |p| <usize>::from(p) + 1)
}

//
// Operations on incremental merkle trees with interstitial
// witnesses.
//

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_empty(
) -> *mut BridgeTree<OrchardIncrementalTreeDigest> {
    let empty_tree = BridgeTree::new(32, 100);
    Box::into_raw(Box::new(empty_tree))
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_clone(
    tree: *const BridgeTree<OrchardIncrementalTreeDigest>,
) -> *mut BridgeTree<OrchardIncrementalTreeDigest> {
    unsafe { tree.as_ref() }
        .map(|tree| Box::into_raw(Box::new(tree.clone())))
        .unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_free(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest>,
) {
    if !tree.is_null() {
        drop(unsafe { Box::from_raw(tree) });
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_parse(
    stream: Option<StreamObj>,
    read_cb: Option<ReadCb>,
) -> *mut BridgeTree<OrchardIncrementalTreeDigest> {
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
    tree: *const BridgeTree<OrchardIncrementalTreeDigest>,
    stream: Option<StreamObj>,
    write_cb: Option<WriteCb>,
) -> bool {
    let tree = unsafe {
        tree.as_ref()
            .expect("Orchard tree pointer may not be null.")
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
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest>,
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

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_checkpoint(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest>,
) {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard tree pointer may not be null.")
    };

    tree.checkpoint()
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_rewind(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest>,
) -> bool {
    let tree = unsafe {
        tree.as_mut()
            .expect("Orchard tree pointer may not be null.")
    };

    tree.rewind()
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_root(
    tree: *mut BridgeTree<OrchardIncrementalTreeDigest>,
    root_ret: *mut [u8; 32],
) -> bool {
    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };
    if let Some(tree) = unsafe { tree.as_ref() } {
        if let Some(root) = tree.root().to_bytes() {
            root_ret.copy_from_slice(&root);
            true
        } else {
            false
        }
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn incremental_sinsemilla_tree_empty_root(depth: usize, root_ret: *mut [u8; 32]) {
    let root_ret = unsafe {
        root_ret
            .as_mut()
            .expect("Cannot return to the null pointer.")
    };

    let altitude = Altitude::from(depth as u32);

    let digest = OrchardIncrementalTreeDigest::empty_root(altitude)
        .to_bytes()
        .unwrap();

    root_ret.copy_from_slice(&digest);
}
