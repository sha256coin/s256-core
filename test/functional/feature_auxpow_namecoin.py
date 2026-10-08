#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Cross-check the auxpow format against Namecoin's own test helpers.

data/namecoin/ holds Namecoin's test_framework/auxpow.py and
auxpow_testing.py, unmodified (checked by hash below). Their auxpows fill in
the hashBlock field with the parent block hash.

- an auxpow from Namecoin's helpers is accepted by submitauxblock, and one
  that doesn't meet the target is rejected
- the node stores and relays it with hashBlock zeroed, and the full block
  (Namecoin format) is accepted by submitblock on a second node
"""

import hashlib
import importlib.util
from io import BytesIO
from pathlib import Path

import test_framework
from test_framework.messages import CBlock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

NAMECOIN_DIR = Path(__file__).resolve().parent / "data" / "namecoin"
# namecoin/namecoin-core d52dff4d3eadee3a09dbf93b5fee67a9f1329a2e, test/functional/test_framework/
NAMECOIN_FILES = {
    "auxpow.py": "46ea1d4201841c2f520b872663475c5c1214f040147f5763dad619a2ad2d79d6",
    "auxpow_testing.py": "8b58d2c22e8e36dae85b0a65a0f10723ae39dbfe1c0c1e5c14cc0456105e69d9",
}


def load_namecoin_helpers():
    """Load Namecoin's helpers as they are. auxpow_testing.py does
    "from test_framework import auxpow", which must give Namecoin's auxpow.py,
    not ours, while it is imported."""
    for name, digest in NAMECOIN_FILES.items():
        assert_equal(hashlib.sha256((NAMECOIN_DIR / name).read_bytes()).hexdigest(), digest)

    def load(name):
        spec = importlib.util.spec_from_file_location(f"namecoin_{name[:-3]}", NAMECOIN_DIR / name)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module

    nmc_auxpow = load("auxpow.py")
    ours = getattr(test_framework, "auxpow", None)
    test_framework.auxpow = nmc_auxpow
    try:
        nmc_testing = load("auxpow_testing.py")
    finally:
        if ours is None:
            del test_framework.auxpow
        else:
            test_framework.auxpow = ours
    return nmc_auxpow, nmc_testing


class AuxpowNamecoinTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        nmc_auxpow, nmc_testing = load_namecoin_helpers()
        node, other = self.nodes
        addr = node.get_deterministic_priv_key().address
        self.generatetoaddress(node, 5, addr)

        self.log.info("An auxpow from Namecoin's helpers that misses the target is rejected")
        aux = node.createauxblock(addr)
        target = nmc_auxpow.reverseHex(aux["_target"])
        bad = nmc_testing.computeAuxpow(aux["hash"], target, False)
        assert_equal(node.submitauxblock(aux["hash"], bad), False)

        self.log.info("Namecoin's mineAuxpowBlock merge-mines a block (createauxblock, submitauxblock)")
        self.disconnect_nodes(0, 1)
        block_hash = nmc_testing.mineAuxpowBlock(node, addr)
        assert_equal(node.getbestblockhash(), block_hash)

        self.log.info("The node keeps the auxpow with hashBlock zeroed")
        raw = bytes.fromhex(node.getblock(block_hash, 0))
        block = CBlock()
        block.deserialize(BytesIO(raw))
        coinbase_len = len(block.auxpow.coinbaseTx.serialize_with_witness())
        assert_equal(raw[80 + coinbase_len:80 + coinbase_len + 32], bytes(32))

        self.log.info("The same block with a non-zero hashBlock (Namecoin format) is accepted by submitblock")
        hash_block = hashlib.sha256(b"parent").digest()
        namecoin_raw = raw[:80 + coinbase_len] + hash_block + raw[80 + coinbase_len + 32:]
        assert_equal(other.submitblock(namecoin_raw.hex()), None)
        assert_equal(other.getbestblockhash(), block_hash)
        assert_equal(other.getblock(block_hash, 0), raw.hex())


if __name__ == "__main__":
    AuxpowNamecoinTest(__file__).main()
