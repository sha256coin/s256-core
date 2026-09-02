S256 Core - Digital Platinum
============================

**Website:** https://sha256coin.eu/
**Block Explorer:** https://explorer.sha256coin.eu/

## What is S256?

S256 is a Bitcoin fork that takes the contrarian approach: **"Double the Work, Double the Value"**

Based on Bitcoin Core v31.1, S256 doubles key parameters for increased scarcity and deliberation:

## What's New in v2.2.0

Rebased onto upstream **Bitcoin Core v31.1** (from v30.0), carrying forward roughly a year and a half of upstream fixes and improvements, plus:

- **Testnet fixes** — testnet3 and testnet4 nodes were silently connecting to the *real* Bitcoin test networks instead of S256's own (unmodified network magic bytes and DNS/fixed seeds inherited from upstream); both are now properly isolated on their own network identity, with freshly mined genesis blocks and working difficulty adjustment.
- **Fixed a persistent sync warning** — "unknown new rules activated (versionbit 2)" no longer appears in bitcoin-qt or the daemon after every sync.
- **Critical fix caught before it could ship** — this rebase briefly introduced a bug that would have crashed every node on its very first sync (a missing anti-DoS parameter for header synchronization); found via a live sync test against real mainnet peers, fixed, and re-verified before release. It was never active on any released version.
- **Pre-fork validation** — rehearsed the upcoming AuxPoW/LWMA transition at block 17,500 end to end, including a simulated hashrate drop and recovery, to check LWMA's difficulty response before the real fork happens. See [Pre-Fork Validation](#pre-fork-validation) below.
- **Automated releases** — tagged releases now build and publish Linux and Windows binaries automatically.
- Substantially hardened unit test coverage across key encoding, descriptors, and the P2P transport layer.

## What's New in v2.0.1

A coordinated hard fork activating at **block 17,500** on mainnet, bundling two consensus changes plus network/RPC improvements:

