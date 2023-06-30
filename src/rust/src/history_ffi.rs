use std::convert::TryFrom;

use zcash_history::{Entry as MMREntry, Tree as MMRTree, Version, V1, V2};
use zcash_primitives::consensus::BranchId;

#[cxx::bridge]
mod ffi {
    struct Effect {
        // Root commitment after the called method is applied to the tree.
        root: [u8; 32],
        // Count associated with the result of the called method.
        count: usize,
    }

    #[namespace = "mmr"]
    extern "Rust" {
        fn append(
            // Consensus branch id
            cbranch: u32,
            // Length of tree in array representation
            t_len: u32,
            // Indices of provided tree nodes, length of p_len
            indices: &[u32],
            // Provided tree nodes data, length of p_len
            nodes: &[[u8; 253]], // zcash_history::MAX_ENTRY_SIZE
            // New node
            new_node_bytes: &[u8; 244], // zcash_history::MAX_NODE_DATA_SIZE
            // Return buffer for appended leaves, should be pre-allocated of ceiling(log2(t_len)) length
            buf_ret: &mut [[u8; 244]], // zcash_history::MAX_NODE_DATA_SIZE
        ) -> Result<Effect>;

        fn remove(
            // Consensus branch id
            cbranch: u32,
            // Length of tree in array representation
            t_len: u32,
            // Indices of provided tree nodes, length of p_len+e_len
            indices: &[u32],
            // Provided tree nodes data, length of p_len+e_len
            nodes: &[[u8; 253]], // zcash_history::MAX_ENTRY_SIZE
            // Peaks count
            p_len: usize,
        ) -> Result<Effect>;

        fn hash_node(cbranch: u32, node_bytes: &[u8; 244]) -> Result<[u8; 32]>;
    }
}

/// Switch the tree version on the epoch it is for.
fn dispatch<S, T>(cbranch: u32, input: S, v1: impl FnOnce(S) -> T, v2: impl FnOnce(S) -> T) -> T {
    match BranchId::try_from(cbranch).unwrap() {
        BranchId::Sprout
        | BranchId::Overwinter
        | BranchId::Sapling
        | BranchId::Heartwood
        | BranchId::Canopy => v1(input),
        _ => v2(input),
    }
}

fn construct_mmr_tree<V: Version>(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,

    // Indices of provided tree nodes, length of p_len+e_len
    indices: &[u32],
    // Provided tree nodes data, length of p_len+e_len
    nodes: &[[u8; zcash_history::MAX_ENTRY_SIZE]],

    // Peaks count
    p_len: usize,
) -> Result<MMRTree<V>, &'static str> {
    let mut peaks: Vec<_> = indices
        .iter()
        .zip(nodes.iter())
        .map(
            |(index, node)| match MMREntry::from_bytes(cbranch, &node[..]) {
                Ok(entry) => Ok((*index, entry)),
                Err(_) => Err("Invalid encoding"),
            },
        )
        .collect::<Result<_, _>>()?;
    let extra = peaks.split_off(p_len);

    Ok(MMRTree::new(t_len, peaks, extra))
}

/// Appends a leaf to the given history tree.
///
/// `t_len` must be at least 1.
///
/// Aborts if `cbranch` is not a valid consensus branch ID.
pub(crate) fn append(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len
    indices: &[u32],
    // Provided tree nodes data, length of p_len
    nodes: &[[u8; zcash_history::MAX_ENTRY_SIZE]],
    // New node
    new_node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE],
    // Return buffer for appended leaves, should be pre-allocated of ceiling(log2(t_len)) length
    buf_ret: &mut [[u8; zcash_history::MAX_NODE_DATA_SIZE]],
) -> Result<ffi::Effect, String> {
    dispatch(
        cbranch,
        buf_ret,
        |buf_ret| append_inner::<V1>(cbranch, t_len, indices, nodes, new_node_bytes, buf_ret),
        |buf_ret| append_inner::<V2>(cbranch, t_len, indices, nodes, new_node_bytes, buf_ret),
    )
}

