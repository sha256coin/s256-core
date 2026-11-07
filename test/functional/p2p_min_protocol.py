#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the minimum peer protocol version (70101).

Nodes older than 3.0.1 cannot decode merge-mined blocks in Namecoin's auxpow
format, so a version message below 70101 is answered with a disconnect, and
the node logs why. A peer at exactly 70101 stays connected. Software that
connects over P2P for block notifications (some mining pools do) has to send
at least 70101 as well.
"""

from test_framework.p2p import (
    P2P_VERSION,
    P2PInterface,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

MIN_PEER_PROTO_VERSION = 70101


class VersionPeer(P2PInterface):
    """A peer that announces the given protocol version."""

    def __init__(self, version):
        super().__init__()
        self.version = version
        self.opened = False

    def on_open(self):
        self.opened = True

    def peer_connect_send_version(self, services):
        super().peer_connect_send_version(services)
        self.on_connection_send_msg.nVersion = self.version


class MinProtocolTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]
        assert_equal(P2P_VERSION, MIN_PEER_PROTO_VERSION)

        self.log.info("A peer at the minimum version stays connected")
        peer = node.add_p2p_connection(VersionPeer(MIN_PEER_PROTO_VERSION))
        peer.sync_with_ping()
        info = node.getpeerinfo()
        assert_equal(len(info), 1)
        assert_equal(info[0]["version"], MIN_PEER_PROTO_VERSION)
        peer.peer_disconnect()
        peer.wait_for_disconnect()

        for old_version in (MIN_PEER_PROTO_VERSION - 1, 70016, 70002):
            self.log.info(f"A peer announcing {old_version} is disconnected, and the node says why")
            with node.assert_debug_log([f"peer using obsolete version {old_version}"]):
                # The node drops the peer as soon as it reads the version, which
                # on a fast machine can be before add_p2p_connection() polls
                # is_connected: don't wait for the connection, wait for it to
                # have opened and closed.
                peer = node.add_p2p_connection(VersionPeer(old_version), expect_success=False)
                peer.wait_until(lambda: peer.opened and not peer.is_connected, check_connected=False)
            self.wait_until(lambda: len(node.getpeerinfo()) == 0)


if __name__ == '__main__':
    MinProtocolTest(__file__).main()
