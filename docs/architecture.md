# Virgin architecture and delivery status

Virgin is a native trust-control shell around Qt WebEngine/Chromium. Web content stays behind Chromium's renderer/site-isolation/sandbox boundary; browser-owned decisions remain in C++/Qt components.

```text
BrowserWindow / BrowserTab / VirginPage
                  │
      ProfileManager / VirginProfile
        ├── RequestInterceptor → AdBlockEngine
        ├── CookieFilter
        ├── PermissionManager
        ├── FilterUpdater
        └── DownloadManager
                  │
             Qt WebEngine
                  │
              Chromium
```

## Storage boundaries

```text
~/.local/share/virgin/
├── profiles/default/                 persistent Chromium state
├── profiles/containers/<validated>/  isolated Chromium state + permissions
├── virgin.db                         history/bookmarks/sessions metadata
├── settings.json                     atomic JSON settings
└── filters/                          downloaded lists and bounded cache
```

Private state uses a parent-owned off-the-record `QWebEngineProfile`. Private windows do not receive history/session writers; panic closes those windows, then profile destruction removes the in-memory Chromium state.

## Phase status

| Design phase | Implemented state | Remaining release work |
|---|---|---|
| 0 Foundation | Native shell, Qt WebEngine, navigation, secure settings | desktop compatibility lab |
| 1 Browser core | tabs/reopen, downloads, fullscreen, history, bookmarks, session, crash page | richer multi-window session model |
| 2 Privacy core | normal/private profiles, permissions, TLS/popup/scheme/file policy, WebRTC/canvas/cookies | automated browser security pages; HTTPS fallback interstitial |
| 3 AdBlock v1 | interceptor, ABP subset, exceptions, party/resource/domain rules, counters | broaden ABP compatibility corpus |
| 4 Fast blocker | trie/index/Aho/regex path, cache, worker compile, atomic active-rule swap, benchmark/fuzz smoke, manual HTTPS update | signed/pinned metadata is not part of EasyList's normal distribution model |
| 5 Cosmetics | domain-indexed CSS and exceptions; scriptlets rejected | earlier document-start injection and larger golden corpus |
| 6 Containers | create/open/delete UI, validated IDs, isolated WebEngine storage/permissions; container history disabled | per-container history database if users opt in |
| 7 Hardening | release flags, sandbox refusal, bounded cache/update, dangerous-download handling, GCC/Clang/sanitizer CI | independent audit and real-site regression matrix |
| Future agent API | intentionally absent | separate capability-token, IPC, origin, consent, and audit design |

## Request hot path

`RequestInterceptor` creates a request context and reads an immutable compiled-rules pointer. Matching uses exact maps, a reverse-domain trie, Aho-Corasick candidates, and a bounded regex set. It performs no disk, network, or database I/O. A worker compiles updates and atomically swaps the shared pointer after validation.

## Trust decisions

- `RuntimeSecurityCheck`: refuses known sandbox-disable routes before creating WebEngine.
- `VirginPage`: gates top-level navigation, new windows, external schemes, permissions, and certificates.
- `CookieFilter`: uses Chromium's third-party classification from Qt's cookie filter callback.
- `PermissionManager`: keys decisions by normalized origin; persistent only for persistent profiles.
- `ContainerManager`: accepts only `[A-Za-z0-9_-]{1,64}` before creating or deleting storage paths.
- `FilterUpdater`: HTTPS-only, safe redirects, timeout, 10 MiB per-list limit, atomic writes, background compile, and previous-rule preservation on failure.

## Testing boundary

The automated suite covers rule behavior, party/resource constraints, corrupt cache rejection, public-suffix heuristics, sandbox flag rejection, HTTPS-first rules, internal URL allowlisting, atomic settings, parser fuzz smoke, and 10k/100k/300k performance gates. An offscreen process smoke test verifies startup and renderer creation without disabling the Chromium sandbox. Real webcam, microphone, WebRTC, media-codec, OAuth, and hostile-certificate scenarios require an interactive test environment.
