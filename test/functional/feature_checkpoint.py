#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the checkpoint rule.

A header at a checkpoint height must be the checkpoint block: a header with
another hash there is rejected with "checkpoint-mismatch", so a chain that
forks before the checkpoint is never followed, even a longer one. Mainnet has
one checkpoint (block 18,000); regtest can set one with the hidden option
-testcheckpoint=<height>:<hash>, which every other network refuses to start
with.
"""

import re
import subprocess
import tempfile
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import (
    assert_equal,
    assert_not_equal,
    assert_raises_rpc_error,
)

CHECKPOINT_HEIGHT = 5


class CheckpointTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        self.setup_clean_chain = True

    def setup_network(self):
        self.setup_nodes()  # no connections: the chains must stay apart

    def run_test(self):
        node_x, node_y, node_c = self.nodes
        addr = node_x.get_deterministic_priv_key().address

        self.log.info("Two different chains from genesis: X (7 blocks) and Y (8 blocks, longer)")
        base = int(time.time())
        node_x.setmocktime(base)
        node_y.setmocktime(base + 60)
        node_c.setmocktime(base + 60)
        self.generatetoaddress(node_x, 7, addr, sync_fun=self.no_op)
        self.generatetoaddress(node_y, 8, addr, sync_fun=self.no_op)
        x_hash = node_x.getblockhash(CHECKPOINT_HEIGHT)
        y_hash = node_y.getblockhash(CHECKPOINT_HEIGHT)
        assert_not_equal(x_hash, y_hash)

        self.log.info("Malformed -testcheckpoint values are init errors")
        self.stop_node(2)
        for bad in (str(CHECKPOINT_HEIGHT), f"x:{x_hash}", f"0:{x_hash}", f"{CHECKPOINT_HEIGHT}:abcd"):
            node_c.assert_start_raises_init_error(
                [f"-testcheckpoint={bad}"],
                re.escape(f"Error: Invalid format ({bad}) for -testcheckpoint=height:hash."),
                match=ErrorMatch.PARTIAL_REGEX)

        self.log.info("Mainnet refuses to start with -testcheckpoint (an init error, not ignored)")
        with tempfile.TemporaryDirectory() as datadir:  # no config file: mainnet
            proc = subprocess.run(
                self.get_binaries().node_argv() + [f"-datadir={datadir}", f"-testcheckpoint={CHECKPOINT_HEIGHT}:{x_hash}",
                                                   "-listen=0", "-connect=0", "-dnsseed=0", "-fixedseeds=0", "-server=0"],
                capture_output=True, text=True, timeout=60)
        assert_not_equal(proc.returncode, 0)
        assert "-testcheckpoint is only available on regtest" in proc.stderr, proc.stderr

        self.log.info("A node with the checkpoint follows the chain that matches it")
        self.start_node(2, [f"-testcheckpoint={CHECKPOINT_HEIGHT}:{x_hash}"])
        node_c.setmocktime(base + 60)
        self.connect_nodes(2, 0)
        self.sync_blocks([node_x, node_c])
        assert_equal(node_c.getbestblockhash(), node_x.getbestblockhash())
        self.disconnect_nodes(2, 0)

        self.log.info("Headers of the other chain are accepted below the checkpoint height, and rejected at it")
        for height in range(1, CHECKPOINT_HEIGHT):
            node_c.submitheader(node_y.getblockheader(node_y.getblockhash(height), False))
        assert_raises_rpc_error(-25, "checkpoint-mismatch",
                                node_c.submitheader, node_y.getblockheader(y_hash, False))

        self.log.info("The longer chain is not followed when its peer offers it")
        with node_c.assert_debug_log(["checkpoint-mismatch"], timeout=10):
            self.connect_nodes(2, 1)
        assert_equal(node_c.getbestblockhash(), node_x.getbestblockhash())
        assert_equal(node_c.getblockcount(), 7)
        assert_raises_rpc_error(-5, "Block not found", node_c.getblockheader, node_y.getblockhash(8))


if __name__ == '__main__':
    CheckpointTest(__file__).main()
