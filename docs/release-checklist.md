# Preview release checklist

Do not call a build ready for friends until every item is checked on the exact
AppImage being distributed. Use a fresh temporary profile as well as an upgraded
existing profile. Record the Linux distribution, desktop session, GPU, Qt version,
Chromium version, and artifact SHA-256 with the results.

## Automated gates

- [ ] Hardened warnings-as-errors build passes.
- [ ] GCC, Clang, ASan, and UBSan jobs pass.
- [ ] Ad-block, cookie/security, storage/session, and parser-fuzz tests pass.
- [ ] AppImage starts with the Chromium sandbox enabled and `--runtime-version`
      reports the expected bundled Qt and Chromium versions.
- [ ] Current EasyList/EasyPrivacy parse, cache, reload, and meet the recorded
      blocker latency budget.

## Interactive desktop matrix

- [ ] Cold start, warm start, crash recovery, and a 10-tab session restore.
- [ ] Wikipedia, GitHub, Gmail, Reddit, a shopping site, and a banking test/demo
      site load and navigate without broken first-party content.
- [ ] A normal login survives restart; logout works; strict cookie mode visibly
      warns when it can break a login.
- [ ] YouTube video starts only after user interaction, seeks, enters fullscreen,
      changes quality, and plays audio. Test one DRM service separately and record
      whether Widevine is available.
- [ ] A small file and a multi-gigabyte file download; cancel, retry, duplicate
      names, suspicious extensions, and disk-full behavior are understandable.
- [ ] Camera, microphone, location, notifications, and clipboard prompts show the
      exact origin and remember only the selected scope.
- [ ] Certificate errors, deceptive Unicode hosts, HTTP navigation, and external
      application links show unambiguous identity and risk text.
- [ ] Private cookies, local storage, cache, permissions, and history disappear
      after the final private window closes and do not return after restart.
- [ ] Two containers remain logged into different accounts and cannot see each
      other's cookies/storage; deleting a closed container removes its state.
- [ ] The shield's request/block/ad/tracker numbers match observed requests on the
      current site and reset/clear actions describe their real scope.

## Performance record

- [ ] Cold and warm time-to-first-usable-tab (at least 10 runs each).
- [ ] Whole process-tree RSS/PSS at 1, 10, and 50 settled tabs.
- [ ] Page-load timings for the same saved workload with blocking on and off.
- [ ] Blocker p50/p95/p99 against the exact shipped EasyList/EasyPrivacy files.
- [ ] Container creation and final private-profile destruction latency.

Unchecked items are unverified, not presumed to pass.
