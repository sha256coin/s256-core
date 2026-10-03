Contributing to SHA256Coin Core
===============================

SHA256Coin Core (S256) is a fork of [Bitcoin Core](https://github.com/bitcoin/bitcoin). Contributions are welcome: bug reports, testing (especially on testnet), review, and patches.

Reporting issues
----------------

- **Bugs and feature requests:** open an issue at https://github.com/sha256coin/s256-core/issues. Include your version (`sha256coind -version`), operating system and network (mainnet or testnet). For a crash or sync problem, include the relevant part of `debug.log`.
- **Security vulnerabilities:** never in a public issue. Follow [SECURITY.md](SECURITY.md).
- **Merge-mining and pool integration:** see [POOL_PROXY_NOTE.md](POOL_PROXY_NOTE.md) first.

Contributing changes
--------------------

1. Fork the repository and create a branch from `main`.
2. Make focused commits: one logical change per commit, with a message that says what changed and why. Follow the existing style, `area: short summary` (for example `rpc: …`, `net: …`, `test: …`, `doc: …`).
3. Add or update tests for what you change. Build and run the unit tests and the relevant functional tests before opening a pull request. See [doc/build-unix.md](doc/build-unix.md) and the test READMEs in [src/test](src/test/README.md) and [test](test/README.md).
4. Open a pull request against `main` and describe what it changes, how you tested it, and anything reviewers should look at closely.

### Changes that need extra care

- **Consensus changes** (block or transaction validity, proof of work, AuxPoW, difficulty adjustment, chain parameters) affect every node. They're released only as coordinated upgrades with an activation height announced well in advance, as merged mining was at block 17,500. Open an issue to discuss before writing one.
- **P2P protocol changes** must stay compatible with the node versions still on the network.
- **Tests:** when porting upstream tests, adapt Bitcoin-specific values to S256. Don't change node code just to make a test pass; if a test reveals a real bug, report it and fix it separately. Status per suite: [test/S256_TEST_PORTING.md](test/S256_TEST_PORTING.md).

Code inherited from Bitcoin Core
--------------------------------

Most of the code is unchanged Bitcoin Core code. A bug there may also exist in Bitcoin Core, so consider reporting it upstream as well. Bitcoin Core's own [contributing guide](https://github.com/bitcoin/bitcoin/blob/master/CONTRIBUTING.md) is a good reference for coding conventions and review practice ([doc/developer-notes.md](doc/developer-notes.md) also applies here).

Copyright
---------

By contributing, you agree to license your work under the MIT license, as in [COPYING](COPYING), unless a file states otherwise.
