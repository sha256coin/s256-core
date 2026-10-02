#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test merge-mining S256 blocks via createauxblock/submitauxblock.

- an empty (coinbase-only) aux block is accepted
- an aux block carrying SegWit transactions is accepted, commits to their
  witnesses, confirms them, and relays to a peer
"""

from test_framework.messages import (
    COutPoint,
    CBlockHeader,
    CTransaction,
    CTxIn,
    CTxOut,
    ser_compact_size,
)
from test_framework.script import (
    CScript,
    OP_TRUE,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
)

MERGE_MINING_HEADER = bytes.fromhex("fabe6d6d")
VERSION_AUXPOW_BIT = 1 << 8
WITNESS_COMMITMENT_PREFIX = "6a24aa21a9ed"
COINBASE_MATURITY = 200  # S256: 2x Bitcoin's


def create_auxpow(aux_hash_hex, bits_hex):
    """Build a minimal auxpow for a single aux chain: a parent coinbase whose
    scriptSig carries the merge-mining tag, as the only transaction of a
    parent header ground to meet the aux block's target."""
    tag = (MERGE_MINING_HEADER + bytes.fromhex(aux_hash_hex)[::-1]
           + (1).to_bytes(4, "little")    # merkle tree size: one aux chain
           + (0).to_bytes(4, "little"))   # merkle nonce
    coinbase = CTransaction()
    coinbase.vin = [CTxIn(COutPoint(0, 0xffffffff), CScript([tag]))]
    coinbase.vout = [CTxOut(0, CScript([OP_TRUE]))]

    parent = CBlockHeader()
    parent.hashMerkleRoot = coinbase.txid_int
    parent.nBits = int(bits_hex, 16)
    target = (parent.nBits & 0xffffff) << (8 * ((parent.nBits >> 24) - 3))
    while parent.hash_int > target:
        parent.nNonce += 1

    return (coinbase.serialize()
            + ser_compact_size(0) + (0).to_bytes(4, "little")   # coinbase merkle branch, index
            + ser_compact_size(0) + (0).to_bytes(4, "little")   # chain merkle branch, index
            + parent.serialize()).hex()


class AuxpowSegwitTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.wallet_names = []

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def merge_mine(self, node, address):
        aux = node.createauxblock(address)
        assert_equal(node.submitauxblock(aux["hash"], create_auxpow(aux["hash"], aux["bits"])), True)
        assert_equal(node.getbestblockhash(), aux["hash"])
        self.sync_blocks()
        block = node.getblock(aux["hash"], 2)
        assert block["version"] & VERSION_AUXPOW_BIT
        return block

    def run_test(self):
        node = self.nodes[0]
        node.createwallet("w")
        wallet = node.get_wallet_rpc("w")
        address = wallet.getnewaddress(address_type="bech32")
        self.generatetoaddress(node, COINBASE_MATURITY + 10, address)

        self.log.info("Merge-mine an empty aux block")
        assert_equal(node.getrawmempool(), [])
        block = self.merge_mine(node, address)
        assert_equal(block["nTx"], 1)

        self.log.info("Merge-mine an aux block carrying SegWit transactions")
        txids = [wallet.sendtoaddress(wallet.getnewaddress(address_type=t), 1)
                 for t in ("bech32", "bech32", "bech32m", "legacy")]
        assert_equal(sorted(node.getrawmempool()), sorted(txids))
        block = self.merge_mine(node, address)
        assert_equal(sorted(tx["txid"] for tx in block["tx"][1:]), sorted(txids))
        # Every tx spends a P2WPKH coinbase output, so all carry witness data.
        assert all(tx["hash"] != tx["txid"] for tx in block["tx"][1:])
        coinbase = block["tx"][0]
        assert any(out["scriptPubKey"]["hex"].startswith(WITNESS_COMMITMENT_PREFIX) for out in coinbase["vout"])
        assert_equal(coinbase["vin"][0]["txinwitness"], ["00" * 32])
        assert_equal(node.getrawmempool(), [])
        for txid in txids:
            assert_greater_than(wallet.gettransaction(txid)["confirmations"], 0)
        assert_equal(self.nodes[1].getbestblockhash(), block["hash"])


if __name__ == '__main__':
    AuxpowSegwitTest(__file__).main()
