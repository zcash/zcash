use zcash_history::{Entry, EntryLink, NodeData, V1};

use crate::history_ffi::{librustzcash_mmr_append, librustzcash_mmr_delete};

const NODE_DATA_16L: &[u8] = include_bytes!("./res/tree16.dat");
const NODE_DATA_1023L: &[u8] = include_bytes!("./res/tree1023.dat");

struct TreeView {
    peaks: Vec<(u32, Entry<V1>)>,
    extra: Vec<(u32, Entry<V1>)>,
}

fn draft(into: &mut Vec<(u32, Entry<V1>)>, nodes: &[NodeData], peak_pos: usize, h: u32) {
    let node_data = nodes[peak_pos - 1].clone();
    let peak = match h {
        0 => Entry::new_leaf(node_data),
        _ => Entry::new(
            node_data,
            EntryLink::Stored((peak_pos - (1 << h) - 1) as u32),
            EntryLink::Stored((peak_pos - 2) as u32),
        ),
    };

    into.push(((peak_pos - 1) as u32, peak));
}

fn prepare_tree(nodes: &[NodeData]) -> TreeView {
    assert!(!nodes.is_empty());

    // integer log2 of (nodes.len()+1), -1
    let mut h = (32 - ((nodes.len() + 1) as u32).leading_zeros() - 1) - 1;
    let mut peak_pos = (1 << (h + 1)) - 1;
    let mut peaks = Vec::new();

    // used later
    let mut last_peak_pos = 0;
    let mut last_peak_h = 0;

    loop {
        if peak_pos > nodes.len() {
            // left child, -2^h
            peak_pos -= 1 << h;
            h -= 1;
        }

        if peak_pos <= nodes.len() {
            draft(&mut peaks, nodes, peak_pos, h);

            // save to be used in next loop
            last_peak_pos = peak_pos;
            last_peak_h = h;

            // right sibling
            peak_pos += (1 << (h + 1)) - 1;
        }

        if h == 0 {
            break;
        }
    }

    // for deletion, everything on the right slope of the last peak should be pre-loaded
    let mut extra = Vec::new();
    let mut h = last_peak_h;
    let mut peak_pos = last_peak_pos;

    while h > 0 {
        let left_pos = peak_pos - (1 << h);
        let right_pos = peak_pos - 1;
        h -= 1;

        // drafting left child
        draft(&mut extra, nodes, left_pos, h);

        // drafting right child
        draft(&mut extra, nodes, right_pos, h);

        // continuing on right slope
        peak_pos = right_pos;
    }

    TreeView { peaks, extra }
}

fn preload_tree_append(nodes: &[NodeData]) -> (Vec<u32>, Vec<[u8; zcash_history::MAX_ENTRY_SIZE]>) {
    assert!(!nodes.is_empty());

    let tree_view = prepare_tree(nodes);

    let mut indices = Vec::new();
    let mut bytes = Vec::new();

    for (idx, entry) in tree_view.peaks.into_iter() {
        let mut buf = [0u8; zcash_history::MAX_ENTRY_SIZE];
        entry
            .write(&mut &mut buf[..])
            .expect("Cannot fail if enough buffer length");
        indices.push(idx);
        bytes.push(buf);
    }

    (indices, bytes)
}

// also returns number of peaks
fn preload_tree_delete(
    nodes: &[NodeData],
) -> (Vec<u32>, Vec<[u8; zcash_history::MAX_ENTRY_SIZE]>, usize) {
    assert!(!nodes.is_empty());

    let tree_view = prepare_tree(nodes);

    let mut indices = Vec::new();
    let mut bytes = Vec::new();

    let peak_count = tree_view.peaks.len();

    for (idx, entry) in tree_view
        .peaks
        .into_iter()
        .chain(tree_view.extra.into_iter())
    {
        let mut buf = [0u8; zcash_history::MAX_ENTRY_SIZE];
        entry
            .write(&mut &mut buf[..])
            .expect("Cannot fail if enough buffer length");
        indices.push(idx);
        bytes.push(buf);
    }

    (indices, bytes, peak_count)
}

fn load_nodes(bytes: &'static [u8]) -> Vec<NodeData> {
    let mut res = Vec::new();
    let mut cursor = std::io::Cursor::new(bytes);
    while (cursor.position() as usize) < bytes.len() {
        let node_data = zcash_history::NodeData::read(0, &mut cursor)
            .expect("Statically checked to be correct");
        res.push(node_data);
    }

    res
}

#[test]
fn append() {
    let nodes = load_nodes(NODE_DATA_16L);
    let (indices, peaks) = preload_tree_append(&nodes);

    let mut rt_ret = [0u8; 32];

    let mut buf_ret = Vec::<[u8; zcash_history::MAX_NODE_DATA_SIZE]>::with_capacity(32);

    let mut new_node_data = [0u8; zcash_history::MAX_NODE_DATA_SIZE];
    let new_node = NodeData {
        consensus_branch_id: 0,
        subtree_commitment: [0u8; 32],
        start_time: 101,
        end_time: 110,
        start_target: 190,
        end_target: 200,
        start_sapling_root: [0u8; 32],
        end_sapling_root: [0u8; 32],
        subtree_total_work: Default::default(),
        start_height: 10,
        end_height: 10,
        sapling_tx: 13,
    };
    new_node
        .write(&mut &mut new_node_data[..])
        .expect("Failed to write node data");

    let result = librustzcash_mmr_append(
        0,
        nodes.len() as u32,
        indices.as_ptr(),
        peaks.as_ptr(),
        peaks.len(),
        &new_node_data,
        &mut rt_ret,
        buf_ret.as_mut_ptr(),
    );

    unsafe {
        buf_ret.set_len(result as usize);
    }

    assert_eq!(result, 2);

    let new_node_1 =
        NodeData::from_bytes(0, &buf_ret[0][..]).expect("Failed to reconstruct return node #1");

    let new_node_2 =
        NodeData::from_bytes(0, &buf_ret[1][..]).expect("Failed to reconstruct return node #2");

    assert_eq!(new_node_1.start_height, 10);
    assert_eq!(new_node_1.end_height, 10);

    // this is combined new node (which is `new_node_1`) + the one which was there before (for block #9)
    assert_eq!(new_node_2.start_height, 9);
    assert_eq!(new_node_2.end_height, 10);
    assert_eq!(new_node_2.sapling_tx, 27);
}

#[test]
fn delete() {
    let nodes = load_nodes(NODE_DATA_1023L);
    let (indices, nodes, peak_count) = preload_tree_delete(&nodes);

    let mut rt_ret = [0u8; 32];

    let result = librustzcash_mmr_delete(
        0,
        nodes.len() as u32,
        indices.as_ptr(),
        nodes.as_ptr(),
        peak_count,
        indices.len() - peak_count,
        &mut rt_ret,
    );

    // Deleting from full tree of 9 height would result in cascade deleting of 10 nodes
    assert_eq!(result, 10);
}
