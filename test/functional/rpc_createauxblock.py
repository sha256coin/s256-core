#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test createauxblock template handling.

- polling returns the same template per payout address until the tip
  changes, or until the mempool changed and the template is a minute old
- a template stays submittable, also after a rejected submission, until a
  call after a tip change drops it
- templates leave -auxpowreservedweight room for the auxpow
"""

import re
import time

from test_framework.auxpow import create_auxpow
from test_framework.test_framework import BitcoinTestFramework
from test_framework.test_node import ErrorMatch
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)

COINBASE_MATURITY = 200  # S256: 2x Bitcoin's
DEFAULT_AUXPOW_RESERVED_WEIGHT = 40000
MAX_WEIGHT = 60000


class CreateAuxBlockTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        node.createwallet("w")
        wallet = node.get_wallet_rpc("w")
        addr1 = wallet.getnewaddress()
        addr2 = wallet.getnewaddress()
        self.generatetoaddress(node, COINBASE_MATURITY + 10, addr1)
        now = int(time.time())
        node.setmocktime(now)

        self.log.info("Polling reuses the template per address")
        first = node.createauxblock(addr1)
        assert_equal(node.createauxblock(addr1), first)
        other = node.createauxblock(addr2)
        assert other["hash"] != first["hash"]
        assert_equal(node.createauxblock(addr2), other)

        self.log.info("Mempool changes refresh the template only after a minute")
        wallet.sendtoaddress(wallet.getnewaddress(), 1)
        assert_equal(node.createauxblock(addr1)["hash"], first["hash"])
        node.setmocktime(now + 60)
        assert_equal(node.createauxblock(addr1)["hash"], first["hash"])
        node.setmocktime(now + 61)
        refreshed = node.createauxblock(addr1)
        assert refreshed["hash"] != first["hash"]
        assert_equal(node.createauxblock(addr1), refreshed)
        self.log.info("Without mempool changes the template is not refreshed")
        node.setmocktime(now + 200)
        assert_equal(node.createauxblock(addr1), refreshed)

        self.log.info("Templates stay submittable until the tip changes, also after a rejected proof")
        bad = create_auxpow(other["hash"], first["bits"])  # commits to a different block
        assert_equal(node.submitauxblock(first["hash"], bad), False)
        assert_equal(node.submitauxblock(first["hash"], create_auxpow(first["hash"], first["bits"])), True)
        assert_equal(node.getbestblockhash(), first["hash"])
        # The next call sees the new tip and drops the old templates.
        new_tip = node.createauxblock(addr1)
        assert_equal(new_tip["previousblockhash"], first["hash"])
        assert_equal(new_tip["height"], first["height"] + 1)
        for stale in (other, refreshed):
            assert_raises_rpc_error(-8, "block hash unknown", node.submitauxblock, stale["hash"],
                                    create_auxpow(stale["hash"], stale["bits"]))

        self.log.info("Templates leave room for the auxpow")
        node.setmocktime(0)
        self.restart_node(0, [f"-blockmaxweight={MAX_WEIGHT}"])
        node.loadwallet("w")
        wallet = node.get_wallet_rpc("w")
        for _ in range(40):
            wallet.sendtoaddress(wallet.getnewaddress(), 0.1)
        mempool_size = len(node.getrawmempool())
        gbt = node.getblocktemplate({"rules": ["segwit"]})
        assert_equal(len(gbt["transactions"]), mempool_size)  # fits with the default 8,000 WU reserve
        aux = node.createauxblock(addr1)
        auxpow = create_auxpow(aux["hash"], aux["bits"])
        assert node.submitauxblock(aux["hash"], auxpow)
        block = node.getblock(aux["hash"])
        weight_without_auxpow = block["weight"] - 4 * len(bytes.fromhex(auxpow))
        assert weight_without_auxpow <= MAX_WEIGHT - DEFAULT_AUXPOW_RESERVED_WEIGHT
        assert_greater_than(mempool_size, block["nTx"] - 1)
        assert_greater_than(block["nTx"] - 1, 0)

        self.log.info("-auxpowreservedweight=0 builds the same template as getblocktemplate")
        self.restart_node(0, [f"-blockmaxweight={MAX_WEIGHT}", "-auxpowreservedweight=0"])
        node.loadwallet("w")
        wallet = node.get_wallet_rpc("w")
        for _ in range(40 - len(node.getrawmempool())):
            wallet.sendtoaddress(wallet.getnewaddress(), 0.1)
        assert_equal(len(node.getblocktemplate({"rules": ["segwit"]})["transactions"]), 40)
        aux = node.createauxblock(addr1)
        assert node.submitauxblock(aux["hash"], create_auxpow(aux["hash"], aux["bits"]))
        assert_equal(node.getblock(aux["hash"])["nTx"], 41)

        self.log.info("Invalid -auxpowreservedweight values are rejected at startup")
        self.stop_node(0)
        for value in (-1, 4_000_000):
            node.assert_start_raises_init_error([f"-auxpowreservedweight={value}"],
                                                re.escape(f"Error: Specified -auxpowreservedweight ({value}) must be at least 0"),
                                                match=ErrorMatch.PARTIAL_REGEX)


if __name__ == '__main__':
    CreateAuxBlockTest(__file__).main()
