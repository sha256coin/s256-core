#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that createauxblock does not wait for initial block download.

A test chain (regtest, testnet, signet) may be in IBD because its tip is
older than the max tip age (a fresh regtest node has only the old genesis
block, a quiet testnet has been idle for a week). createauxblock exempts test
chains from its own IBD refusal, but it used to call createNewBlock with its
default cooldown, which waits for IBD to end. It did so holding the template
mutex, so every other createauxblock call queued behind it, all RPC worker
threads ended up blocked, and nothing could submit the block that would end
the wait: the whole RPC server hung. getblocktemplate and the generate RPCs
opt out of the cooldown; createauxblock now does too.
"""

from test_framework.auxpow import create_auxpow
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class CreateAuxBlockIBDTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        # The old code hangs; fail after this many seconds instead of 60 x timeout_factor.
        self.rpc_timeout = 10

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        node.createwallet("w")
        addr = node.get_wallet_rpc("w").getnewaddress()
        assert_equal(node.getblockcount(), 0)
        assert node.getblockchaininfo()["initialblockdownload"]

        self.log.info("createauxblock returns a template in IBD instead of waiting for it to end")
        aux = node.createauxblock(addr)
        assert_equal(aux["height"], 1)
        assert_equal(node.createauxblock(addr), aux)

        self.log.info("The node still answers other RPCs, and the block can be submitted")
        assert_equal(node.getblockcount(), 0)
        assert node.submitauxblock(aux["hash"], create_auxpow(aux["hash"], aux["bits"]))
        assert_equal(node.getbestblockhash(), aux["hash"])
        assert_equal(node.createauxblock(addr)["height"], 2)


if __name__ == '__main__':
    CreateAuxBlockIBDTest(__file__).main()
