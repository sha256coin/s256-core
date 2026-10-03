# Merge-mining proxy changes in SHA256Coin Core 3.0.0

**Who needs this:** anyone who merge-mines SHA256Coin (S256), meaning a pool or merge-mining proxy that calls `createauxblock` / `submitauxblock`, and the operators of the S256 nodes those proxies talk to. Solo miners and pools that mine S256 directly with `getblocktemplate` don't need to change anything.

**Summary:** SHA256Coin Core 3.0.0 is mandatory before mainnet block 17,500, where merged mining (AuxPoW) starts. It changes two things a merge-mining proxy must match: the byte order of the hash in the coinbase tag, and the coinbase merkle index. Blocks built the old way are rejected by 3.0.0 nodes, and blocks built the new way are rejected by older nodes.

## 1. Byte order of the hash in the coinbase tag (the main change)

Put the `hash` returned by `createauxblock` into the merge-mining tag **as the hex string decodes, with no byte reversal**. Namecoin and Dogecoin do it the same way. S256 builds before 3.0.0 expected the hash reversed.

Tag layout in the parent coinbase scriptSig (single aux chain):

```
fabe6d6d | unhex(hash) (32 bytes, as is) | 01000000 (tree size 1, LE) | nonce (4 bytes, LE)
```

Example for `hash = 000000000000003f2a9c1e5b7d4f60718293a4b5c6d7e8f90a1b2c3d4e5f6071` and merkle nonce 0:

```
new (required): fabe6d6d000000000000003f2a9c1e5b7d4f60718293a4b5c6d7e8f90a1b2c3d4e5f60710100000000000000
old (rejected): fabe6d6d71605f4e3d2c1b0af9e8d7c6b5a4938271604f7d5b1e9c2a3f000000000000000100000000000000
```

### Merging S256 with other chains in one tag

S256 now works like Namecoin and Dogecoin:

- Build the standard merged-mining tree and put its root in the tag the same way you do for those chains.
- S256's slot in the tree comes from the standard expected-index formula, using the tree's merkle nonce and S256's chain ID: `1395799350` (`0x53323536`). `createauxblock` returns it as `chainid`.
- If your proxy reverses the hash for S256 only, remove that special case. Treat S256 exactly like Namecoin.

## 2. Coinbase merkle index must be 0

The auxpow sent to `submitauxblock` must prove the tag is in the parent block's **coinbase**, so its coinbase merkle branch index must be 0. A proxy that builds the branch for the coinbase already sends 0; anything else is rejected (`auxpow-coinbase-not-first`).

## Unchanged

How `createauxblock` and `submitauxblock` are used, the auxpow serialization, and the tree size and nonce fields.

## Timing

Switch the proxy **at the same time as the node upgrade**. Don't switch it until the S256 node it talks to runs 3.0.0: an old node rejects every block built the new way.

- **Mainnet:** the first merge-mined block is at 17,500, so use the new order from the start. Upgrade the node to 3.0.0 before 17,500.
- **Testnet3:** the testnet3 chain is being restarted for 3.0.0. Switch when you restart on the reset chain: upgrade, delete `testnet3/blocks/` and `testnet3/chainstate/` (wallets can be kept), and start the node.

## How to check

On testnet or regtest, `submitauxblock` should return `true`. If it returns `false` and the node's `debug.log` shows `auxpow-chain-merkle-mismatch`, the hash bytes in the tag are in the wrong order.

Other rejection reasons in `debug.log`:

| Reason | Meaning |
|---|---|
| `auxpow-coinbase-not-first` | Coinbase merkle index is not 0 |
| `auxpow-wrong-index` | S256's hash is not at its expected slot in the merged-mining tree (chain ID or merkle nonce mismatch) |
| `auxpow-no-merge-mining-tag` / `auxpow-multiple-merge-mining-tags` | The `fabe6d6d` tag is missing, or appears more than once, in the parent coinbase |

## Other changes in 3.0.0 relevant to pools

- **Template reuse.** `createauxblock` keeps one current template per payout address. It makes a new one only when the tip changes, or when the mempool has changed and 60 seconds have passed (as Namecoin does). Polling it no longer creates a new block each time.
- **Room for the auxpow.** `createauxblock` reserves block weight for the auxpow attached at submission. `-auxpowreservedweight=<n>` sets the amount on top of `-blockreservedweight`. The default is 40,000 weight units, enough for about a 10 KB parent coinbase, for example a pool that pays out in the coinbase. Raise it if your parent coinbase is larger.
- **`_target`.** `createauxblock` also returns the target in little-endian byte order, as Namecoin's does.
- **No templates during sync on mainnet.** Like `getblocktemplate`, `createauxblock` refuses to work while the node has no peers or is in initial block download. A node restarted with a tip up to 7 days old still serves templates straight away (the default `-maxtipage` is now 7 days).

## RPC reference

`createauxblock <address>` returns:

| Field | Meaning |
|---|---|
| `hash` | Hash of the new S256 block: put it in the tag as is (see above) |
| `chainid` | S256's merge-mining chain ID, `1395799350` |
| `previousblockhash` | The S256 tip the template builds on |
| `coinbasevalue` | Block reward in satoshis |
| `bits`, `target`, `_target` | Target of the next S256 block (compact, big-endian, little-endian) |
| `height` | Height of the next S256 block |

`submitauxblock <hash> <auxpow>` takes the `hash` from `createauxblock` and the serialized auxpow: the parent coinbase, its merkle branch (index 0), the merged-mining tree branch with S256's index, and the parent block header. It returns `true` if the block was accepted.

Full release notes: [doc/release-notes.md](doc/release-notes.md).
