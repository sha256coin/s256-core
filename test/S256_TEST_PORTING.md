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
| `MAX_MONEY` | 21,000,000 COIN (unchanged from Bitcoin) | `src/consensus/amount.h` |

## Unit tests (`test_bitcoin`)

Baseline before porting (full run with the exclude list): 14,256 failures.

| Suite / case | Status | Notes |
|---|---|---|
| `validation_tests/block_subsidy_test` | Ported | Initial subsidy 50 -> 100 COIN |
| `validation_tests/subsidy_limit_test` | **Blocked: open question** | S256 total issuance (83,999,999.95 COIN) exceeds `MAX_MONEY` (21M) from height 210,000; the test's `MoneyRange(nSum)` check fails there. Needs a decision on `MAX_MONEY` (consensus), see below |
| `spend_tests` (10) | Failing | Step 3 |
| `script_standard_tests` (8) | Failing | Step 3 |
| `util_tests` (6) | Failing | Step 3 |
| `miniminer_tests` (4) | Failing | Step 3 |
| `disconnected_transactions` (3) | Failing | Step 3 |
| `bip328_tests` (3) | Failing | Step 3 |
| `walletload_tests` (2) | Failing | Step 3 |
| `interfaces_tests` (2) | Failing | Step 3 |
| `wallet_tests` (1) | Failing | Step 3 |

### Open question: `MAX_MONEY` vs S256 supply

S256 issues 100 COIN per block halving every 420,000 blocks, so total supply
approaches 84M COIN, but `MAX_MONEY` is Bitcoin's 21M. Total supply passes 21M
at height 210,000. `MAX_MONEY` is enforced per transaction
(`src/consensus/tx_check.cpp:29-32`, `src/consensus/tx_verify.cpp:186`) and in
wallet balance sums (`src/wallet/receive.cpp:46`, `:94`,
`src/wallet/wallet.cpp:1696`), so after that height a transaction moving more
than 21M COIN would be invalid and a wallet holding more than 21M would throw.
Litecoin, with the same 4x supply, set `MAX_MONEY` to 84M. Raising it is a
consensus change (it relaxes a rule, so a hard fork) and is not done here.

## Functional tests (`test/functional`)

Framework not yet ported (step 2). Done so far:

| Item | Status |
|---|---|
| Framework writes `sha256coin.conf` | Done |
| `feature_auxpow_segwit.py` (S256-native, clean chain) | Passing |

## Skipped tests

None yet.
