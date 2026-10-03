#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test when a node leaves initial block download.

- a fresh node at genesis is in IBD (no genesis shortcut)
- a node restarted with a tip younger than the default max tip age (7 days)
  is not in IBD; with an older tip it is, until a new block arrives
- -maxtipage and -minimumchainwork still keep a node in IBD
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

HOUR = 3600
DAY = 24 * HOUR


class IBDTipAgeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def ibd(self):
        return self.nodes[0].getblockchaininfo()["initialblockdownload"]

    def tip_time(self):
        node = self.nodes[0]
        return node.getblockheader(node.getbestblockhash())["time"]

    def restart(self, clock, extra_args=()):
        self.restart_node(0, [f"-mocktime={clock}", *extra_args])

    def run_test(self):
        node = self.nodes[0]
        address = node.get_deterministic_priv_key().address

        self.log.info("A fresh node at genesis is in IBD until its tip is recent")
        assert_equal(node.getblockcount(), 0)
        assert_equal(self.ibd(), True)
        self.generatetoaddress(node, 20, address)
        assert_equal(self.ibd(), False)

        self.log.info("Restart with a tip 25 h old (pool node during a block gap): not in IBD")
        self.restart(self.tip_time() + 25 * HOUR)
        assert_equal(self.ibd(), False)

        self.log.info("Restart with a tip 6 days old: not in IBD")
        self.restart(self.tip_time() + 6 * DAY)
        assert_equal(self.ibd(), False)

        self.log.info("Restart with a tip 8 days old: in IBD until a new block arrives")
        clock = self.tip_time() + 8 * DAY
        self.restart(clock)
        assert_equal(self.ibd(), True)
        self.generatetoaddress(node, 1, address)
        assert_equal(self.ibd(), False)

        self.log.info("-maxtipage=86400 (Bitcoin's default) keeps a 25 h old tip in IBD")
        self.restart(self.tip_time() + 25 * HOUR, ["-maxtipage=86400"])
        assert_equal(self.ibd(), True)

        self.log.info("A recent tip below -minimumchainwork is in IBD")
        self.restart(self.tip_time() + HOUR, [f"-minimumchainwork=0x{'ff' * 16}"])
        assert_equal(self.ibd(), True)


if __name__ == '__main__':
    IBDTipAgeTest(__file__).main()
