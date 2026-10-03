#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Helpers for merge-mining (AuxPoW) S256 blocks in functional tests."""

import time

from .messages import (
    COutPoint,
    CBlockHeader,
    CTransaction,
    CTxIn,
    CTxOut,
    hash256,
    ser_compact_size,
)
from .script import (
    CScript,
    OP_TRUE,
)

MERGE_MINING_HEADER = bytes.fromhex("fabe6d6d")
VERSION_AUXPOW_BIT = 1 << 8


def target_from_bits(bits):
    return (bits & 0xffffff) << (8 * ((bits >> 24) - 3))


def header_hash(header_hex):
    """Block hash (hex) of a serialized header, auxpow or not."""
    return hash256(bytes.fromhex(header_hex)[:80])[::-1].hex()


def expected_index(nonce, chain_id, height):
    """Slot an aux chain must occupy in a merge-mining tree of 2**height
    leaves (CAuxPow::GetExpectedIndex)."""
    rand = (nonce * 1103515245 + 12345) & 0xffffffff
    rand = (rand + chain_id) & 0xffffffff
    rand = (rand * 1103515245 + 12345) & 0xffffffff
    return rand % (1 << height)


def merkle_leaf(block_hash_hex):
    """A block hash (RPC hex) as a merkle leaf / node, in internal byte order."""
    return bytes.fromhex(block_hash_hex)[::-1]


def merge_mining_tag(chain_root, size=1, nonce=0):
    """The merge-mining tag committing to chain_root (internal byte order) in
    a tree of `size` leaves. The tag carries the root reversed, i.e. in the
    order of its hex form."""
    return (MERGE_MINING_HEADER + chain_root[::-1]
            + size.to_bytes(4, "little") + nonce.to_bytes(4, "little"))


def parent_tx(script_sig, prevout=None):
    """A parent-chain coinbase with the given scriptSig, or an ordinary
    transaction if prevout is given."""
    tx = CTransaction()
    tx.vin = [CTxIn(prevout or COutPoint(0, 0xffffffff), script_sig)]
    tx.vout = [CTxOut(0, CScript([OP_TRUE]))]
    return tx


def serialize_auxpow(tagged_tx, merkle_branch, index, chain_branch, chain_index, merkle_root, bits_hex):
    """Serialize an auxpow, grinding a parent header with the given
    transaction merkle root to meet bits_hex. Branches are lists of 32-byte
    internal-order hashes."""
    parent = CBlockHeader()
    parent.hashMerkleRoot = int.from_bytes(merkle_root, "little")
    parent.nBits = int(bits_hex, 16)
    target = target_from_bits(parent.nBits)
    while parent.hash_int > target:
        parent.nNonce += 1

    return (tagged_tx.serialize()
            + ser_compact_size(len(merkle_branch)) + b"".join(merkle_branch) + index.to_bytes(4, "little", signed=True)
            + ser_compact_size(len(chain_branch)) + b"".join(chain_branch) + chain_index.to_bytes(4, "little", signed=True)
            + parent.serialize()).hex()


def create_auxpow(aux_hash_hex, bits_hex):
    """Build a minimal auxpow for a single aux chain: a parent coinbase whose
    scriptSig carries the merge-mining tag, as the only transaction of a
    parent header ground to meet the aux block's target."""
    coinbase = parent_tx(CScript([merge_mining_tag(merkle_leaf(aux_hash_hex))]))
    return serialize_auxpow(coinbase, [], 0, [], 0, coinbase.txid_int.to_bytes(32, "little"), bits_hex)


def merge_mine(node, address, *, own_hash_above_target=False):
    """Merge-mine one block on top of node's tip via createauxblock and
    submitauxblock, returning its hash.

    With own_hash_above_target, retry with a later mocked time until the aux
    block's own hash does not meet its target, so that only the auxpow can
    satisfy the proof of work (the normal case on a real network)."""
    mocktime = int(time.time())
    while True:
        aux = node.createauxblock(address)
        if not own_hash_above_target or int(aux["hash"], 16) > target_from_bits(int(aux["bits"], 16)):
            break
        mocktime += 1
        node.setmocktime(mocktime)
    assert node.submitauxblock(aux["hash"], create_auxpow(aux["hash"], aux["bits"]))
    node.setmocktime(0)
    assert node.getbestblockhash() == aux["hash"]
    return aux["hash"]
