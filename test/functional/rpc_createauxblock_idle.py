#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test createauxblock templates on a node with no new blocks or transactions.

- the template is rebuilt once it is a minute old, whatever the mempool does,
  so a block found by a pool that polls an idle node carries a current
  timestamp (a cached template would give it the time of the first poll)
- a superseded template stays submittable for 10 minutes, since a pool's
  parent job can hold an older hash that long, and is then rejected by
  submitauxblock even if nobody polled createauxblock in between
- the newest template stays submittable whatever its age
"""

import time

from test_framework.auxpow import create_auxpow
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

REFRESH = 60
KEEP = 600


class CreateAuxBlockIdleTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        node.createwallet("w")
        addr = node.get_wallet_rpc("w").getnewaddress()
        self.generatetoaddress(node, 5, addr)
        clock = int(time.time())
        node.setmocktime(clock)

        def at(offset):
            node.setmocktime(clock + offset)

        def wrong_proof(template):
            """A proof for another block: rejected (False) if the template is
            known, an RPC error if it is not."""
            return node.submitauxblock(template["hash"], create_auxpow("11" * 32, template["bits"]))

        self.log.info("An idle node's template is rebuilt every minute")
        t0 = node.createauxblock(addr)
        at(REFRESH)
        assert_equal(node.createauxblock(addr), t0)
        at(REFRESH + 1)
        t1 = node.createauxblock(addr)
        assert t1["hash"] != t0["hash"]
        assert_equal(node.createauxblock(addr), t1)
        at(2 * REFRESH + 2)
        t2 = node.createauxblock(addr)
        assert t2["hash"] not in (t0["hash"], t1["hash"])
        assert_equal(len(node.getrawmempool()), 0)

        self.log.info("A template found after two refreshes is still accepted")
        assert_equal(wrong_proof(t0), False)
        assert_equal(node.submitauxblock(t0["hash"], create_auxpow(t0["hash"], t0["bits"])), True)
        assert_equal(node.getbestblockhash(), t0["hash"])
        clock += 2 * REFRESH + 2

        self.log.info("A superseded template expires after 10 minutes, the newest never")
        at(0)
        d0 = node.createauxblock(addr)
        at(REFRESH + 1)
        d1 = node.createauxblock(addr)
        at(2 * REFRESH + 2)
        d2 = node.createauxblock(addr)
        assert len({d0["hash"], d1["hash"], d2["hash"]}) == 3
        at(KEEP)
        assert_equal(wrong_proof(d0), False)  # exactly 10 minutes: kept
        at(KEEP + 1)
        for expired in (d0,):
            assert_raises_rpc_error(-8, "block hash unknown", wrong_proof, expired)
        assert_equal(wrong_proof(d1), False)  # created at +61, 540 s old
        at(KEEP + REFRESH + 2)
        assert_raises_rpc_error(-8, "block hash unknown", wrong_proof, d1)
        at(100 * KEEP)
        assert_equal(wrong_proof(d2), False)  # newest, however old

        self.log.info("Blocks found while polling an idle node carry a current timestamp")
        for offset in range(100 * KEEP, 100 * KEEP + 1000, 20):
            at(offset)
            template = node.createauxblock(addr)
        found_at = clock + 100 * KEEP + 980
        assert node.submitauxblock(template["hash"], create_auxpow(template["hash"], template["bits"]))
        block_time = node.getblockheader(template["hash"])["time"]
        assert found_at - REFRESH <= block_time <= found_at, (block_time, found_at)


if __name__ == '__main__':
    CreateAuxBlockIdleTest(__file__).main()