#[allow(clippy::too_many_arguments)]
fn append_inner<V: Version>(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len
    indices: &[u32],
    // Provided tree nodes data, length of p_len
    nodes: &[[u8; zcash_history::MAX_ENTRY_SIZE]],
    // New node pointer
    new_node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE],
    // Return buffer for appended leaves, should be pre-allocated of ceiling(log2(t_len)) length
    buf_ret: &mut [[u8; zcash_history::MAX_NODE_DATA_SIZE]],
) -> Result<ffi::Effect, String> {
    let mut tree = construct_mmr_tree::<V>(cbranch, t_len, indices, nodes, indices.len())?;

    let node = V::from_bytes(cbranch, &new_node_bytes[..])
        .map_err(|e| format!("Invalid node encoding: {}", e))?;

    let appended = tree
        .append_leaf(node)
        .map_err(|e| format!("Failed to append leaf: {}", e))?;

    let return_count = appended.len();

    let root_node = tree
        .root_node()
        .expect("Just added, should resolve always; qed");

    let ret = ffi::Effect {
        root: V::hash(root_node.data()),
        count: return_count,
    };

    for (idx, next_buf) in buf_ret[..return_count].iter_mut().enumerate() {
        V::write(
            tree.resolve_link(appended[idx])
                .expect("This was generated by the tree and thus resolvable; qed")
                .data(),
            &mut &mut next_buf[..],
        )
        .expect("Write using cursor with enough buffer size cannot fail; qed");
    }

    Ok(ret)
}

/// Removes the most recently-appended leaf from the given history tree.
///
/// `t_len` must be at least 1.
///
/// Aborts if `cbranch` is not a valid consensus branch ID.
pub(crate) fn remove(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len+e_len
    indices: &[u32],
    // Provided tree nodes data, length of p_len+e_len
    nodes: &[[u8; zcash_history::MAX_ENTRY_SIZE]],
    // Peaks count
    p_len: usize,
) -> Result<ffi::Effect, String> {
    dispatch(
        cbranch,
        (),
        |()| remove_inner::<V1>(cbranch, t_len, indices, nodes, p_len),
        |()| remove_inner::<V2>(cbranch, t_len, indices, nodes, p_len),
    )
}

fn remove_inner<V: Version>(
    // Consensus branch id
    cbranch: u32,
    // Length of tree in array representation
    t_len: u32,
    // Indices of provided tree nodes, length of p_len+e_len
    indices: &[u32],
    // Provided tree nodes data, length of p_len+e_len
    nodes: &[[u8; zcash_history::MAX_ENTRY_SIZE]],
    // Peaks count
    p_len: usize,
) -> Result<ffi::Effect, String> {
    let mut tree = construct_mmr_tree::<V>(cbranch, t_len, indices, nodes, p_len)?;

    let truncate_len = tree
        .truncate_leaf()
        .map_err(|e| format!("Failed to truncate leaf: {}", e))?;

    Ok(ffi::Effect {
        root: V::hash(
            tree.root_node()
                .expect("Just generated without errors, root should be resolving")
                .data(),
        ),
        count: truncate_len as usize,
    })
}

/// Returns the hash of the given history tree node.
///
/// Aborts if `cbranch` is not a valid consensus branch ID.
fn hash_node(
    cbranch: u32,
    node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE],
) -> Result<[u8; 32], String> {
    dispatch(
        cbranch,
        (),
        |()| hash_node_inner::<V1>(cbranch, node_bytes),
        |()| hash_node_inner::<V2>(cbranch, node_bytes),
    )
}

fn hash_node_inner<V: Version>(
    cbranch: u32,
    node_bytes: &[u8; zcash_history::MAX_NODE_DATA_SIZE],
) -> Result<[u8; 32], String> {
    let node = V::from_bytes(cbranch, &node_bytes[..])
        .map_err(|e| format!("Invalid node encoding: {}", e))?;

    Ok(V::hash(&node))
}
