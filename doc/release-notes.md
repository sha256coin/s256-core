SHA256Coin Core 3.0.1 Release Notes
===================================

SHA256Coin Core 3.0.1 is a **mandatory upgrade**. Every mainnet node must run 3.0.1, and every merge-mining proxy must be updated, **before mainnet block 17,500**.

**If you already upgraded to 3.0.0, you must upgrade again to 3.0.1 before block 17,500.**

3.0.1 is based on Bitcoin Core 31.1, like 3.0.0. These notes list what changed since SHA256Coin Core 3.0.0. If you are upgrading from 2.2.0, everything in the [3.0.0 release notes](release-notes/release-notes-sha256coin-3.0.0.md) applies too.

Why another mandatory upgrade
-----------------------------

The way 3.0.0 serializes merge-mining proofs (auxpows) differs from Namecoin's: it leaves out one 32-byte field. Pool software written for Namecoin-style merged mining therefore cannot submit blocks to 3.0.0. Its submissions fail with `Block decode failed` or `auxpow decode failed`. 3.0.1 uses Namecoin's format exactly, and gives SHA256Coin a 16-bit merge-mining chain ID.

Both are consensus changes for merge-mined blocks. 3.0.0 nodes cannot decode merge-mined blocks in the 3.0.1 format, so a 3.0.0 node will split off the network at the first merge-mined block. Merged mining starts at mainnet block 17,500, as before. No merge-mined blocks exist on mainnet yet, so no existing mainnet data is affected.

What to do:

- **Node operators:** install 3.0.1 before block 17,500. No reindex is needed, whether you are coming from 3.0.0 or from 2.2.0. Mainnet data directories, wallets and configuration files carry over unchanged.
- **Exchanges:** upgrade your wallet nodes to 3.0.1 before block 17,500. No reindex is needed.
- **Pool and merge-mining operators:** see "Pool integration notes" below. Pool software that follows Namecoin's format works without SHA256Coin-specific changes. Switch the proxy at the same time as the node it talks to.
- **Testnet3 operators:** testnet3 is being restarted again (see "Testnet3 reset").

Consensus changes
-----------------

These apply to merge-mined blocks only, so on mainnet they take effect from block 17,500. Blocks that are not merge-mined are unaffected.

- **Namecoin's auxpow format.** The auxpow now carries a 32-byte `hashBlock` field between the parent coinbase transaction and the coinbase merkle branch, as in Namecoin. The field is not used: any value is accepted, and nodes store and relay it as zero. Pools commonly put the parent block hash there.
- **Merge-mining chain ID 598 (`0x0256`)** on all networks. It was `1395799350` (`0x53323536`) in 3.0.0. A 16-bit ID, like Namecoin's (1) and Dogecoin's (98), works with pool software that stores the chain ID in 16 bits. SHA256Coin does not encode the chain ID in the block version; pools must take it from `createauxblock`'s `chainid` or configure 598. 598 was checked against the chain IDs of other merge-mined chains, including ESF (1175), Namecoin and Dogecoin, and collides with none. The chain ID decides SHA256Coin's slot in a merged-mining tree with several chains. `createauxblock` returns it as `chainid`.

Pool integration notes
----------------------

**1. Auxpow format: Namecoin's, byte for byte**

```
parent coinbase tx | hashBlock (32 bytes, any value) | coinbase merkle branch | coinbase index (int32, must be 0)
  | chain merkle branch | chain index (int32) | parent block header (80 bytes)
```

- **Pool software built for Namecoin's `createauxblock` / `submitauxblock`** needs no SHA256Coin-specific changes.
- **A proxy adapted to 3.0.0's format** (no `hashBlock`) must add the 32-byte field after the parent coinbase. Zero is fine.

**2. Chain ID 598**

If your proxy has the chain ID configured, change it from `1395799350` to `598`, or take it from `createauxblock`'s `chainid`. With a single aux chain the chain ID doesn't matter (the slot is always 0). In a tree of 64 or more leaves, the old ID puts SHA256Coin in the wrong slot, which is rejected with `auxpow-wrong-index`.

**3. Unchanged since 3.0.0**

- The tag byte order: the `createauxblock` hash goes into the tag as it decodes, with no byte reversal, as in Namecoin.
- The coinbase merkle index must be 0.
- How `createauxblock` and `submitauxblock` are called.

`POOL_PROXY_NOTE.md` has the details.

**Timing:** switch the proxy at the same time as the node upgrade. A 3.0.0 node cannot decode auxpows in the 3.0.1 format, and a 3.0.1 node cannot decode 3.0.0's.

**How to check:** on testnet or regtest, `submitauxblock` should return `true`.

- `auxpow decode failed` means the auxpow is not in Namecoin's format. Check for the `hashBlock` field.
- `auxpow-wrong-index` in the node's `debug.log` means a wrong chain ID or merkle nonce in a tree with several chains.

RPC
---

- **`submitauxblock` rejects an auxpow followed by extra bytes** with `auxpow decode failed: leftover bytes after the auxpow` (error code -22). Before, such an auxpow was misparsed and `submitauxblock` returned `false`, which hid format mismatches.
- `createauxblock` returns `chainid` 598.

P2P and protocol
----------------

- **Protocol version 70101.** It marks the Namecoin auxpow format. 3.0.0 nodes (70100) still connect, but cannot decode merge-mined blocks in the new format. There is no disconnect by height.
- The minimum peer protocol version is unchanged, so 2.x and 3.0.0 nodes can still connect.

Networks
--------

### Testnet3 reset

The auxpow format and chain ID changes apply from block 1 on testnet3, so merge-mined testnet3 blocks made with 3.0.0 cannot be read by 3.0.1. The testnet3 chain is being restarted.

- Upgrade, then delete `blocks/`, `chainstate/`, `indexes/` and `mempool.dat` under `testnet3/` in your data directory, and start the node. Wallets in `testnet3/wallets/` can be kept.
- Pools: switch the merge-mining proxy to the 3.0.1 format and chain ID when you restart on the reset chain.

Testnet4, signet and regtest use the new format and chain ID too.

Testing
-------

- **Functional test suite.** A new test builds auxpows with Namecoin's own test helpers, included unmodified. Those auxpows are accepted by `submitauxblock` and `submitblock`. The same test fails against 3.0.0.
- **Chain ID.** The tests check `createauxblock`'s `chainid`. They also check SHA256Coin's slot in a tree of 64 leaves.
- **Real pool software.** Before 3.0.1 final, a pool operator's merge-mining pool (Namecoin-style, unchanged) is to be tested against 3.0.1rc1 on the reset testnet3.

Notes for the next release
--------------------------

- **The next release after block 17,500 will raise the minimum peer protocol version to 70101.** Nodes older than 3.0.1 will then no longer be able to connect. 3.0.1 keeps accepting them so the network can upgrade before the fork.

Credits
-------

Thanks to RodB (@rodb2008), whose merge-mining tests with mkpool found the auxpow format difference, and to the Namecoin developers for the auxpow design and test helpers. Release notes for 3.0.0 are in `doc/release-notes/release-notes-sha256coin-3.0.0.md`; for Bitcoin Core 31.1, in `doc/release-notes/release-notes-31.1.md`.
