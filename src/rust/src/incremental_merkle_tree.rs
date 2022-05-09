use byteorder::{ReadBytesExt, WriteBytesExt};
use std::collections::{BTreeMap, BTreeSet};
use std::io::{self, Read, Write};

use incrementalmerkletree::{
    bridgetree::{BridgeTree, Checkpoint},
    Hashable, Position,
};
use zcash_encoding::{Optional, Vector};
use zcash_primitives::merkle_tree::{
    incremental::{
        read_bridge_v1, read_leu64_usize, read_position, write_bridge_v1, write_position,
        write_usize_leu64, SER_V1, SER_V2,
    },
    HashSer,
};

pub fn write_checkpoint_v2<W: Write>(mut writer: W, checkpoint: &Checkpoint) -> io::Result<()> {
    write_usize_leu64(&mut writer, checkpoint.bridges_len())?;
    writer.write_u8(if checkpoint.is_witnessed() { 1 } else { 0 })?;
    Vector::write_sized(&mut writer, checkpoint.witnessed().iter(), |w, p| {
        write_position(w, *p)
    })?;
    Vector::write_sized(
        &mut writer,
        checkpoint.forgotten().iter(),
        |mut w, (pos, idx)| {
            write_position(&mut w, *pos)?;
            write_usize_leu64(&mut w, *idx)
        },
    )?;

    Ok(())
}

pub fn read_checkpoint_v1<R: Read>(
    mut reader: R,
    save_positions: &mut BTreeMap<usize, Position>,
) -> io::Result<Checkpoint> {
    let bridges_len = read_leu64_usize(&mut reader)?;
    let is_witnessed = reader.read_u8()? == 1;
    let forgotten = Vector::read_collected(&mut reader, |mut r| {
        Ok((read_position(&mut r)?, read_leu64_usize(&mut r)?))
    })?;

    // v1 checkpoints lacked information about the witnesses that were made during their
    // span of blocks, so we assume that a witness was made in the first checkpoint
    // with a corresponding bridge index.
    let witnessed = if let Some(position) = save_positions.remove(&bridges_len) {
        BTreeSet::from([position])
    } else {
        BTreeSet::new()
    };

    Ok(Checkpoint::from_parts(
        bridges_len,
        is_witnessed,
        witnessed,
        forgotten,
    ))
}

pub fn read_checkpoint_v2<R: Read>(mut reader: R) -> io::Result<Checkpoint> {
    Ok(Checkpoint::from_parts(
        read_leu64_usize(&mut reader)?,
        reader.read_u8()? == 1,
        Vector::read_collected(&mut reader, |r| read_position(r))?,
        Vector::read_collected(&mut reader, |mut r| {
            Ok((read_position(&mut r)?, read_leu64_usize(&mut r)?))
        })?,
    ))
}

pub fn write_tree<H: Hashable + HashSer + Ord, W: Write>(
    mut writer: W,
    tree: &BridgeTree<H, 32>,
) -> io::Result<()> {
    writer.write_u8(SER_V2)?;
    Vector::write(&mut writer, tree.prior_bridges(), |mut w, b| {
        write_bridge_v1(&mut w, b)
    })?;
    Optional::write(&mut writer, tree.current_bridge().as_ref(), |mut w, b| {
        write_bridge_v1(&mut w, b)
    })?;
    Vector::write_sized(
        &mut writer,
        tree.witnessed_indices().iter(),
        |mut w, (pos, i)| {
            write_position(&mut w, *pos)?;
            write_usize_leu64(&mut w, *i)
        },
    )?;
    Vector::write(&mut writer, tree.checkpoints(), |w, c| {
        write_checkpoint_v2(w, c)
    })?;
    write_usize_leu64(&mut writer, tree.max_checkpoints())?;

    Ok(())
}

#[allow(clippy::redundant_closure)]
pub fn read_tree<H: Hashable + HashSer + Ord + Clone, R: Read>(
    mut reader: R,
) -> io::Result<BridgeTree<H, 32>> {
    let ser_version = reader.read_u8()?;
    let prior_bridges = Vector::read(&mut reader, |r| read_bridge_v1(r))?;
    let current_bridge = Optional::read(&mut reader, |r| read_bridge_v1(r))?;
    let saved: BTreeMap<Position, usize> = Vector::read_collected(&mut reader, |mut r| {
        Ok((read_position(&mut r)?, read_leu64_usize(&mut r)?))
    })?;

    let checkpoints = match ser_version {
        SER_V1 => {
            let mut save_positions = saved.iter().map(|(p, i)| (*i, *p)).collect();
            Vector::read_collected_mut(&mut reader, |r| read_checkpoint_v1(r, &mut save_positions))
        }
        SER_V2 => Vector::read_collected(&mut reader, |r| read_checkpoint_v2(r)),
        flag => Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            format!("Unrecognized tree serialization version: {:?}", flag),
        )),
    }?;
    let max_checkpoints = read_leu64_usize(&mut reader)?;

    BridgeTree::from_parts(
        prior_bridges,
        current_bridge,
        saved,
        checkpoints,
        max_checkpoints,
    )
    .map_err(|err| {
        io::Error::new(
            io::ErrorKind::InvalidInput,
            format!(
                "Consistency violation found when attempting to deserialize Merkle tree: {:?}",
                err
            ),
        )
    })
}
