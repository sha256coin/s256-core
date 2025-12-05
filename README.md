S256 Core - Digital Platinum
============================

**Website:** https://sha256coin.eu/
**Block Explorer:** https://explorer.sha256coin.eu/

## What is S256?

S256 is a Bitcoin fork that takes the contrarian approach: **"Double the Work, Double the Value"**

Based on Bitcoin Core v30.0, S256 doubles key parameters for increased scarcity and deliberation:

### Key Specifications

| Parameter | Bitcoin | S256 |
|-----------|---------|------|
| Block Time | 10 minutes | **20 minutes** |
| Block Reward | 50 BTC | **100 S256** |
| Total Supply | 21 million | **84 million** |
| Halving Interval | 210,000 blocks (~4 years) | **420,000 blocks (~8 years)** |
| Coinbase Maturity | 100 confirmations | **200 confirmations** |
| Algorithm | SHA256 (PoW) | **SHA256 (PoW)** |

### Network Details

- **Port:** 25256
- **RPC Port:** 25332
- **Address Prefix:** S (e.g., S1A2B3C...)
- **Script Address Prefix:** 8
- **Bech32 HRP:** s2
- **Magic Bytes:** 0xf1, 0xc2, 0xa5, 0xd8

### Genesis Block

- **Hash:** `00000000abe2a78ceb00eca81258804d59fe4ad45345e1750e705139e6da7297`
- **Timestamp:** November 30, 2025
- **Message:** "S256 2025-11-30 Double the Work Double the Value"

## Building S256 Core

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
- **Difficulty Adjustment:** Every 1,008 blocks (~14 days)

## License

S256 Core is released under the terms of the MIT license. See [COPYING](COPYING) for more information or see https://opensource.org/license/MIT.

## Development

S256 is based on Bitcoin Core v30.0 and maintains compatibility with Bitcoin's proven codebase while implementing the 2x parameter modifications.

For technical details on the modifications, see the documentation in the `/doc` folder.

---

*Fork of Bitcoin Core - Modified for S256 specifications*
