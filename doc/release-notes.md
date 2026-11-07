SHA256Coin Core 3.0.2 Release Notes
===================================

SHA256Coin Core 3.0.2 is a **recommended upgrade**, and **strongly recommended for merge-mining pools and testnet operators**. It has no fork and no activation height: nodes on 3.0.1 keep following the same chain. It does not change the consensus rules for the current chain. The only new validity rule is the checkpoint at block 18,000, which the current chain already satisfies.

3.0.2 is based on Bitcoin Core 31.1, like 3.0.0 and 3.0.1. These notes list what changed since SHA256Coin Core 3.0.1. If you are upgrading from an older version, everything in the [3.0.1 release notes](release-notes/release-notes-sha256coin-3.0.1.md) applies too, and 3.0.1 was mandatory before block 17,500.

What to do
----------

- **Node operators:** upgrade at your convenience. No reindex is needed, and data directories, wallets and configuration files carry over unchanged. Nodes older than 3.0.1 stop at block 17,499 and are now disconnected by 3.0.2 nodes (see "P2P and protocol").
- **Merge-mining pools:** upgrade the node that serves `createauxblock`, so the blocks you find carry current timestamps (see "Merge mining"). If your pool connects to the node over P2P for block notifications, it must announce protocol version 70101 or higher, or the node will disconnect it and the pool will fall back to polling.
- **Testnet and regtest operators:** upgrade to avoid the `createauxblock` hang described below.

Merge mining
------------

- **Block timestamps stay current.** `createauxblock` used to keep handing out the same template while the mempool was unchanged. The block time is set when the template is built, so a block found hours later carried the time of the first poll, and with idle stretches between blocks, timestamps could lag real time by up to about two hours. The difficulty adjustment (LWMA) then reacted one block late. `createauxblock` now rebuilds its template once it is a minute old, whether or not the mempool changed. A node only helps once the pool uses it: pools still on 3.0.1 keep producing old timestamps.

  Example from mainnet: block 18,150 is stamped 14:58:16 UTC, 22 seconds after block 18,149 (14:57:54). The nodes received it at 16:31:28 UTC, 93 minutes later. Each block carried about the arrival time of the previous one (block 18,149 had arrived at 14:58:13), so the 93-minute wait was recorded as a 22-second block, and the difficulty adjustment (LWMA) saw a very fast block where it should have seen a very slow one.
- **Older templates stay valid for 10 minutes.** A pool's parent job can hold an earlier template's hash for longer than a minute. `submitauxblock` keeps accepting a template that was replaced for 10 minutes, and the newest template of each address for as long as the tip does not change.
- **`getblocktemplate` reserves room for the auxpow by default.** The auxpow is attached after the template is built and counts towards the block weight. `createauxblock` already reserved `-auxpowreservedweight` (40,000 weight units) on top of `-blockreservedweight`; `getblocktemplate` did not, so a pool that built its block from `getblocktemplate` with a nearly full block and a large parent coinbase could produce a block over the 4,000,000 limit, rejected as `bad-blk-length`. It now reserves the same 40,000. Merge-mined blocks stay optional at every height, so this is only a safety margin: a pool that never merge-mines gives up 1% of the block space, which matters only when blocks are full. **To opt out, start the node with `-auxpowreservedweight=0`.** This also replaces the earlier workaround `-blockreservedweight=48000` on pool nodes.
- **`createauxblock` no longer hangs the RPC server on a test chain in initial block download.** On regtest, testnet or signet, a node whose tip was older than 7 days counts as in initial block download. `createauxblock` waited for that to end while holding its lock, every other call queued behind it, all RPC workers ended up blocked, and nothing could submit the block that would end the wait. It now returns a template, as `getblocktemplate` does. Mainnet was not affected: `createauxblock` refuses to run there during initial block download.

P2P and protocol
----------------

- **The minimum peer protocol version is now 70101.** Peers that announce a lower version are disconnected, and the node logs `peer using obsolete version N`. 3.0.0 and older nodes cannot decode merge-mined blocks in the current format and stop at block 17,499, so they are of no use as peers. The protocol version itself stays 70101.
- **Pools that use P2P block notification must send protocol version 70101 or higher.** Some pool software hard-codes an old version in its handshake. After the upgrade the node disconnects such a pool, which then falls back to polling: slower detection of new blocks and more stale work.

Chain parameters
----------------

- **Checkpoint at block 18,000.** A header at height 18,000 must be block `d465034b116df75304b9aea548d0706110b13ed7c7abe87f746abb35e0726416`; a header with another hash there is rejected (`checkpoint-mismatch`). Any chain that forks before the checkpoint has a different block there, so it is never followed, even a longer one. The current chain already satisfies this. Block 18,000 is 500 blocks after the fork at 17,500.
- **`defaultAssumeValid` and `nMinimumChainWork` now point at the same block.** They were block 11,277, before the fork. During initial sync a node now skips the signature checks of blocks up to 18,000 and requires a peer chain with at least that much work. Proof of work, merge-mining proofs and every other consensus rule are still checked for all of those blocks; only the signature checks are skipped.
- Testnet3, testnet4, signet and regtest are unchanged.

Other changes
-------------

- **Release downloads now include `SHA256SUMS.txt`**, for checking the files you download (`sha256sum -c --ignore-missing SHA256SUMS.txt`).
- **Copyright line.** `-version` and the About dialog now say `Copyright (C) 2025-2026 The SHA256Coin developers`. SHA256Coin started in 2025. The Bitcoin Core line, `Copyright (C) 2009-2026 The Bitcoin Core developers`, is unchanged.

Testing
-------

- **Real pool software.** On a local test pool (sha256-nomp with node-stratum-pool, merge-mining S256 on a regtest parent chain), the pool was left idle for six minutes and then found one block. The block time of that block was **400 seconds behind** its arrival on 3.0.1, and **41 seconds behind** on 3.0.2.
- **Functional tests.** New tests cover the template refresh and retention, a nearly full block with a large auxpow through both `getblocktemplate` and `createauxblock`, the `createauxblock` hang, the minimum protocol version, the checkpoint and the copyright lines. Each fails on 3.0.1. The release workflow now runs the AuxPoW functional tests, and a failure blocks publishing.

Platforms
---------

No platform changes since 3.0.1. For reference, from the 3.0.1 notes:

- **macOS on Apple Silicon now needs macOS 15 or later** (3.0.0 needed 14). The release build moved to GitHub's macOS 15 runner because the macOS 14 runner is being retired. The Intel macOS build already needed macOS 15.

Credits
-------

Release notes for 3.0.1 are in `doc/release-notes/release-notes-sha256coin-3.0.1.md`, for 3.0.0 in `doc/release-notes/release-notes-sha256coin-3.0.0.md`, and for Bitcoin Core 31.1 in `doc/release-notes/release-notes-31.1.md`.
