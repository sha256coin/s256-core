SHA256Coin Core 3.0.0 Release Notes
===================================

SHA256Coin Core 3.0.0 is a **mandatory upgrade**. Every mainnet node and every merge-mining setup must run 3.0.0 **before mainnet block 17,500**.

3.0.0 is based on Bitcoin Core 31.1, the same base as 2.2.0. These notes list what changed since SHA256Coin Core 2.2.0.

Mandatory upgrade before block 17,500
-------------------------------------

Merged mining (AuxPoW) starts at mainnet block 17,500, as before. 3.0.0 changes how merge-mined blocks are validated. Nodes older than 3.0.0 will reject merge-mined blocks made the new way, and 3.0.0 nodes will reject blocks made the old way. A node that is not upgraded will split off the network at the first merge-mined block.

What to do:

- **Node operators:** install 3.0.0 before block 17,500. No reindex is needed. Mainnet data directories, wallets and configuration files carry over unchanged.
- **Pool and merge-mining operators:** update your merge-mining proxy as described in "Pool integration notes" below. Change the proxy at the same time as the node it talks to, never before.
- **Testnet3 operators:** testnet3 is being restarted (see "Testnet3 reset").

Testing
-------

Merged mining in 3.0.0 was verified on regtest, by the functional test suite, and on a reset testnet3 with two 3.0.0 nodes and a reference merge-mining proxy script. The testnet3 run covered:

- merge-mined blocks containing SegWit transactions;
- merged-mining trees with several chains;
- rejection of the old tag byte order;
- a second node syncing from scratch;
- restarts while holding merge-mined blocks;
- `getblocktemplate` blocks mixed in.

Real pool software has not been tested against 3.0.0 yet. Pool operators should check their setup on testnet3 before mainnet block 17,500 (see "Pool integration notes").

Consensus changes
-----------------

These apply to merge-mined blocks only, so on mainnet they take effect from block 17,500. Blocks that are not merge-mined are unaffected and stay valid at every height.

- **The auxpow must prove the merge-mining tag is in the parent block's coinbase.** Before, the tagged transaction could be any transaction in the parent block. That let one parent block's work be reused for SHA256Coin blocks it never intended to mine. The auxpow's coinbase merkle index must now be 0 (rejection reason `auxpow-coinbase-not-first`).
- **Byte order of the block hash in the merge-mining tag** now matches Namecoin and Dogecoin. The hash returned by `createauxblock` goes into the tag as it decodes, with no byte reversal. Older SHA256Coin builds expected it reversed. See "Pool integration notes".
- **`MAX_MONEY` raised from 21,000,000 to 84,000,000 coins.** SHA256Coin's issuance (100 coins per block, halving every 420,000 blocks) approaches 84 million, and total supply passes 21 million at block 210,000. Without this change, transactions and wallet balances above 21 million would become invalid from then on. It has no effect before that height, so no activation logic is needed.

Pool integration notes
----------------------

A merge-mining proxy must match two changes. Blocks built the old way will be rejected by 3.0.0 nodes.

**1. Byte order of the hash in the coinbase tag (the main change)**

Put the `hash` returned by `createauxblock` into the merge-mining tag **as the hex string decodes, with no byte reversal**. Namecoin and Dogecoin do it the same way.

Tag layout in the parent coinbase scriptSig (single aux chain):

```
fabe6d6d | unhex(hash) (32 bytes, as is) | 01000000 (tree size 1, LE) | nonce (4 bytes, LE)
```

Example for `hash = 000000000000003f2a9c1e5b7d4f60718293a4b5c6d7e8f90a1b2c3d4e5f6071`:

```
new (required): fabe6d6d000000000000003f2a9c1e5b7d4f60718293a4b5c6d7e8f90a1b2c3d4e5f60710100000000000000
old (rejected): fabe6d6d71605f4e3d2c1b0af9e8d7c6b5a4938271604f7d5b1e9c2a3f000000000000000100000000000000
```

