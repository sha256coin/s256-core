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
| `MAX_MONEY` | 84,000,000 COIN | `src/consensus/amount.h` (raised from Bitcoin's 21M, see decision below) |

## Unit tests (`test_bitcoin`)

Baseline before porting (full run with the exclude list): 14,256 failures.

| Suite / case | Status | Notes |
|---|---|---|
| `validation_tests/block_subsidy_test` | Ported | Initial subsidy 50 -> 100 COIN |
| `validation_tests/subsidy_limit_test` | Ported | Cap 50 -> 100 COIN; range 14M -> 28M blocks (doubled halving interval); expected total 8,399,999,995,380,000 sat |
| `transaction_tests/tx_invalid` | Ported | `MAX_MONEY + 1` vectors re-encoded at 84M (failed after the `MAX_MONEY` change) |
| `spend_tests` (10) | Failing | Step 3 |
| `script_standard_tests` (8) | Failing | Step 3 |
| `util_tests` (6) | Failing | Step 3 |
| `miniminer_tests` (4) | Failing | Step 3 |
| `disconnected_transactions` (3) | Failing | Step 3 |
| `bip328_tests` (3) | Failing | Step 3 |
| `walletload_tests` (2) | Failing | Step 3 |
| `interfaces_tests` (2) | Failing | Step 3 |
| `wallet_tests` (1) | Failing | Step 3 |

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

Framework not yet ported (step 2). Done so far:

| Item | Status |
|---|---|
| Framework writes `sha256coin.conf` | Done |
| `feature_auxpow_segwit.py` (S256-native, clean chain) | Passing |

## Skipped tests

None yet.
