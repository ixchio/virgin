# Security policy

Virgin is currently an engineering preview for a small trusted user group. It is
not independently audited. Security defects should be reported privately to the
maintainer rather than demonstrated against another person's browsing data.

## Qt WebEngine and Chromium updates

Virgin does not maintain a Chromium fork. The shipped Chromium build comes from
Qt WebEngine, so a Virgin release is considered supportable only while its Qt
WebEngine package receives security updates.

The release routine is intentionally small and enforceable:

1. CI rebuilds every week with a freshly pulled Ubuntu builder image.
2. `virgin --runtime-version` records the exact Qt and Chromium versions in every
   release log; the same values are visible at `virgin://version`.
3. After a Qt WebEngine security update, rebuild the AppImage, run all automated
   tests and [the interactive checklist](docs/release-checklist.md), and replace
   the previous artifact.
4. Stop distributing an artifact if its underlying Qt WebEngine package is no
   longer supported. Never describe an old bundled engine as secure.

There is no silent in-app binary updater yet. Friends receiving preview builds
must replace the complete AppImage when a new build is published.

## Release blockers

- A sandbox-disable switch is accepted.
- A certificate, permission, private-profile, cookie, or container isolation test fails.
- The AppImage cannot print its runtime versions or create a sandboxed renderer.
- ASan/UBSan, hardened GCC/Clang, or the interactive daily-site checklist fails.
