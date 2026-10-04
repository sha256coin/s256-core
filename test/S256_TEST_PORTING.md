# S256 test porting status

Tracks porting of the unit (`test_bitcoin`) and functional (`test/functional`)
suites from Bitcoin's assumptions to S256 consensus. Tests are changed to match
S256; node and consensus code are never changed to make a test pass. S256
constants are taken from the source, cited below.

## S256 constants used by tests

| Constant | S256 value | Source |
|---|---|---|
| Initial block subsidy | 100 COIN | `src/validation.cpp`, `GetBlockSubsidy()` |
| Halving interval (main/test/test4/signet) | 420,000 | `src/kernel/chainparams.cpp`, `nSubsidyHalvingInterval` |
| Halving interval (regtest) | 150 | `src/kernel/chainparams.cpp`, `CRegTestParams` |
| Coinbase maturity | 200 | `src/consensus/consensus.h`, `COINBASE_MATURITY` |
| Regtest bech32 HRP (main / test) | `s2rt` (`s2` / `ts2`) | `src/kernel/chainparams.cpp`, `bech32_hrp` |
| Regtest base58 prefixes, magic, port, genesis time/bits | unchanged from Bitcoin | `src/kernel/chainparams.cpp`, `CRegTestParams` |
| Mainnet / testnet4 magic | `f1c2a5d8` / `f4c5a8db` | `src/kernel/chainparams.cpp`, `pchMessageStart` |
| Block spacing | 20 min | `src/kernel/chainparams.cpp`, `nPowTargetSpacing` |
| Binary names | `sha256coind`, `sha256coin-cli`, `-tx`, `-wallet`, `-util` | `src/CMakeLists.txt`, `OUTPUT_NAME` |
| Default datadir / pid file | `~/.sha256coin` / `sha256coind.pid` | `src/common/args.cpp`, `src/init.cpp` |
| `MAX_MONEY` | 84,000,000 COIN | `src/consensus/amount.h` (raised from Bitcoin's 21M, see decision below) |

## Unit tests (`test_bitcoin`)

Baseline before porting (full run with the exclude list): 14,256 failures.
After step 1 (subsidy tests + `MAX_MONEY`): **39 failures**, all in the 9 suites below.
After step 3: those 9 suites pass.

rc1 (2026-10-03): the former exclude list is ported or fixed; only
`miner_tests/CreateNewBlock_validity` was still excluded.
v3.0.0 (2026-10-04): `miner_tests` ported too. No unit tests are excluded.

| Suite / case | Status | Notes |
|---|---|---|
| `net_tests/initial_advertise_from_version_message`, `advertise_local_address` | Passing | Fixed by removing the genesis IBD shortcut (node change, reported and approved) |
| `net_peer_connection_tests` | Ported | |
| `txpackage_tests` | Ported | Amounts relative to `COINBASE_VALUE` (100 COIN) instead of 50-coin literals |
| `txvalidationcache_tests/checkinputs_test` | Ported | `-testactivationheight=dersig@202` (fixture chain is 200 blocks) |
| `validation_chainstate_tests/chainstate_update_tip` | Passing | Needs the S256 regtest assumeutxo entry at 210 |
| `validation_chainstatemanager_tests` | Ported | Heights +100 (snapshot at 210, snapshot chain 310/320), coin count `COINBASE_MATURITY` |
| `miner_tests/CreateNewBlock_validity` | Ported (v3.0.0) | Bitcoin's 110 hard-coded nonces fail (`high-hash`), and 110 blocks are too few at maturity 200 (inputs from blocks 1-4 are spent at 111). Now 210 blocks with nonces ground for S256 mainnet difficulty 1 (extranonce bumped where no nonce existed). No node change. |

| Suite / case | Status | Notes |
|---|---|---|
| `validation_tests/block_subsidy_test` | Ported | Initial subsidy 50 -> 100 COIN |
| `validation_tests/subsidy_limit_test` | Ported | Cap 50 -> 100 COIN; range 14M -> 28M blocks (doubled halving interval); expected total 8,399,999,995,380,000 sat |
| `transaction_tests/tx_invalid` | Ported | `MAX_MONEY + 1` vectors re-encoded at 84M (failed after the `MAX_MONEY` change) |
| `spend_tests` (10) | Ported | Coinbase input 50 -> 100 COIN; preset-inputs scenario amounts doubled (400 wallet / 300 preset / 598 target) |
| `script_standard_tests` (8) | Ported | Taproot builder literal re-encoded with HRP `s2`; BIP341 vectors kept as published, expected address re-encoded with `Params().Bech32HRP()` |
| `util_tests` (6) | Ported | `message_verify` addresses re-encoded with S256 base58 prefixes (same key hashes; `MESSAGE_MAGIC` unchanged, signatures still valid) |
| `miniminer_tests` (4) | Ported | tx2/tx4 tie at equal feerate is broken by txid; expected order now derived from the txids |
| `disconnected_transactions` (3) | Ported | Uses the first 100 of the fixture's `COINBASE_MATURITY` (200) coinbases |
| `bip328_tests` (3) | Ported | BIP328 vectors kept as published; version bytes swapped to S256's `EXT_PUBLIC_KEY` before comparing |
| `walletload_tests` (2) | Ported | Descriptor xpub re-encoded with S256 version bytes, checksum recomputed |
| `interfaces_tests` (2) | Ported | Fixture tip height is `COINBASE_MATURITY`, not 100 |
| `wallet_tests` (1) | Ported | One mature coinbase is 100 COIN |

### Decision: `MAX_MONEY` raised to 84M (2026-10-02)

S256 issues 100 COIN per block halving every 420,000 blocks, so total supply
approaches 84M COIN (83,999,999.95), but `MAX_MONEY` was Bitcoin's 21M, which
supply passes at height 210,000. After that, transactions over 21M would have
been invalid (`src/consensus/tx_check.cpp`, `src/consensus/tx_verify.cpp`) and
wallet balance sums over 21M would throw (`src/wallet/receive.cpp`,
`src/wallet/wallet.cpp`).

Decision (option A): set `MAX_MONEY` to 84M with no activation height, on
branch `fix/max-money`, shipping with the 17,500 release. No fork logic is
needed: until supply exceeds 21M no transaction can exceed it. The IPC
interface's `maxMoney` (`src/ipc/capnp/mining.capnp`) changed with it.

Still on Bitcoin's 21M, to port later:
- `src/test/data/tx_valid.json` "MAX_MONEY output" vectors (still pass, but no
  longer test the boundary; an 84M vector needs re-signing)
- `src/test/compress_tests.cpp` 21M round-trip case (passes, not a boundary)
- `test/functional/test_framework/messages.py` `MAX_MONEY` and
  `CTransaction.is_valid()`, `compressor.py` (step 2)
- functional tests using `MAX_MONEY`: `feature_assumeutxo.py`,
  `mempool_limit.py`, `p2p_ibd_txrelay.py`, `mempool_accept.py`

## Functional tests (`test/functional`)

### Framework (`test/functional/test_framework/`): ported

- bech32 HRPs `s2` / `ts2` / `s2rt`; address constants re-encoded and checked
  against the node (`validateaddress`, `getdescriptorinfo`)
- `COINBASE_MATURITY` 200; `create_coinbase()` pays 100 COIN by default (an
  explicit `nValue` is still used as-is); `BLOCK_SUBSIDY` constant
- shared cache height `COINBASE_MATURITY + 99` = 299 (upstream 199), so tests
  start at height 300 instead of 200; 25 mature coinbases per cache address as
  upstream
- `MAX_MONEY` 84M; mainnet/testnet4 magic bytes
- binary names, datadir, pid file, `sha256coin.conf`
- No change needed: regtest base58 prefixes/WIF, regtest magic, genesis time,
  `0x207fffff` difficulty (LWMA honours `fPowNoRetargeting`), AuxPoW (blocks
  without the auxpow bit stay valid)

Not ported (not in the core set): signet magic, `compressor.py` self-test.

### Core set (step 2), 36 runs: all pass (2026-10-03)

The 20 failures below were ported in step 4; the table records their causes.

Every failure is a Bitcoin value hard-coded in the test itself; none points to
a node bug.

| Test | Status | Cause |
|---|---|---|
| `feature_auxpow_segwit.py` | Passing | S256-native |
| `mining_getblocktemplate_longpoll.py`, `mining_prioritisetransaction.py` | Passing | |
| `p2p_blocksonly.py`, `p2p_compactblocks.py`, `p2p_getdata.py`, `p2p_ping.py` | Passing | |
| `rpc_help.py`, `rpc_net.py` (v1, v2), `rpc_signmessagewithprivkey.py`, `rpc_uptime.py` | Passing | |
| `wallet_createwallet.py` (+ `--usecli`), `wallet_keypool.py`, `wallet_listtransactions.py` | Passing | |
| `mining_basic.py` | Ported (was blocked by the node bug below, now fixed) | `-blockversion=1337` sets the AuxPoW bit (0x100) without an auxpow; the node segfaults serializing the template. See "Node bug found" below |
| `rpc_getchaintips.py`, `rpc_misc.py`, `p2p_leak.py` | Failing | Expect tip height 200/201 (now 300/301) |
| `wallet_basic.py`, `wallet_balance.py`, `wallet_send.py`, `wallet_address_types.py` | Failing | Expect 50-coin coinbase balances |
| `rpc_blockchain.py` (v1, v2) | Failing | UTXO set totals assume 50-coin subsidy |
| `feature_segwit.py` (v1, v2), `p2p_segwit.py` | Failing | Spend coinbases that are immature under maturity 200 |
| `mining_template_verification.py`, `p2p_invalid_block.py` (v1, v2) | Failing | 100-coin "overspend" is a valid S256 subsidy |
| `rpc_createmultisig.py` | Failing | `bcrt` literal |
| `rpc_rawtransaction.py` | Failing | Pruning heights assume the 200-block start |
| `p2p_handshake.py` (v1, v2) | Failing | "24h" limited-peer window is 144 blocks; at S256's 20-min spacing that is 48h (`src/net_processing.cpp:156, 1344`) |

Running: no environment variables needed any more,
`python3 test/functional/test_runner.py <tests>` from `build-release`.

### Node bug found (2026-10-02): crash serializing headers of merge-mined blocks

`CBlockHeader` serialization (`src/primitives/block.h:51-54`) dereferences
`auxpow` whenever `nVersion` has `VERSION_AUXPOW_BIT`, but headers rebuilt from
the block index (`CBlockIndex::GetBlockHeader()`, `src/chain.h:185-196`) carry
the bit and no auxpow. Serializing one segfaults the node. Reproduced:
- a fresh peer syncing headers from a node that holds one merge-mined block
  crashes that node (`getheaders` reply, `src/net_processing.cpp:4452`)
- `getblockheader <aux block hash> false` crashes the node
  (`src/rpc/blockchain.cpp:661`)
Same path, not separately reproduced: header announcements
(`src/net_processing.cpp:5876, 5883`), REST `/headers` (`src/rest.cpp:240, 251`).
Also hit by `-blockversion` with bit 8 set (regtest-only option).
Fixed on `fix/auxpow-header-crash` (`a9ecaab931`: `GetPureHeader()` + `ReadBlockHeader()`).

### Also known

### Regtest assumeutxo entries (approved 2026-10-03)

Regtest `m_assumeutxo_data` held Bitcoin's snapshots, whose blocks cannot
exist on S256's regtest chain. Replaced with values measured on S256's
deterministic chains (`src/kernel/chainparams.cpp`):

| Used by | Bitcoin | S256 |
|---|---|---|
| unit tests (200-block fixture + 10) | 110 | 210, chain_tx_count 211 |
| `feature_assumeutxo.py`, `wallet_assumeutxo.py`, `tool_bitcoin_chainstate.py` (cache 299 + 100) | 299 | 399, chain_tx_count 434 |
| fuzz target `utxo_snapshot` | 200 | **unchanged: regenerate with a fuzz build later** |

`feature_assumeutxo.py` port, besides heights and hashes: the coin height that
must be above the snapshot base moved from 364 to 464; the mempool spend of a
coin known only from the snapshot uses the MiniWallet output in block 300
(none of the 100 not-yet-downloaded coinbases is mature at 200); the cached
chain overflows one 64 KiB `-fastprune` blockfile, so assumed blocks start in
`blk00002`. `tool_bitcoin_chainstate.py` is skipped in this build (no
`bitcoin-chainstate` binary).

### rc1 ports (2026-10-03)

- IBD tests assuming Bitcoin's 24 h max tip age (S256: 7 days):
  `feature_maxtipage.py`, `feature_minchainwork.py`, `p2p_ibd_txrelay.py`.
- `p2p_auxpow_headers.py`: the node connects blocks one at a time, so a busy
  node may announce three large headers in more than one message; the test
  now checks the size limit on every message and retries until the inv
  fallback is exercised.
- Merge leftovers from v31.1 removed: `tool_wallet.py` (createfromdump with an
  unnamed wallet, which v31.1 rejects) and a duplicated block in `rpc_misc.py`.
  `tool_wallet.py` still fails further on (101 blocks then a spend: maturity 100).
- Signet: the genesis nonce was Bitcoin's and did not meet the target with
  S256's coinbase, so no signet node could start. Nonce re-mined (node change,
  approved), genesis asserts enabled. `tool_signet_miner.py` and
  `wallet_crosschain.py` pass; `feature_signet.py` still submits Bitcoin's
  pregenerated signet blocks (to regenerate).

The remaining functional failures of the full suite (all Bitcoin values:
maturity 100 and 200-block heights, 50-coin balances, `bcrt` and mainnet
address literals and vectors, Bitcoin's genesis hash, the literal
`bitcoin.conf`, the 144-block versionbits warning period) are ported after rc1.

## Skipped tests

None yet.
