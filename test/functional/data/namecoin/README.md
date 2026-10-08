# Namecoin auxpow test helpers

`auxpow.py` and `auxpow_testing.py` are copied unmodified, copyright headers
included, from Namecoin Core's `test/functional/test_framework/`:

- Source: https://github.com/namecoin/namecoin-core
- Commit: `d52dff4d3eadee3a09dbf93b5fee67a9f1329a2e` (2026-10-05)

`feature_auxpow_namecoin.py` checks their SHA-256 hashes and uses them to
build auxpows, so S256's auxpow format is tested against Namecoin's own code.
Don't edit them. To update, copy the new versions and update the hashes in
the test.