- **AuxPoW (merged mining)** — other SHA256 coins' pools can now merge-mine S256 alongside their own chain at no extra cost to their miners' hashrate. S256 itself is unaffected either way: legacy (non-AuxPoW) blocks remain valid forever, before and after activation. See [AuxPoW / Merged Mining](#auxpow--merged-mining) below.
- **LWMA difficulty adjustment** — replaces the classic 1,008-block-window DAA with per-block LWMA (96-block window) from block 17,500 onward. The old DAA could leave the chain stuck at a too-high difficulty for up to a full 1,008-block window if hashrate dropped suddenly; LWMA responds within a handful of blocks instead.
- **Additional DNS + fixed seed nodes** for more resilient peer discovery (previously a single point of failure).
- **RPC help text fixes** — example amounts now correctly show `S256` instead of a leftover `BTC`.
- **v2.0.1**: fixes two AuxPoW bugs found by new end-to-end test coverage before activation — a merge-mining tag nonce that was incorrectly tied to the parent header's own mining nonce (would have made merge-mining computationally infeasible), and a hash-timing mismatch between `createauxblock` and validation that would have made every `submitauxblock` call fail. Neither was ever active on mainnet (both predate block 17,500), so no chain impact — see the [v2.0.1 commit](https://github.com/sha256coin/s256-core/commit/6f39940304) for details.

### Key Specifications

| Parameter | Bitcoin | S256 |
|-----------|---------|------|
| Block Time | 10 minutes | **20 minutes** |
| Block Reward | 50 BTC | **100 S256** |
| Total Supply | 21 million | **84 million** |
| Halving Interval | 210,000 blocks (~4 years) | **420,000 blocks (~16 years)** |
| Coinbase Maturity | 100 confirmations | **200 confirmations** |
| Algorithm | SHA256 (PoW) | **SHA256 (PoW)** |

### Network Details

- **Port:** 25256
- **RPC Port:** 25332
- **Address Prefix:** S (e.g., S1A2B3C...)
- **Script Address Prefix:** 8
- **Bech32 HRP:** s2
- **Magic Bytes:** 0xf1, 0xc2, 0xa5, 0xd8
- **DNS Seeds:** `seednode.sha256coin.eu`, `s256seednode.bitcoinsilver.eu`, `sha256-mining.go.ro`
- **Fixed Seeds:** hardcoded fallback peer IPs baked into the binary (see `contrib/seeds/nodes_main.txt`), so peer discovery still works if DNS is unavailable

### Genesis Block

- **Hash:** `00000000abe2a78ceb00eca81258804d59fe4ad45345e1750e705139e6da7297`
- **Timestamp:** November 30, 2025
- **Message:** "S256 2025-11-30 Double the Work Double the Value"

## Building S256 Core
Download Current Compiled Binaries from the Official Web Site:
                **https://sha256coin.eu/#downloads**
### Dependencies

Refer to the build documentation in the `doc/` folder:
- Linux: [doc/build-unix.md](doc/build-unix.md)
- Windows: [doc/build-windows.md](doc/build-windows.md)
- macOS: [doc/build-osx.md](doc/build-osx.md)

### Quick Build (Linux)

#### Option 1: CLI Only (daemon + bitcoin-cli)
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config \
    libssl-dev libevent-dev libboost-all-dev libsqlite3-dev

# Build
cmake -B build
cmake --build build -j$(nproc)

# Binaries will be in: build/bin/
```

#### Option 2: With GUI (sha256coin-qt)
```bash
# Install all dependencies including Qt libraries (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config \
    libssl-dev libevent-dev libboost-all-dev libsqlite3-dev \
    libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 \
    libxcb-xinerama0 libxcb-xkb1 libxkbcommon-x11-0 libfontconfig1 \
    libfreetype6

# Build with GUI enabled
cmake -B build -DBUILD_GUI=ON
cmake --build build -j$(nproc)

# GUI wallet: build/bin/sha256coin-qt
# Daemon: build/bin/sha256coind
# CLI: build/bin/sha256coin-cli
```

**Note:** If you get "library not found" errors when running the GUI, install the missing Qt/X11 libraries listed above.

### Cross-Compile for Windows (Linux)

```bash
# Install mingw-w64
sudo apt-get install g++-mingw-w64-x86-64

# Configure and build
cmake -B build-win \
    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/x86_64-w64-mingw32.cmake \
    -DBUILD_GUI=OFF

cmake --build build-win -j$(nproc)

# Windows binaries will be in: build-win/bin/
```

## Running S256

### Start the daemon:
```bash
./build/bin/bitcoind -daemon
```

### Check blockchain status:
```bash
./build/bin/bitcoin-cli getblockchaininfo
```

### Stop the daemon:
```bash
./build/bin/bitcoin-cli stop
```

### Data Directory

**Linux:** `~/.sha256coin/`
**Windows:** `%APPDATA%\SHA256Coin\`
**macOS:** `~/Library/Application Support/SHA256Coin/`

### Configuration

Create `sha256coin.conf` in your data directory for custom settings:
```ini
# RPC Settings
rpcuser=yourusername
rpcpassword=yourpassword
rpcallowip=127.0.0.1

# Network
port=25256
rpcport=25332

# Optional: Add nodes
addnode=sha256coin.eu:25256
```

## Mining

S256 uses SHA256 Proof-of-Work, compatible with Bitcoin mining hardware (ASICs).

**Solo Mining:**
```bash
./bitcoin-cli generatetoaddress 1 "your_s256_address"
```

**Pool Mining:** Contact pool operators or set up your own using sha256-nomp.

## AuxPoW / Merged Mining

Starting at block 17,500, S256 supports auxiliary proof-of-work (AuxPoW), the same merged-mining
mechanism used by Namecoin, Dogecoin, and other coins. Any other SHA256 chain's pool can merge-mine
S256 alongside its own block, at no extra cost to its miners' hashrate.

- **Chain ID:** `0x53323536`
- **RPC methods:** `createauxblock <address>` returns a new S256 block template to embed in the parent
  chain's coinbase; `submitauxblock <hash> <auxpow-hex>` submits the completed proof once the parent
  pool has mined a qualifying block.
- **Compatibility:** legacy (non-AuxPoW) blocks remain valid at every height, before and after
  activation — solo miners and existing pools are unaffected.

See `src/auxpow.h`/`src/auxpow.cpp` for the AuxPoW proof format and validation, and `src/rpc/mining.cpp`
for the RPC implementation.

## Pre-Fork Validation

Before block 17,500 activates on mainnet, the AuxPoW/LWMA transition was rehearsed end-to-end on an
isolated test chain — including a simulated hashrate drop and recovery, to check how LWMA's per-block
difficulty adjustment responds when miners come and go. See the full report with charts:

**https://sha256coin.github.io/s256-core/test/lwma-transition-test.html**

(source: [public/test/lwma-transition-test.html](public/test/lwma-transition-test.html))

## Links

- **Website:** https://sha256coin.eu/
- **Block Explorer:** https://explorer.sha256coin.eu/
- **GitHub:** https://github.com/sha256coin
- **Discord:** https://discord.gg/dtn58HrC94
- **Telegram:** https://t.me/+Ecf4ApES37NjZTBk

## Philosophy

> **"Digital Platinum"** - Because excellence takes effort.

S256 embraces difficulty rather than avoiding it:
- Harder to mine, not easier
- Longer block times for deliberation
- True scarcity through proof of real work

## Security

- **Exchange Deposits:** 500+ confirmations recommended
- **Consensus:** Pure Proof-of-Work
- **Difficulty Adjustment:** Classic 1,008-block window (~14 days) below block 17,500; per-block LWMA (96-block window) from block 17,500 onward — see [What's New in v2.0.1](#whats-new-in-v201)

## License

S256 Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/license/MIT.

## Development

S256 is based on Bitcoin Core v31.1 and maintains compatibility with Bitcoin's proven codebase while implementing the 2x parameter modifications.

For technical details on the modifications, see the documentation in the `/doc` folder.

---

*Fork of Bitcoin Core - Modified for S256 specifications*