Merging SHA256Coin with other chains in one tag now works as for Namecoin and Dogecoin:

- Build the standard merged-mining tree and put its root in the tag the same way you do for those chains.
- SHA256Coin's slot in the tree comes from the standard expected-index formula, using the tree's merkle nonce and SHA256Coin's chain ID: `598` (`0x0256`). `createauxblock` returns it as `chainid`.
- If your proxy reverses the hash for SHA256Coin only, remove that special case. Treat SHA256Coin exactly like Namecoin.

**2. Coinbase merkle index must be 0**

The auxpow sent to `submitauxblock` must prove the tag is in the parent block's coinbase, so its coinbase merkle branch index must be 0. A proxy that builds the branch for the coinbase already sends 0.

**Unchanged:** how `createauxblock` and `submitauxblock` are used, the auxpow serialization, and the tree size and nonce fields.

**Timing:** switch the proxy at the same time as the node upgrade. Old nodes reject the new tag order and new nodes reject the old one. Do not switch the proxy until the node it talks to runs 3.0.0, because an old node rejects every block built the new way.

- **Mainnet:** the first merge-mined block is at 17,500, so use the new order from the start.
- **Testnet3:** switch when you restart on the reset chain.

**How to check:** on testnet or regtest, `submitauxblock` should return `true`. If it returns `false` and the node's `debug.log` shows `auxpow-chain-merkle-mismatch`, the hash bytes in the tag are in the wrong order.

**Other changes relevant to pools:**

- `createauxblock` keeps one current template per payout script. It makes a new template only when the tip changes, or when the mempool has changed and 60 seconds have passed (as Namecoin does). Polling it no longer creates a new block each time. A template stays valid after a submission until the tip changes.
- `createauxblock` reserves block weight for the auxpow attached at submission. The new option `-auxpowreservedweight=<n>` sets the amount on top of `-blockreservedweight`. The default is 40,000 weight units, room for about a 10 KB parent coinbase, for example a pool that pays out in the coinbase. Raise it if your parent coinbase is larger.
- `createauxblock` returns `_target` (the target in little-endian byte order) next to `target`.
- On mainnet, `createauxblock` now refuses to hand out templates while the node has no peers or is in initial block download, as `getblocktemplate` does. Test networks skip these checks.

Initial block download and template RPCs
----------------------------------------

