use byteorder::{ReadBytesExt, WriteBytesExt};
use std::io::{self, Read, Write};

use incrementalmerkletree::{
    bridgetree::{BridgeTree, Checkpoint},
    Hashable,
};
use zcash_encoding::Vector;
use zcash_primitives::merkle_tree::{
    incremental::{
        read_bridge_v1, read_leu64_usize, read_position, write_bridge_v1, write_position,
        write_usize_leu64, SER_V1,
    },
    HashSer,
};

pub fn write_checkpoint_v1<H: HashSer + Ord, W: Write>(
    mut writer: W,
    checkpoint: &Checkpoint<H>,
) -> io::Result<()> {
    write_usize_leu64(&mut writer, checkpoint.bridges_len())?;
    writer.write_u8(if checkpoint.is_witnessed() { 1 } else { 0 })?;
    Vector::write_sized(
        &mut writer,
        checkpoint.forgotten().iter(),
        |mut w, ((pos, leaf_value), idx)| {
            write_position(&mut w, *pos)?;
            leaf_value.write(&mut w)?;
            write_usize_leu64(&mut w, *idx)
        },
    )?;

    Ok(())
}

pub fn read_checkpoint_v1<H: HashSer + Ord, R: Read>(mut reader: R) -> io::Result<Checkpoint<H>> {
    Ok(Checkpoint::from_parts(
        read_leu64_usize(&mut reader)?,
        reader.read_u8()? == 1,
        Vector::read_collected(&mut reader, |mut r| {
            Ok((
                (read_position(&mut r)?, H::read(&mut r)?),
                read_leu64_usize(&mut r)?,
            ))
        })?,
    ))
}

pub fn write_tree_v1<H: Hashable + HashSer + Ord, W: Write>(
    mut writer: W,
    tree: &BridgeTree<H, 32>,
) -> io::Result<()> {
    Vector::write(&mut writer, tree.bridges(), |w, b| write_bridge_v1(w, b))?;
    Vector::write_sized(
        &mut writer,
        tree.witnessed_indices().iter(),
        |mut w, ((pos, a), i)| {
            write_position(&mut w, *pos)?;
            a.write(&mut w)?;
            write_usize_leu64(&mut w, *i)
        },
    )?;
    Vector::write(&mut writer, tree.checkpoints(), |w, c| {
        write_checkpoint_v1(w, c)
    })?;
    write_usize_leu64(&mut writer, tree.max_checkpoints())?;

    Ok(())
}

#[allow(clippy::redundant_closure)]
pub fn read_tree_v1<H: Hashable + HashSer + Ord + Clone, R: Read>(
    mut reader: R,
) -> io::Result<BridgeTree<H, 32>> {
    BridgeTree::from_parts(
        Vector::read(&mut reader, |r| read_bridge_v1(r))?,
        Vector::read_collected(&mut reader, |mut r| {
            Ok((
                (read_position(&mut r)?, H::read(&mut r)?),
                read_leu64_usize(&mut r)?,
            ))
        })?,
        Vector::read(&mut reader, |r| read_checkpoint_v1(r))?,
        read_leu64_usize(&mut reader)?,
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

pub fn write_tree<H: Hashable + HashSer + Ord, W: Write>(
    mut writer: W,
    tree: &BridgeTree<H, 32>,
) -> io::Result<()> {
    writer.write_u8(SER_V1)?;
    write_tree_v1(&mut writer, tree)
}

pub fn read_tree<H: Hashable + HashSer + Ord + Clone, R: Read>(
    mut reader: R,
) -> io::Result<BridgeTree<H, 32>> {
    match reader.read_u8()? {
        SER_V1 => read_tree_v1(&mut reader),
        flag => Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            format!("Unrecognized tree serialization version: {:?}", flag),
        )),
    }
}
