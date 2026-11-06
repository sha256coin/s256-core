#!/usr/bin/env python3
# Copyright (c) 2026-present The SHA256Coin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the copyright lines printed by -version.

SHA256Coin began in 2025, so its line starts then. The Bitcoin Core line
(MIT attribution) keeps its own start year, 2009. The current year is not
hard-coded here: both lines must end in the same year.
"""

import re
import subprocess

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class VersionCopyrightTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        binaries = self.get_binaries()
        for name, argv in (
            ("node", binaries.node_argv()),
            ("cli", binaries.rpc_argv()),
            ("tx", binaries.tx_argv()),
            ("util", binaries.util_argv()),
            ("wallet", binaries.wallet_argv()),
        ):
            self.log.info(f"{name} -version prints separate SHA256Coin (2025) and Bitcoin Core (2009) lines")
            out = subprocess.run(argv + ["-version"], capture_output=True, text=True, check=True).stdout
            sha = re.search(r"^Copyright \(C\) (\d{4})-(\d{4}) The SHA256Coin developers$", out, re.M)
            core = re.search(r"^Copyright \(C\) (\d{4})-(\d{4}) The Bitcoin Core developers$", out, re.M)
            assert sha, out
            assert core, out
            assert_equal(sha.group(1), "2025")
            assert_equal(core.group(1), "2009")
            assert_equal(sha.group(2), core.group(2))
            assert int(sha.group(2)) >= 2026, out


if __name__ == '__main__':
    VersionCopyrightTest(__file__).main()
