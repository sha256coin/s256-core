#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test size-limited "headers" messages with large merge-mined headers.

Merge-mined headers carry an auxpow of any size (a pool's parent coinbase).
- a getheaders reply stops at the header that reaches THRESHOLD_HEADERS_SIZE
  bytes, for old and new peers, and never exceeds the message limit
- a node syncs through several such replies, normally and through the
  low-work headers presync
- a header announcement over the threshold falls back to an inv
- messages.py round-trips merge-mined headers and blocks
"""

from io import BytesIO

from test_framework.auxpow import (
    merge_mining_tag,
    merkle_leaf,
    parent_tx,
    serialize_auxpow,
)
from test_framework.messages import (
    CBlock,
    CBlockHeader,
    CTxOut,
    MAX_HEADERS_RESULTS,
    from_hex,
    msg_getheaders,
    msg_headers,
    msg_sendheaders,
)
from test_framework.p2p import P2PInterface
from test_framework.script import CScript
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
)

THRESHOLD_HEADERS_SIZE = 2_000_000
MAX_PROTOCOL_MESSAGE_LENGTH = 4_000_000
OLD_P2P_VERSION = 70016


def big_auxpow(aux, padding):
    """An auxpow whose parent coinbase has an output of `padding` bytes."""
    coinbase = parent_tx(CScript([merge_mining_tag(merkle_leaf(aux["hash"]))]))
    coinbase.vout.append(CTxOut(0, CScript([b"\x00" * padding])))
    return serialize_auxpow(coinbase, [], 0, [], 0, coinbase.txid_int.to_bytes(32, "little"), aux["bits"])


class msg_raw:
    """A message sent as given (a serialized block from the node)."""
    __slots__ = ("msgtype", "data")

    def __init__(self, msgtype, data):
        self.msgtype = msgtype
        self.data = data

    def serialize(self):
        return self.data


class HeadersCollector(P2PInterface):
    def __init__(self, version=None):
        super().__init__()
        self.version = version
        self.headers_msgs = []
        self.invs = []

    def peer_connect_send_version(self, services):
        super().peer_connect_send_version(services)
        if self.version is not None:
            self.on_connection_send_msg.nVersion = self.version

    def on_headers(self, message):
        self.headers_msgs.append(message.headers)

    def on_inv(self, message):
        super().on_inv(message)
        self.invs.append(message.inv)


class AuxpowHeadersP2PTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        self.setup_clean_chain = True
        # node1: fresh syncing node; node2: syncs through the headers presync;
        # node3: builds blocks for the announcement test.
        self.extra_args = [[], [], [], []]

    def setup_network(self):
        self.setup_nodes()

    def merge_mine(self, node, address, padding):
        aux = node.createauxblock(address)
        auxpow = big_auxpow(aux, padding)
        assert node.submitauxblock(aux["hash"], auxpow)
        return aux["hash"]

    def get_all_headers(self, peer, node):
        """Walk node's chain with getheaders; return the headers messages."""
        peer.headers_msgs = []
        locator = [int(node.getblockhash(0), 16)]
        while True:
            count = len(peer.headers_msgs)
            msg = msg_getheaders()
            msg.locator.vHave = locator
            peer.send_and_ping(msg)
            peer.wait_until(lambda: len(peer.headers_msgs) > count)
            headers = peer.headers_msgs[-1]
            if not headers:
                return peer.headers_msgs[:-1]
            locator = [headers[-1].hash_int]

    def check_capped(self, msgs, node):
        """Every message but the last stops at the threshold; together they
        are the whole chain."""
        for headers in msgs[:-1]:
            sizes = [len(h.serialize()) for h in headers]
            assert_greater_than(MAX_HEADERS_RESULTS, len(headers))
            assert_greater_than_or_equal(sum(sizes), THRESHOLD_HEADERS_SIZE)
            assert_greater_than(THRESHOLD_HEADERS_SIZE, sum(sizes[:-1]))
            assert_greater_than(MAX_PROTOCOL_MESSAGE_LENGTH, len(msg_headers(headers).serialize()))
        all_headers = [h for headers in msgs for h in headers]
        assert_equal([h.hash_hex for h in all_headers],
                     [node.getblockhash(i) for i in range(1, node.getblockcount() + 1)])
        return all_headers

    def run_test(self):
        node, fresh, presync_node, builder = self.nodes
        address = node.get_deterministic_priv_key().address
        self.generatetoaddress(node, 20, address, sync_fun=self.no_op)

        self.log.info("Merge-mine 30 blocks with ~150 KB auxpows")
        for _ in range(30):
            self.merge_mine(node, address, 150_000)
        tip = node.getbestblockhash()

        self.log.info("messages.py round-trips a merge-mined block")
        raw = node.getblock(tip, 0)
        block = from_hex(CBlock(), raw)
        assert block.auxpow is not None
        assert_equal(block.serialize().hex(), raw)
        assert_equal(block.hash_hex, tip)
        header = CBlockHeader()
        header.deserialize(BytesIO(bytes.fromhex(raw)))
        assert_equal(header.serialize().hex(), node.getblockheader(tip, False))

        self.log.info("getheaders replies stop at the size threshold")
        for version in (None, OLD_P2P_VERSION):
            peer = node.add_p2p_connection(HeadersCollector(version))
            msgs = self.get_all_headers(peer, node)
            assert_greater_than(len(msgs), 2)
            headers = self.check_capped(msgs, node)
            assert all(h.auxpow is not None for h in headers[20:])
            node.disconnect_p2ps()

        self.log.info("A fresh node syncs through size-capped replies")
        self.connect_nodes(1, 0)
        self.sync_blocks([node, fresh], timeout=120)
        self.disconnect_nodes(1, 0)

        self.log.info("A node syncs through size-capped replies in the headers presync")
        chainwork = node.getblockheader(tip)["chainwork"]
        self.restart_node(2, [f"-minimumchainwork=0x{chainwork}", "-debug=net"])
        with presync_node.assert_debug_log(["Initial headers sync complete with peer"], timeout=120):
            self.connect_nodes(2, 0)
            self.sync_blocks([node, presync_node], timeout=120)
        self.disconnect_nodes(2, 0)

        self.log.info("Header announcements over the threshold fall back to an inv")
        self.connect_nodes(3, 0)
        self.sync_blocks([node, builder])
        self.disconnect_nodes(3, 0)
        listener = node.add_p2p_connection(HeadersCollector())
        listener.send_and_ping(msg_sendheaders())
        sender = node.add_p2p_connection(HeadersCollector())
        builder_address = builder.get_deterministic_priv_key().address

        for padding, expect_headers in ((1_000, True), (750_000, False)):
            # The listener has the tip header, so announcements can connect.
            getheaders = msg_getheaders()
            getheaders.locator.vHave = [int(node.getbestblockhash(), 16)]
            listener.send_and_ping(getheaders)
            listener.headers_msgs, listener.invs = [], []
            hashes = [self.merge_mine(builder, builder_address, padding) for _ in range(3)]
            blocks = [bytes.fromhex(builder.getblock(h, 0)) for h in hashes]
            headers = [from_hex(CBlockHeader(), b.hex()) for b in blocks]
            # Headers first, then the blocks in reverse, so the node connects
            # all three at once and announces them together.
            sender.send_and_ping(msg_headers(headers))
            for b in reversed(blocks):
                sender.send_and_ping(msg_raw(b"block", b))
            node.waitforblock(hashes[-1])
            listener.sync_with_ping()
            announced = [h.hash_hex for msg in listener.headers_msgs for h in msg]
            inved = [f"{i.hash:064x}" for msg in listener.invs for i in msg]
            if expect_headers:
                assert_equal(announced, hashes)
            else:
                assert_equal(announced, [])
                assert_equal(inved, [hashes[-1]])


if __name__ == '__main__':
    AuxpowHeadersP2PTest(__file__).main()
