#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that templates leave room for a large auxpow, for both mining paths.

A merge-mining pool attaches the auxpow after the template is built, and it
counts towards the block weight. With a nearly full block and a big parent
coinbase (a pool paying its miners in the coinbase), the block must still
fit under the consensus limit, whether the pool built it from
getblocktemplate or from createauxblock/submitauxblock:

- by default both reserve -blockreservedweight + -auxpowreservedweight
- with -auxpowreservedweight=0 neither does, and the same block is too heavy
  (the control: it shows the reserve is what makes the block fit)
"""

import io
from decimal import Decimal

from test_framework.auxpow import create_large_auxpow
from test_framework.blocktools import (
    COINBASE_MATURITY,
    add_witness_commitment,
    create_block,
)
from test_framework.messages import (
    CAuxPow,
    VERSION_AUXPOW_BIT,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
)
from test_framework.wallet import MiniWallet

MAX_BLOCK_WEIGHT = 4_000_000
DEFAULT_BLOCK_RESERVED_WEIGHT = 8000
DEFAULT_AUXPOW_RESERVED_WEIGHT = 40000
BIG_TX_VSIZE = 99_000        # standard maximum is 100,000 vbytes
SMALL_TX_VSIZE = 2_000
# 10 * 396,000 + 70 * 8,000 WU is more than a block. The big transactions pay
# a higher feerate, so the assembler takes 9 of them first and then small ones
# into the gap, which ends under 8,000 WU for both limits used here
# (3,952,000 and 3,992,000 WU).
NUM_BIG, NUM_SMALL = 10, 70
# Why the node rejects a block over the limit here: CheckBlock's size check on
# the non-witness part (x4) fires before ContextualCheckBlock's weight check
# (bad-blk-weight), as these transactions are almost all non-witness data.
REJECT_REASON = "bad-blk-length"
SLACK = 20_000               # how far below its limit a template may stop
PAYOUT_OUTPUTS = 290         # a parent coinbase of about 9,000 bytes, 36,000 WU


class GbtAuxpowWeightTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True
        self.extra_args = [["-limitancestorcount=100", "-limitdescendantcount=100"]]

    def fill_mempool(self):
        """Start from an empty mempool, then add more than a block's worth of
        independent transactions: the big ones fill most of the block and the
        small ones the gap that is left, so every template is cut off by the
        weight limit and not by what is available."""
        while self.node.getrawmempool():
            self.generate(self.wallet, 1)
        for _ in range(NUM_BIG):
            self.wallet.send_self_transfer(from_node=self.node, target_vsize=BIG_TX_VSIZE, fee_rate=Decimal('0.004'), confirmed_only=True)
        for _ in range(NUM_SMALL):
            self.wallet.send_self_transfer(from_node=self.node, target_vsize=SMALL_TX_VSIZE, fee_rate=Decimal('0.003'), confirmed_only=True)
        assert_greater_than(sum(e["weight"] for e in self.node.getrawmempool(verbose=True).values()), MAX_BLOCK_WEIGHT)

    def check_fill(self, weight_without_auxpow, reserved):
        """The template was cut off by the weight limit, and by nothing else:
        it stops within SLACK of the limit minus the weight it reserved (the
        reserve covers the coinbase, which the template then counts for real)."""
        limit = MAX_BLOCK_WEIGHT - reserved
        assert_greater_than_or_equal(weight_without_auxpow, limit - SLACK)
        assert_greater_than_or_equal(limit + DEFAULT_BLOCK_RESERVED_WEIGHT, weight_without_auxpow)

    def gbt_block(self):
        """The block a pool would build from getblocktemplate, with the auxpow
        bit set but no auxpow yet."""
        return self.block_from_template(self.node.getblocktemplate({"rules": ["segwit"]}))

    def block_from_template(self, tmpl):
        block = create_block(tmpl=tmpl, txlist=[t["data"] for t in tmpl["transactions"]])
        block.vtx[0].vout[0].nValue = tmpl["coinbasevalue"]
        block.nVersion |= VERSION_AUXPOW_BIT
        add_witness_commitment(block)
        block.hashMerkleRoot = block.calc_merkle_root()
        return block

    def attach_auxpow(self, block):
        raw = create_large_auxpow(block.hash_hex, f"{block.nBits:08x}", PAYOUT_OUTPUTS)
        block.auxpow = CAuxPow()
        block.auxpow.deserialize(io.BytesIO(bytes.fromhex(raw)))
        return raw

    def run_test(self):
        self.node = node = self.nodes[0]
        self.wallet = MiniWallet(node)
        self.generate(self.wallet, COINBASE_MATURITY + 450)
        addr = self.wallet.get_address()

        self.log.info("getblocktemplate, default reserve: a nearly full block with a 9 KB parent coinbase fits")
        self.fill_mempool()
        block = self.gbt_block()
        raw = self.attach_auxpow(block)
        auxpow_weight = 4 * len(bytes.fromhex(raw))
        assert_greater_than(auxpow_weight, DEFAULT_BLOCK_RESERVED_WEIGHT)  # more than the old reserve alone
        assert_equal(node.submitblock(block.serialize().hex()), None)
        assert_equal(node.getbestblockhash(), block.hash_hex)
        self.check_fill(block.get_weight() - auxpow_weight, DEFAULT_BLOCK_RESERVED_WEIGHT + DEFAULT_AUXPOW_RESERVED_WEIGHT)
        assert_greater_than_or_equal(MAX_BLOCK_WEIGHT, block.get_weight())

        self.log.info("createauxblock, default reserve: the same fill and auxpow are accepted by submitauxblock")
        self.fill_mempool()
        aux = node.createauxblock(addr)
        raw = create_large_auxpow(aux["hash"], aux["bits"], PAYOUT_OUTPUTS)
        assert node.submitauxblock(aux["hash"], raw)
        accepted = node.getblock(aux["hash"])
        self.check_fill(accepted["weight"] - 4 * len(bytes.fromhex(raw)), DEFAULT_BLOCK_RESERVED_WEIGHT + DEFAULT_AUXPOW_RESERVED_WEIGHT)
        assert_greater_than_or_equal(MAX_BLOCK_WEIGHT, accepted["weight"])

        self.log.info("-auxpowreservedweight=0: the same fill leaves no room, the block is too heavy")
        self.restart_node(0, ["-limitancestorcount=100", "-limitdescendantcount=100", "-auxpowreservedweight=0"])
        self.fill_mempool()
        block = self.gbt_block()
        raw = self.attach_auxpow(block)
        self.check_fill(block.get_weight() - 4 * len(bytes.fromhex(raw)), DEFAULT_BLOCK_RESERVED_WEIGHT)
        assert_greater_than(block.get_weight(), MAX_BLOCK_WEIGHT)
        assert_equal(node.submitblock(block.serialize().hex()), REJECT_REASON)
        assert_greater_than(len(node.getrawmempool()), 0)

        aux = node.createauxblock(addr)
        raw = create_large_auxpow(aux["hash"], aux["bits"], PAYOUT_OUTPUTS)
        with node.assert_debug_log([REJECT_REASON]):
            assert_equal(node.submitauxblock(aux["hash"], raw), False)


if __name__ == '__main__':
    GbtAuxpowWeightTest(__file__).main()
