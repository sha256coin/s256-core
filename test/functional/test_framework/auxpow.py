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


def create_auxpow(aux_hash_hex, bits_hex):
    """Build a minimal auxpow for a single aux chain: a parent coinbase whose
    scriptSig carries the merge-mining tag, as the only transaction of a
    parent header ground to meet the aux block's target."""
    tag = (MERGE_MINING_HEADER + bytes.fromhex(aux_hash_hex)[::-1]
           + (1).to_bytes(4, "little")    # merkle tree size: one aux chain
           + (0).to_bytes(4, "little"))   # merkle nonce
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(COutPoint(0, 0xffffffff), CScript([tag]))]
    coinbase.vout = [CTxOut(0, CScript([OP_TRUE]))]

    parent = CBlockHeader()
    parent.hashMerkleRoot = coinbase.txid_int
    parent.nBits = int(bits_hex, 16)
    target = target_from_bits(parent.nBits)
    while parent.hash_int > target:
        parent.nNonce += 1

    return (coinbase.serialize()
            + ser_compact_size(0) + (0).to_bytes(4, "little")   # coinbase merkle branch, index
            + ser_compact_size(0) + (0).to_bytes(4, "little")   # chain merkle branch, index
            + parent.serialize()).hex()


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
