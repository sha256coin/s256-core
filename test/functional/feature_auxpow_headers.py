#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test serving and storing the headers of merge-mined (AuxPoW) blocks.

The block index does not store the auxpow, so every path that rebuilds a
header from the index has to read a merge-mined block's header from disk:

- a fresh peer syncs headers and blocks from a node holding merge-mined blocks
- getblockheader verbose=false and REST /headers return the full header
- a node with merge-mined blocks whose own hash misses the target restarts,
  reads those blocks back, and reorgs across them
- headers presync (-minimumchainwork) carries the auxpow through redownload
- a headers-only node and a pruned node, which lack the block data, return
  errors instead of crashing, and the pruned node still serves the headers
  it can
- a block template with the auxpow bit but no auxpow is refused, not a crash
"""

import http.client
import urllib.parse

from test_framework.auxpow import (
    header_hash,
    merge_mine,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
    p2p_port,
)

MINER, FRESH, PRESYNC, PRUNED, HEADERS_ONLY, FROM_PRUNED = range(6)


class AuxpowHeadersTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 6
        self.setup_clean_chain = True
        self.wallet_names = []
        self.extra_args = [
            ["-rest"],
            [],
            [],
            ["-rest", "-prune=1", "-fastprune"],
            ["-rest"],
            [],
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # Connections are made per scenario.
        self.setup_nodes()

    def rest_get(self, node, path):
        url = urllib.parse.urlparse(node.url)
        conn = http.client.HTTPConnection(url.hostname, url.port)
        conn.request("GET", path)
        resp = conn.getresponse()
        return resp.status, resp.read()

    def headers_hex(self, node, first_height, count):
        return "".join(node.getblockheader(node.getblockhash(h), False)
                       for h in range(first_height, first_height + count))

    def run_test(self):
        miner = self.nodes[MINER]
        miner.createwallet("w")
        address = miner.get_wallet_rpc("w").getnewaddress()
        self.generatetoaddress(miner, 20, address, sync_fun=self.no_op)

        self.log.info("Merge-mine blocks, half of them with an own hash above the target")
        aux_hashes = [merge_mine(miner, address, own_hash_above_target=(i % 2 == 0)) for i in range(6)]
        first_aux_height = miner.getblockheader(aux_hashes[0])["height"]
        self.generatetoaddress(miner, 5, address, sync_fun=self.no_op)

        self.log.info("getblockheader verbose=false returns the full header with its auxpow")
        for h in aux_hashes:
            hex_header = miner.getblockheader(h, False)
            assert_greater_than(len(hex_header), 2 * 80)
            assert_equal(header_hash(hex_header), h)
            assert_equal(miner.getblockheader(h)["hash"], h)

        self.log.info("REST /headers returns the same headers")
        expected = self.headers_hex(miner, first_aux_height, len(aux_hashes))
        status, body = self.rest_get(miner, f"/rest/headers/{aux_hashes[0]}.hex?count={len(aux_hashes)}")
        assert_equal(status, 200)
        assert_equal(body.decode().strip(), expected)
        status, body = self.rest_get(miner, f"/rest/headers/{aux_hashes[0]}.bin?count={len(aux_hashes)}")
        assert_equal(status, 200)
        assert_equal(body.hex(), expected)

        self.log.info("A fresh peer syncs from the node holding merge-mined blocks")
        self.connect_nodes(FRESH, MINER)
        self.sync_blocks([miner, self.nodes[FRESH]])
        assert_equal(miner.getblockcount(), self.nodes[FRESH].getblockcount())

        self.log.info("Restart with merge-mined blocks in the index whose own hash misses the target")
        self.restart_node(MINER, extra_args=self.extra_args[MINER])
        assert_equal(miner.getbestblockhash(), self.nodes[FRESH].getbestblockhash())
        for h in aux_hashes:
            assert_equal(miner.getblockstats(h)["blockhash"], h)  # read through ReadBlock

        self.log.info("Reorg across merge-mined blocks (disconnect and reconnect read them from disk)")
        fresh = self.nodes[FRESH]
        tip = fresh.getbestblockhash()
        fresh.invalidateblock(aux_hashes[0])
        assert_equal(fresh.getblockcount(), first_aux_height - 1)
        fresh.reconsiderblock(aux_hashes[0])
        assert_equal(fresh.getbestblockhash(), tip)

        self.log.info("Extend the chain past one full headers message")
        self.generatetodescriptor(miner, 2050, miner.getdescriptorinfo("raw(51)")["descriptor"], sync_fun=self.no_op)
        self.connect_nodes(MINER, FRESH)
        self.sync_blocks([miner, fresh])

        self.log.info("Headers presync and redownload carry the auxpow")
        # Minimum work is reached only after the first full headers message,
        # so the whole start of the chain, merge-mined headers included, goes
        # through presync and redownload.
        min_work = miner.getblockheader(miner.getblockhash(miner.getblockcount() - 20))["chainwork"]
        self.restart_node(PRESYNC, extra_args=[f"-minimumchainwork=0x{min_work}", "-debug=net"])
        presync = self.nodes[PRESYNC]
        with presync.assert_debug_log(["Initial headers sync complete with peer"], timeout=60):
            self.connect_nodes(PRESYNC, MINER)
            self.sync_blocks([miner, presync], timeout=120)

        self.log.info("A headers-only node returns errors for headers it cannot serve")
        headers_only = self.nodes[HEADERS_ONLY]
        for height in range(1, first_aux_height + len(aux_hashes) + 1):
            headers_only.submitheader(miner.getblockheader(miner.getblockhash(height), False))
        assert_equal(headers_only.getblockchaininfo()["headers"], first_aux_height + len(aux_hashes))
        assert_raises_rpc_error(-1, "Block header not available", headers_only.getblockheader, aux_hashes[0], False)
        assert_equal(headers_only.getblockheader(aux_hashes[0])["hash"], aux_hashes[0])
        # REST /headers only serves the active chain, which here is just genesis.
        status, body = self.rest_get(headers_only, f"/rest/headers/{aux_hashes[0]}.hex?count=1")
        assert_equal((status, body.decode().strip()), (200, ""))

        self.log.info("A pruned node returns errors for pruned headers and serves the rest")
        pruned = self.nodes[PRUNED]
        self.connect_nodes(PRUNED, MINER)
        self.sync_blocks([miner, pruned])
        self.disconnect_nodes(PRUNED, MINER)
        pruned.pruneblockchain(1000)
        assert_greater_than(pruned.getblockchaininfo()["pruneheight"], first_aux_height + len(aux_hashes))
        assert_raises_rpc_error(-1, "Block header not available", pruned.getblockheader, aux_hashes[0], False)
        status, _ = self.rest_get(pruned, f"/rest/headers/{aux_hashes[0]}.bin?count=1")
        assert_equal(status, 404)
        # A fresh peer that can only ask the pruned node gets the headers below
        # the first pruned merge-mined block, and the pruned node keeps running.
        # The peer then asks for pruned blocks and is disconnected, possibly
        # before connect_nodes() would see the connection settle, so connect
        # with a plain one-shot addnode.
        from_pruned = self.nodes[FROM_PRUNED]
        from_pruned.addnode(f"127.0.0.1:{p2p_port(PRUNED)}", "onetry")
        self.wait_until(lambda: from_pruned.getblockchaininfo()["headers"] == first_aux_height - 1)
        assert_equal(pruned.getblockcount(), miner.getblockcount())

        self.log.info("A template with the auxpow bit but no auxpow is refused, not a crash")
        self.restart_node(FRESH, extra_args=[f"-blockversion={0x20000000 | (1 << 8)}"])
        descriptor = miner.getdescriptorinfo("raw(51)")["descriptor"]
        assert_raises_rpc_error(None, "auxpow bit set but no auxpow",
                                lambda: self.generatetodescriptor(self.nodes[FRESH], 1, descriptor, sync_fun=self.no_op))
        assert_equal(self.nodes[FRESH].getblockcount(), miner.getblockcount())


if __name__ == '__main__':
    AuxpowHeadersTest(__file__).main()