- **The default `-maxtipage` is now 7 days (Bitcoin Core's is 24 hours).** Gaps of several hours between blocks happen on SHA256Coin. With a 24-hour limit, a pool node restarted after such a gap counted as being in initial block download, and `getblocktemplate` refused to work. A node restarted with a tip up to 7 days old now serves templates straight away.
- **Removed a startup shortcut that took a node out of initial block download whenever its tip was the genesis block.** Fresh nodes now stay in initial block download until they have synced. The minimum chain work requirement keeps them there until they have the real chain.
- To use Bitcoin Core's 24-hour limit instead, set `-maxtipage=86400`.

P2P and protocol
----------------

- **Protocol version 70101 (3.0.1).** Marks the Namecoin auxpow format (see "Namecoin auxpow format"). 3.0.0 nodes (70100) still connect, but can't decode merge-mined blocks in the new format; there is no disconnect by height.
- **Protocol version 70100.** Merge-mined headers carry the parent block's coinbase and merkle branches, so their size varies. From version 70100, `headers` messages are also limited by size. A reply stops at the header that reaches 2,000,000 bytes, and the receiving peer then asks for more. A header announcement that would exceed the limit falls back to an `inv`. Older peers are still served (oversized replies are cut for them too) and still connect.
- **The minimum peer protocol version is unchanged in 3.0.0**, so 2.x nodes can still connect.
- **Headers sync** keeps each header's auxpow through the low-work headers presync, and its memory parameters are retuned for merge-mined headers (about 1.6 MiB per syncing peer).

Fixes
-----

- **Crashes with merge-mined blocks.** A node holding merge-mined blocks could crash when serving their headers: answering `getheaders` from a syncing peer, announcing headers, `getblockheader <hash> false`, or REST `/headers`. Merge-mined headers are now read from disk with their auxpow. Restarting a node holding such blocks also failed, because the proof of work was checked against the block's own hash instead of its auxpow. Both are fixed.
- **No permanent "Unknown new rules activated (versionbit 8)" warning.** Bit 8 marks a merge-mined block and is no longer treated as an unknown soft fork signal.
- **The auxpow check verifies the parent header's proof of work first,** so invalid auxpows are rejected cheaply.
- **`createauxblock` no longer hands out a hash over a null merkle root.**

RPC
---

- `getblock` (verbosity 1 and above), verbose `getblockheader` and the REST block JSON include an `auxpow` object for merge-mined blocks: the parent coinbase, the merkle branches, the chain index and the parent header. The format follows Namecoin's.
- `getblockheader` and REST `/headers` return an error for a merge-mined block whose data is not on disk (pruned or not yet downloaded), because the auxpow is only stored with the block.

Networks
--------

### Testnet3 reset

Testnet3 now activates BIP34, BIP65, BIP66, CSV and SegWit from block 1, as testnet4 does. It previously used Bitcoin testnet3's activation heights, which SHA256Coin's testnet never reaches, and that blocked merge-mined testnet blocks. This is a consensus change for testnet3, so the testnet3 chain is being restarted.

- Upgrade, then delete `blocks/`, `chainstate/`, `indexes/` and `mempool.dat` under `testnet3/` in your data directory, and start the node. Wallets in `testnet3/wallets/` can be kept.
- Miners using `getblocktemplate`: BIP34 and SegWit are active from block 1, so blocks must put the block height in the coinbase scriptSig and include the witness commitment (the template's `default_witness_commitment`) from block 1.
- Pools: switch the merge-mining proxy to the new tag order when you restart on the reset chain.

### Testnet4

Testnet4 no longer carries Bitcoin testnet4's minimum chain work, assumevalid block or assumeutxo snapshots. Fresh testnet4 nodes previously could never leave initial block download.

### Signet

- There is **no default signet** any more. `-signet` without `-signetchallenge` now exits with:

  ```
  Error: -signet requires -signetchallenge: SHA256Coin has no default signet.
  ```

  The previous default was Bitcoin's signet: its challenge, seeds and chain data.
- Custom signets (`-signet -signetchallenge=<script>`) work. Before, no signet node could start, because its genesis block was invalid. The signet genesis block is now valid and checked at startup, like every other network's.

Changes since 3.0.0rc1
----------------------

- `-version` and `-help` print the shipped program names: `sha256coind`, `sha256coin-wallet`, `sha256coin-tx` and `sha256coin-util` instead of `bitcoind`, `bitcoin-wallet`, `bitcoin-tx` and `bitcoin-util`.
- User-visible text no longer refers to Bitcoin:
  - `-version` and the GUI's About dialog point to the SHA256Coin source code;
  - crash and error messages point to the SHA256Coin issue tracker;
  - RPC help says "SHA256Coin address";
  - wallet tool messages and the Windows file properties use SHA256Coin names.
- **RPC error text changed:** an invalid address now returns `Invalid SHA256Coin address` (was `Invalid Bitcoin address`). The error code (-5) is unchanged. Update scripts that match the message text.
- The unit test suite runs in full in the release builds, with no exclusions.
- `SECURITY.md` describes how to report vulnerabilities: GitHub private vulnerability reporting, or security@sha256coin.eu.
- No consensus, P2P or RPC changes.

Notes for the next release
--------------------------

- **The next release after block 17,500 will raise the minimum peer protocol version to 70101.** Nodes older than 3.0.1 will then no longer be able to connect. 3.0.1 keeps accepting them so the network can upgrade before the fork.

Credits
-------

Thanks to everyone who contributed to this release, and to the Bitcoin Core developers for the code SHA256Coin Core is based on. Release notes for Bitcoin Core 31.1 are in `doc/release-notes/release-notes-31.1.md`.
