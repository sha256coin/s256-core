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

Still excluded from the full run (crash/cascade, root causes outside these steps): `miner_tests/CreateNewBlock_validity`, `net_peer_connection_tests`, `net_tests/initial_advertise_from_version_message`, `net_tests/advertise_local_address`, `txpackage_tests`, `txvalidationcache_tests/checkinputs_test`, `validation_chainstate_tests/chainstate_update_tip`, `validation_chainstatemanager_tests`.

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

### Core set (step 2), 36 runs: 16 pass, 20 fail

Every failure is a Bitcoin value hard-coded in the test itself; none points to
a node bug.

| Test | Status | Cause |
|---|---|---|
| `feature_auxpow_segwit.py` | Passing | S256-native |
| `mining_getblocktemplate_longpoll.py`, `mining_prioritisetransaction.py` | Passing | |
| `p2p_blocksonly.py`, `p2p_compactblocks.py`, `p2p_getdata.py`, `p2p_ping.py` | Passing | |
| `rpc_help.py`, `rpc_net.py` (v1, v2), `rpc_signmessagewithprivkey.py`, `rpc_uptime.py` | Passing | |
| `wallet_createwallet.py` (+ `--usecli`), `wallet_keypool.py`, `wallet_listtransactions.py` | Passing | |
| `mining_basic.py` | **Blocked: node bug** | `-blockversion=1337` sets the AuxPoW bit (0x100) without an auxpow; the node segfaults serializing the template. See "Node bug found" below |
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
Reported, not fixed (node code).

### Also known

- `feature_assumeutxo.py`: regtest `m_assumeutxo_data` (heights 110/200/299 in
  `src/kernel/chainparams.cpp`) look like Bitcoin's regtest snapshots (not yet
  verified). If so their blocks cannot exist on S256's regtest chain and the
  test needs S256 snapshot values in chainparams (node code): to be reported
  when that test is ported, not changed here.

## Skipped tests

None yet.
