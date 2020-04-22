from pyblake2 import blake2b
import struct
from typing import (List, Optional)

from .mininode import (CBlockHeader, block_work_from_compact, ser_compactsize, ser_uint256)

def H(msg: bytes, consensusBranchId: int) -> bytes:
    digest = blake2b(
        digest_size=32,
        person=b'ZcashHistory' + struct.pack("<I", consensusBranchId))
    digest.update(msg)
    return digest.digest()

class ZcashMMRNode():
    # leaf nodes have no children
    left_child: Optional['ZcashMMRNode']
    right_child: Optional['ZcashMMRNode']

    # commitments
    hashSubtreeCommitment: bytes
    nEarliestTimestamp: int
    nLatestTimestamp: int
    nEarliestTargetBits: int
    nLatestTargetBits: int
    hashEarliestSaplingRoot: bytes # left child's sapling root
    hashLatestSaplingRoot: bytes # right child's sapling root
    nSubTreeTotalWork: int # total difficulty accumulated within each subtree
    nEarliestHeight: int
    nLatestHeight: int
    nSaplingTxCount: int # number of Sapling transactions in block

    consensusBranchId: bytes

    @classmethod
    def from_block(Z, block: CBlockHeader, height, sapling_root, sapling_tx_count, consensusBranchId) -> 'ZcashMMRNode':
        '''Create a leaf node from a block'''
        node = Z()
        node.left_child = None
        node.right_child = None
        node.hashSubtreeCommitment = ser_uint256(block.rehash())
        node.nEarliestTimestamp = block.nTime
        node.nLatestTimestamp = block.nTime
        node.nEarliestTargetBits = block.nBits
        node.nLatestTargetBits = block.nBits
        node.hashEarliestSaplingRoot = sapling_root
        node.hashLatestSaplingRoot = sapling_root
        node.nSubTreeTotalWork = block_work_from_compact(block.nBits)
        node.nEarliestHeight = height
        node.nLatestHeight = height
        node.nSaplingTxCount = sapling_tx_count
        node.consensusBranchId = consensusBranchId
        return node

    def serialize(self) -> bytes:
        '''serializes a node'''
        buf = b''
        buf += self.hashSubtreeCommitment
        buf += struct.pack("<I", self.nEarliestTimestamp)
        buf += struct.pack("<I", self.nLatestTimestamp)
        buf += struct.pack("<I", self.nEarliestTargetBits)
        buf += struct.pack("<I", self.nLatestTargetBits)
        buf += self.hashEarliestSaplingRoot
        buf += self.hashLatestSaplingRoot
        buf += ser_uint256(self.nSubTreeTotalWork)
        buf += ser_compactsize(self.nEarliestHeight)
        buf += ser_compactsize(self.nLatestHeight)
        buf += ser_compactsize(self.nSaplingTxCount)
        return buf

def make_parent(
        left_child: ZcashMMRNode,
        right_child: ZcashMMRNode) -> ZcashMMRNode:
    parent = ZcashMMRNode()
    parent.left_child = left_child
    parent.right_child = right_child
    parent.hashSubtreeCommitment = H(
        left_child.serialize() + right_child.serialize(),
        left_child.consensusBranchId,
    )
    parent.nEarliestTimestamp = left_child.nEarliestTimestamp
    parent.nLatestTimestamp = right_child.nLatestTimestamp
    parent.nEarliestTargetBits = left_child.nEarliestTargetBits
    parent.nLatestTargetBits = right_child.nLatestTargetBits
    parent.hashEarliestSaplingRoot = left_child.hashEarliestSaplingRoot
    parent.hashLatestSaplingRoot = right_child.hashLatestSaplingRoot
    parent.nSubTreeTotalWork = left_child.nSubTreeTotalWork + right_child.nSubTreeTotalWork
    parent.nEarliestHeight = left_child.nEarliestHeight
    parent.nLatestHeight = right_child.nLatestHeight
    parent.nSaplingTxCount = left_child.nSaplingTxCount + right_child.nSaplingTxCount
    parent.consensusBranchId = left_child.consensusBranchId
    return parent

def make_root_commitment(root: ZcashMMRNode) -> bytes:
    '''Makes the root commitment for a blockheader'''
    return H(root.serialize(), root.consensusBranchId)

def get_peaks(node: ZcashMMRNode) -> List[ZcashMMRNode]:
    peaks: List[ZcashMMRNode] = []

    # Get number of leaves.
    leaves = node.nLatestHeight - (node.nEarliestHeight - 1)
    assert(leaves > 0)

    # Check if the number of leaves in this subtree is a power of two.
    if (leaves & (leaves - 1)) == 0:
        # This subtree is full, and therefore a single peak. This also covers
        # the case of a single isolated leaf.
        peaks.append(node)
    else:
        # This is one of the generated nodes; search within its children.
        peaks.extend(get_peaks(node.left_child))
        peaks.extend(get_peaks(node.right_child))

    return peaks


def bag_peaks(peaks: List[ZcashMMRNode]) -> ZcashMMRNode:
    '''
    "Bag" a list of peaks, and return the final root
    '''
    root = peaks[0]
    for i in range(1, len(peaks)):
        root = make_parent(root, peaks[i])
    return root


def append(root: ZcashMMRNode, leaf: ZcashMMRNode) -> ZcashMMRNode:
    '''Append a leaf to an existing tree, return the new tree root'''
    # recursively find a list of peaks in the current tree
    peaks: List[ZcashMMRNode] = get_peaks(root)
    merged: List[ZcashMMRNode] = []

    # Merge peaks from right to left.
    # This will produce a list of peaks in reverse order
    current = leaf
    for peak in peaks[::-1]:
        current_leaves = current.nLatestHeight - (current.nEarliestHeight - 1)
        peak_leaves = peak.nLatestHeight - (peak.nEarliestHeight - 1)

        if current_leaves == peak_leaves:
            current = make_parent(peak, current)
        else:
            merged.append(current)
            current = peak
    merged.append(current)

    # finally, bag the merged peaks
    return bag_peaks(merged[::-1])

def delete(root: ZcashMMRNode) -> ZcashMMRNode:
    '''
    Delete the rightmost leaf node from an existing MMR
    Return the new tree root
    '''

    n_leaves = root.nLatestHeight - (root.nEarliestHeight - 1)
    # if there were an odd number of leaves,
    # simply replace root with left_child
    if n_leaves & 1:
        return root.left_child

    # otherwise, we need to re-bag the peaks.
    else:
        # first peak
        peaks = [root.left_child]

        # we do this traversing the right (unbalanced) side of the tree
        # we keep the left side (balanced subtree or leaf) of each subtree
        # until we reach a leaf
        subtree_root = root.right_child
        while subtree_root.left_child:
            peaks.append(subtree_root.left_child)
            subtree_root = subtree_root.right_child

    new_root = bag_peaks(peaks)
    return new_root
