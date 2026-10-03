# Security Policy

## Supported Versions

Security fixes go into the latest SHA256Coin Core release. Releases are published at
https://github.com/sha256coin/s256-core/releases. Please make sure an issue still exists in the
latest release (or release candidate) before reporting it.

## Reporting a Vulnerability

Please report security issues privately. **Do not open a public GitHub issue, pull request or
discussion for a vulnerability.**

1. **Preferred: GitHub private vulnerability reporting.** Go to the repository's
   [Security tab](https://github.com/sha256coin/s256-core/security) and choose
   **"Report a vulnerability"**. Only the maintainers can see the report.
2. **Fallback: email** security@sha256coin.eu, for example if you can't use GitHub.

This is for security issues only, not for support questions. Helpful details include:

- the affected version or commit;
- the impact (for example: consensus split, crash, remote denial of service, loss of funds,
  privacy leak);
- the steps to reproduce, or a proof of concept;
- whether you know of the issue being exploited.

## What to expect

- We acknowledge your report **within 72 hours**.
- We send a first assessment (whether we can reproduce it, and how severe we think it is)
  **within 14 days**.
- We keep you informed while we work on a fix and tell you when a fixed release is planned.
- With your permission, we credit you in the release notes.

## Disclosure

Please **don't publish details of a vulnerability** (in public issues, social media, write-ups or
exploit code) **until a release that fixes it is out** and node operators have had a reasonable
chance to upgrade. We'll agree a disclosure date with you. A consensus or network issue may need
a coordinated upgrade before it can be disclosed.

Vulnerabilities in code SHA256Coin Core inherits unchanged from Bitcoin Core may also affect
Bitcoin Core. Please report those to Bitcoin Core as well, as described in its own
[security policy](https://github.com/bitcoin/bitcoin/blob/master/SECURITY.md).
