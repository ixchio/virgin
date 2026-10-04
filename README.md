# Virgin Browser

Virgin is a local-first browser shell built with C++20, Qt 6 Widgets, and Qt WebEngine. Chromium renders pages; Virgin owns the native UI, profiles, permissions, navigation policy, blocker, history, bookmarks, sessions, and downloads.

This repository is an engineering preview, not a security-audited 1.0 release. It has no Virgin cloud, account, sync, analytics SDK, crash uploader, or telemetry endpoint. Websites, search providers, DNS, and configured filter-list sources still receive the network traffic required to use them.

## Download v0.2.0

- **Ubuntu 22.04+ (x86_64):** [`Virgin-0.2.0-x86_64.AppImage`](https://github.com/ixchio/virgin/releases/download/v0.2.0/Virgin-0.2.0-x86_64.AppImage)
- **Windows (x64):** [`Virgin-0.2.0-win64.zip`](https://github.com/ixchio/virgin/releases/download/v0.2.0/Virgin-0.2.0-win64.zip)

All releases and their notes are available on the [GitHub Releases page](https://github.com/ixchio/virgin/releases).

## Install on Ubuntu 22.04+

Virgin ships as one AppImage. Download it, then either double-click it in Files or run:

```bash
chmod +x Virgin-0.2.0-x86_64.AppImage
./Virgin-0.2.0-x86_64.AppImage
```

If Ubuntu complains about FUSE, Virgin is still ready; launch it without mounting:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./Virgin-0.2.0-x86_64.AppImage
```

The AppImage is built and runtime-tested against Ubuntu 22.04 (GLIBC 2.35), so it supports Ubuntu 22.04 and newer desktop releases. Ubuntu 20.04 and older are not supported by this Qt WebEngine build.

### Add to the Linux application menu

From a repository checkout containing the downloaded or locally built AppImage, run:

```bash
./tools/install_appimage.sh /path/to/Virgin-0.2.0-x86_64.AppImage
```

The command installs only for the current user, creates a `virgin` launcher, and adds **Virgin** to the application menu. It does not require `sudo`.

## Install on Windows x64

Extract `Virgin-0.2.0-win64.zip`, then launch `virgin.exe`. The package is portable: it does not have a separate installer or require administrator privileges.

Keep the extracted folder intact. `virgin.exe`, `resources`, the Qt and WebEngine runtime files, and `share/virgin/filters` are one package and must stay together.

## Check an artifact

Each release artifact exposes its bundled version information without opening a browser window:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./Virgin-0.2.0-x86_64.AppImage --runtime-version
```

Use `--runtime-version` on the Windows executable from PowerShell as well:

```powershell
.\virgin.exe --runtime-version
```

## Implemented

- Tabs, navigation, reopen, fullscreen, downloads, local history/bookmarks, and default-profile session restore.
- Persistent normal profile, true off-the-record private profile, panic close, and user-created isolated container profiles.
- Default-deny permission prompts with persistent normal/container choices and memory-only private choices.
- Sandbox-disable refusal, certificate fail-closed policy, user-gesture popup policy, external-scheme confirmation, and typed top-level-only `file://` navigation.
- Standard/strict/private cookie modes, WebRTC public-interface protection, strict canvas-read blocking, and configurable HTTPS-first/search behavior.
- ABP-style network-rule subset, exceptions, domain/resource/party constraints, domain trie, token index, Aho-Corasick matching, bounded cache, cosmetic CSS rules, accurate per-site counters, parser fuzz smoke, and automatic HTTPS EasyList/EasyPrivacy updates.
- Hardened release flags, warnings-as-errors builds, GCC/Clang CI, ASan/UBSan CI, unit tests, benchmark gates, desktop entry, icon, and install rules.

The future local agent API from the design document is intentionally not exposed. It is a post-stability feature because browser automation over authenticated sessions requires a separately reviewed capability and IPC security model.

## Requirements

- CMake 3.22+
- Ninja
- C++20 compiler
- Qt 6.2+ with Widgets, WebEngine, Network, SQL, and the SQLite SQL driver

Ubuntu 22.04+ packages:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-webengine-dev \
  libqt6sql6-sqlite libsqlite3-dev pkg-config
```

## Build and test

The reproducible release path uses Ubuntu 22.04 and Qt 6.2:

```bash
./tools/container_build.sh
```

For an already configured host:

```bash
./build.sh
./build/virgin
```

Hardened release:

```bash
cmake -S . -B build-release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVIRGIN_ENABLE_HARDENING=ON \
  -DVIRGIN_WARNINGS_AS_ERRORS=ON
cmake --build build-release --parallel
ctest --test-dir build-release --output-on-failure
```

Sanitizers:

```bash
cmake -S . -B build-sanitize -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVIRGIN_ENABLE_SANITIZERS=ON \
  -DVIRGIN_ENABLE_HARDENING=OFF
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Install into a staging prefix:

```bash
cmake --install build-release --prefix "$PWD/stage"
```

Bundled AppImage (built and tested inside the pinned Qt container):

```bash
./tools/build_appimage.sh
APPIMAGE_EXTRACT_AND_RUN=1 ./dist/Virgin-0.2.0-x86_64.AppImage
```

`APPIMAGE_EXTRACT_AND_RUN=1` is only needed on systems where FUSE is unavailable.
Use `--runtime-version` to verify the Qt and Chromium versions embedded in an artifact.

The release workflow also compiles, tests, and packages the Windows x64 build with MSVC 2022 and Qt 6.8 before publishing a tag.

## Security invariants

- Sandbox escape switches (`QTWEBENGINE_DISABLE_SANDBOX`, `--no-sandbox`, and `--disable-sandbox`) abort startup.
- Private windows never write Virgin history/session records; their native WebEngine profile is destroyed after the last window closes.
- Certificate errors reject by default; only main-frame errors that Qt 6.8+ marks overridable can reach an explicit override dialog. Older Qt builds fail closed because that API cannot prove an error belongs to the main frame.
- Downloads are sanitized, uniquely named, never auto-executed, and suspicious extensions require confirmation.
- Remote pages cannot launch external applications without a user link gesture and explicit confirmation.
- Filter lists are bounded data. Remote scriptlets are not executed, corrupt caches are rejected, and active rules swap atomically.
- Each normal/container/private identity uses a different `QWebEngineProfile` storage boundary.

## Data and controls

Application data lives under `~/.local/share/virgin/` on Linux. Chromium profile state is separated from `virgin.db`, settings, filters, and compiled filter caches. Private browsing uses an off-the-record profile.

Useful controls:

- `Ctrl+T`, `Ctrl+W`, `Ctrl+Shift+T`: tab lifecycle
- `Ctrl+L`: omnibox
- `Ctrl+Shift+P`: private window
- `Ctrl+Shift+X`: panic-close private windows and clear clipboard
- File menu: create/delete container profiles
- View menu: history, bookmarks, downloads, settings, and manual filter-list update
- Shield: current policy/counters, data clearing, and forget-site action

Internal pages are allowlisted under `virgin://newtab`, `settings`, `history`, `downloads`, `privacy`, and `version`.

## Known release boundaries

- The bundled filter files are offline bootstrap rules. Virgin fetches current EasyList/EasyPrivacy at startup when the saved subscriptions are missing or older than seven days; **View → Update Filter Lists** forces a refresh.
- Qt WebEngine does not expose complete per-origin deletion for every storage type. The UI states when an action must clear profile-wide cookies/cache.
- HTTPS-first currently upgrades before navigation; a polished explicit HTTP fallback interstitial remains future work.
- Browser compatibility and security regression testing still need real desktop lab coverage before a 1.0 claim.
- The current real EasyList/EasyPrivacy miss-heavy benchmark is above the desired blocker budget; this is a measured optimization target, not a claimed achievement.
- No password manager, cloud sync, extension compatibility layer, custom TLS/network stack, or agent API is included.

See [docs/architecture.md](docs/architecture.md), [docs/release-checklist.md](docs/release-checklist.md), [SECURITY.md](SECURITY.md), [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and the original design document supplied separately for the larger roadmap.

## License

Virgin Browser is free software licensed under **GPL-3.0-or-later**. You may use,
study, modify, and redistribute it under those terms. See [LICENSE](LICENSE).
