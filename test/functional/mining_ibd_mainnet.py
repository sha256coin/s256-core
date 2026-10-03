#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test getblocktemplate and createauxblock on mainnet during IBD.

Test chains skip these checks, so this runs two mainnet nodes connected only
to each other, with the tip at genesis and the clock mocked to make the tip a
given age. -minimumchainwork=0 so only the tip age decides, except where
stated.

- a pool node whose tip is 25 h or 6 days old (a restart during a block gap)
  gets templates from both RPCs
- with a tip 8 days old, or Bitcoin's 24 h -maxtipage, or a tip below the
  real minimum chain work, both refuse
- both refuse without peers
"""

from test_framework.segwit_addr import encode_segwit_address
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

GENESIS_TIME = 1764497487
HOUR = 3600
DAY = 24 * HOUR
ADDRESS = encode_segwit_address("s2", 0, bytes(20))
IN_IBD = (-10, "is in initial sync and waiting for blocks")
NOT_CONNECTED = (-9, "is not connected")


class MiningIBDMainnetTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.chain = ""  # main

    def setup_network(self):
        self.setup_nodes()

    def start_pair(self, age, extra_args=("-minimumchainwork=0x00",)):
        args = [f"-mocktime={GENESIS_TIME + age}", *extra_args]
        for i in range(2):
            self.start_node(i, args)
        self.connect_nodes(0, 1)

    def stop_pair(self):
        self.stop_nodes()

    def templates(self):
        node = self.nodes[0]
        return (node.getblocktemplate({"rules": ["segwit"]}), node.createauxblock(ADDRESS))

    def assert_refused(self, code_message):
        node = self.nodes[0]
        assert_raises_rpc_error(*code_message, node.getblocktemplate, {"rules": ["segwit"]})
        assert_raises_rpc_error(*code_message, node.createauxblock, ADDRESS)

    def run_test(self):
        self.stop_nodes()

        for age in (25 * HOUR, 6 * DAY):
            self.log.info(f"Tip {age // HOUR} h old: both RPCs give templates")
            self.start_pair(age)
            assert_equal(self.nodes[0].getblockchaininfo()["initialblockdownload"], False)
            gbt, aux = self.templates()
            assert_equal(gbt["height"], 1)
            assert_equal(aux["height"], 1)
            self.stop_pair()

        self.log.info("Tip 8 days old: both refuse")
        self.start_pair(8 * DAY)
        self.assert_refused(IN_IBD)
        self.stop_pair()

        self.log.info("Tip 25 h old with -maxtipage=86400: both refuse")
        self.start_pair(25 * HOUR, ("-minimumchainwork=0x00", "-maxtipage=86400"))
        self.assert_refused(IN_IBD)
        self.stop_pair()

        self.log.info("Recent tip below the real minimum chain work: both refuse")
        self.start_pair(HOUR, ())
        self.assert_refused(IN_IBD)
        self.stop_pair()

        self.log.info("No peers: both refuse")
        self.start_node(0, [f"-mocktime={GENESIS_TIME + HOUR}", "-minimumchainwork=0x00"])
        self.assert_refused(NOT_CONNECTED)


if __name__ == '__main__':
    MiningIBDMainnetTest(__file__).main()
