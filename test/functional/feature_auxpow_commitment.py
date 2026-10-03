#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the merge-mining commitment rules through submitauxblock.

- the tagged transaction must be the parent's coinbase (merkle index 0)
- one parent commits to at most one S256 block: the chain merkle index must
  be the slot expected for S256's chain ID, and the tag may appear only once
- the tag carries the createauxblock hash bytes in hex order (Namecoin's)
- an accepted aux block relays to a peer, a rejected one does not
- getblock / getblockheader show the auxpow
"""

from test_framework.auxpow import (
    MERGE_MINING_HEADER,
    expected_index,
    merge_mining_tag,
    merkle_leaf,
    parent_tx,
    serialize_auxpow,
)
from test_framework.messages import (
    COutPoint,
    hash256,
)
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

S256_CHAIN_ID = 0x53323536


def txid(tx):
    return hash256(tx.serialize_without_witness())


class AuxpowCommitmentTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def assert_rejected(self, aux, auxpow, reason):
        node = self.nodes[0]
        tip = node.getbestblockhash()
        with node.assert_debug_log([f"AcceptBlock FAILED ({reason},"]):
            assert_equal(node.submitauxblock(aux["hash"], auxpow), False)
        assert_equal(node.getbestblockhash(), tip)

    def assert_accepted(self, aux, auxpow):
        assert_equal(self.nodes[0].submitauxblock(aux["hash"], auxpow), True)
        assert_equal(self.nodes[0].getbestblockhash(), aux["hash"])
        self.sync_blocks()
        self.check_auxpow_json(aux["hash"], auxpow)

    def check_auxpow_json(self, block_hash, auxpow_hex):
        """The auxpow object of getblock and getblockheader on both nodes
        matches the serialized auxpow."""
        raw = bytes.fromhex(auxpow_hex)
        parent = raw[-80:]
        for node in self.nodes:
            for result in (node.getblock(block_hash), node.getblock(block_hash, 2), node.getblockheader(block_hash)):
                a = result["auxpow"]
                assert raw.startswith(bytes.fromhex(a["tx"]["hex"]))
                coinbase_len = len(bytes.fromhex(a["tx"]["hex"]))
                rest = raw[coinbase_len:-80]
                n = rest[0]
                assert_equal(a["merklebranch"], [rest[1 + 32 * i:33 + 32 * i][::-1].hex() for i in range(n)])
                rest = rest[1 + 32 * n + 4:]
                m = rest[0]
                assert_equal(a["chainmerklebranch"], [rest[1 + 32 * i:33 + 32 * i][::-1].hex() for i in range(m)])
                assert_equal(a["chainindex"], int.from_bytes(rest[1 + 32 * m:5 + 32 * m], "little", signed=True))
                p = a["parentblock"]
                assert_equal(p["hash"], hash256(parent)[::-1].hex())
                assert_equal(p["merkleroot"], parent[36:68][::-1].hex())
                assert_equal(p["previousblockhash"], parent[4:36][::-1].hex())
                assert_equal(p["version"], int.from_bytes(parent[:4], "little", signed=True))
                assert_equal(p["versionHex"], parent[:4][::-1].hex())
                assert_equal(p["time"], int.from_bytes(parent[68:72], "little"))
                assert_equal(p["bits"], parent[72:76][::-1].hex())
                assert_equal(p["nonce"], int.from_bytes(parent[76:80], "little"))

    def run_test(self):
        node = self.nodes[0]
        addr1 = node.get_deterministic_priv_key().address
        addr2 = self.nodes[1].get_deterministic_priv_key().address
        self.generatetoaddress(node, 20, addr1)
        tip = node.getbestblockhash()
        assert "auxpow" not in node.getblock(tip) and "auxpow" not in node.getblockheader(tip)

        self.log.info("Tag byte order: the createauxblock hash hex, decoded as is")
        aux = node.createauxblock(addr1)
        old_order = MERGE_MINING_HEADER + bytes.fromhex(aux["hash"])[::-1] + (1).to_bytes(4, "little") + (0).to_bytes(4, "little")
        cb = parent_tx(CScript([old_order]))
        self.assert_rejected(aux, serialize_auxpow(cb, [], 0, [], 0, txid(cb), aux["bits"]), "auxpow-chain-merkle-mismatch")
        aux = node.createauxblock(addr1)
        pool_tag = MERGE_MINING_HEADER + bytes.fromhex(aux["hash"]) + (1).to_bytes(4, "little") + (0).to_bytes(4, "little")
        cb = parent_tx(CScript([pool_tag]))
        self.assert_accepted(aux, serialize_auxpow(cb, [], 0, [], 0, txid(cb), aux["bits"]))

        self.log.info("Reject a tag in an ordinary parent transaction (not the coinbase)")
        aux = node.createauxblock(addr1)
        coinbase = parent_tx(CScript([b"\x01\x00"]))
        tagged = parent_tx(CScript([merge_mining_tag(merkle_leaf(aux["hash"]))]), COutPoint(1, 3))
        root = hash256(txid(coinbase) + txid(tagged))
        self.assert_rejected(aux, serialize_auxpow(tagged, [txid(coinbase)], 1, [], 0, root, aux["bits"]),
                             "auxpow-coinbase-not-first")
        aux = node.createauxblock(addr1)
        tagged = parent_tx(CScript([merge_mining_tag(merkle_leaf(aux["hash"]))]), COutPoint(1, 3))
        root = hash256(txid(coinbase) + txid(tagged))
        self.assert_rejected(aux, serialize_auxpow(tagged, [txid(coinbase)], 0, [], 0, root, aux["bits"]),
                             "auxpow-coinbase-merkle-mismatch")
        assert_equal(node.getblockcount(), 21)

        self.log.info("Accept the tag in the coinbase of a multi-transaction parent")
        aux = node.createauxblock(addr1)
        coinbase = parent_tx(CScript([merge_mining_tag(merkle_leaf(aux["hash"]))]))
        other = parent_tx(CScript([b"\x01"]), COutPoint(1, 0))
        root = hash256(txid(coinbase) + txid(other))
        self.assert_accepted(aux, serialize_auxpow(coinbase, [txid(other)], 0, [], 0, root, aux["bits"]))

        self.log.info("Two competing S256 blocks in one merge-mining tree: only the expected slot counts")
        nonce = 7
        slot = expected_index(nonce, S256_CHAIN_ID, 1)

        def two_blocks():
            a, b = node.createauxblock(addr1), node.createauxblock(addr2)
            assert a["hash"] != b["hash"]
            leaves = [None, None]
            leaves[slot], leaves[1 - slot] = merkle_leaf(a["hash"]), merkle_leaf(b["hash"])
            cb = parent_tx(CScript([merge_mining_tag(hash256(leaves[0] + leaves[1]), 2, nonce)]))
            return a, b, leaves, cb, txid(cb)

        a, b, leaves, cb, root = two_blocks()
        self.assert_rejected(b, serialize_auxpow(cb, [], 0, [leaves[slot]], 1 - slot, root, b["bits"]),
                             "auxpow-wrong-index")
        a, b, leaves, cb, root = two_blocks()
        self.assert_rejected(b, serialize_auxpow(cb, [], 0, [leaves[1 - slot]], slot, root, b["bits"]),
                             "auxpow-chain-merkle-mismatch")
        a, b, leaves, cb, root = two_blocks()
        self.assert_accepted(a, serialize_auxpow(cb, [], 0, [leaves[1 - slot]], slot, root, a["bits"]))

        self.log.info("Reject a parent coinbase with two merge-mining tags")
        for submit_first in (True, False):
            a, b = node.createauxblock(addr1), node.createauxblock(addr2)
            cb = parent_tx(CScript([merge_mining_tag(merkle_leaf(a["hash"])), merge_mining_tag(merkle_leaf(b["hash"]))]))
            target = a if submit_first else b
            self.assert_rejected(target, serialize_auxpow(cb, [], 0, [], 0, txid(cb), target["bits"]),
                                 "auxpow-multiple-merge-mining-tags")

        assert_equal(node.getblockcount(), 23)
        assert_equal(self.nodes[1].getbestblockhash(), node.getbestblockhash())


if __name__ == '__main__':
    AuxpowCommitmentTest(__file__).main()
