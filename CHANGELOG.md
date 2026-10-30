# Changelog

All notable changes to this project are documented in this file.

## [Unreleased] -- M2 playlist mode

### Added
- `mc_core` playlist data model (`Track`, `Piece`, `Playlist`), JSON
  persistence with a schema-version + migration scaffold (`PlaylistIO`,
  nlohmann-json), natural-sort file ordering, piece-title guessing, and an
  `AutosaveScheduler` (debounced, unit tested with a fake clock).
- `apps/multicue`: a real Playlist-mode window replacing M0's test-tone
  demo -- playlist list with add files/folder/many-folders, reorder/
  duplicate/remove, a per-piece mixer (fader/mute/solo/output per track,
  live while playing), transport (play/pause/stop/prev/next, draggable
  position bar, auto-advance with a gap), master fader + limiter controls,
  per-track and per-output level meters, autosave, open/save-as.
- `mc_engine`: per-track/per-output peak+RMS metering snapshot on
  `PieceMixer`/`RealtimeAudioEngine`; per-track direct-mode output
  reassignment (`setTrackOutputChannel`), live while playing; a non-blocking
  `requestStop()`/`isStopFadeComplete()` pair alongside the existing
  blocking `stop()`.
- nlohmann-json dependency (dual vcpkg/Homebrew path, like the others).
- 24 new tests (65 total).

### Fixed (caught before shipping, not after)
- `PieceMixer`'s gain matrix: the originally-planned
  `std::atomic<std::shared_ptr<const GainMatrix>>` does not compile under
  Apple's libc++ despite being standard C++20 -- caught by compiling a
  two-line repro before building on top of it, not by a later crash.
  Replaced with a flat `std::vector<std::atomic<float>>` (one lock-free
  cell per source×output pair).
- `RealtimeAudioEngine::stop()` blocked its caller for up to 500 ms waiting
  out the release fade, which would have frozen the GUI's Stop button.
  Split into non-blocking `requestStop()`/`isStopFadeComplete()`, driven by
  the UI's existing poll timer.
- The playlist UI's gap-countdown timing used a function-local `static`
  that was initialized once for the process's lifetime instead of once per
  gap -- every gap after the first would have fired instantly. Fixed with
  an absolute deadline computed when the gap begins.

### Known gaps (tracked in docs/ROADMAP.md)
- Source-format auto-detection deferred to M3 (same reasoning as `mc_dsp`
  itself -- direct mode doesn't use it yet).
- No crossfade between pieces on auto-advance (brief §5 still open).
- The playlist/mixer UI has no automated test coverage -- built, manually
  re-read end to end, and launched to confirm it runs; not click-tested.
- `deviceWasLost()` still has no UI message or reconnect flow.
- Clang RealtimeSanitizer still not attempted (said "next milestone" at the
  end of M1; still true at the end of M2).

## M1 engine core

### Added
- `mc_engine` real-time audio path: `RingBuffer` (lock-free SPSC), `Voice`
  (background reader thread + `FileSampleSource`, libsndfile + on-the-fly
  libsamplerate resampling, bit-exact passthrough when rates match),
  `GainMatrix` (direct-mode N×M routing), `Envelope` (click-free attack/
  release), `MasterLimiter` (lookahead sample-peak limiter + smoothed master
  gain), `PieceMixer` (the shared mixing core used by both realtime and
  offline paths), `OfflineRenderer`, and `RealtimeAudioEngine` (RtAudio
  live playback: load/play/pause/resume/seek/stop, device-loss detection).
- `multicue-cli render`: direct-mode offline mix of mono track files to a
  WAV file. `multicue-cli play`: live playback against a real device,
  including a built-in pause/resume/seek smoke sequence, for manual
  verification.
- libsndfile and libsamplerate added to `vcpkg.json`, with dual CMake-config/
  pkg-config lookup modules (`cmake/FindSndFilePortable.cmake`,
  `cmake/FindSampleRatePortable.cmake`) mirroring `FindRtAudioPortable.cmake`.
- 26 new Catch2 tests (37 total) covering gain-matrix routing, envelope
  click-freeness, limiter behavior, file-source bit-exactness/resampling,
  mixer routing/mute/solo/silence, offline-render bit-exactness and
  sample-locked multi-voice starts, and voice streaming.
- A second CI job (`sanitizers`, macOS + Ubuntu) running the full test suite
  under ASan/UBSan.

### Verified, not just built
- Full test suite (37/37) passes clean under `-DMULTICUE_ENABLE_ASAN=ON`
  (ASan + UBSan), including the multi-threaded `Voice`/`RingBuffer` code.
- `multicue-cli play` was run against real CoreAudio hardware on the dev
  machine, under both a normal and an ASan/UBSan build: live playback with
  0 underruns over 6 s, and a pause→resume→seek sequence where the measured
  positions matched the expected arithmetic exactly (pause freezes position,
  resume continues from it rather than jumping, seek lands within one
  scripted sleep interval of the requested time).
- `multicue-cli render` was run on real generated WAV fixtures and the
  output verified with `afinfo`/`afplay`.

### Known gaps / deliberate scope cuts (tracked in docs/ROADMAP.md)
- Master limiter is sample-peak, not true-peak; upgraded at M4 alongside
  `mc_dsp::Bs1770Meter` instead of building the oversampling filter twice.
- Device-loss detection exists but has no UI/reconnect flow yet (no UI
  consumes it until M2).
- Nothing in this milestone has run on Windows; only macOS was available in
  this environment. The M0 CI-unverified caveat still applies and now also
  covers M1's code.
- Clang RealtimeSanitizer was not attempted on the audio callback this
  milestone (ASan/UBSan were; RealtimeSanitizer is a stronger, more targeted
  check still worth doing).

## M0 skeleton

### Added
- `docs/ARCHITECTURE.md` and `docs/ROADMAP.md`: design and milestone plan,
  written from a full read of the reference implementation,
  [MultichannelPlayer](https://github.com/alib65/MultichannelPlayer).
- Repo skeleton: root `CMakeLists.txt`, `mc_core` (application identity
  constants), `mc_engine` (RtAudio device enumeration, allocation-free sine
  generator, a test-tone stream), `apps/multicue` (Qt 6 Widgets window:
  device picker, channel picker, play/stop a test tone), `apps/multicue-cli`
  (`list-devices`, `version`).
- `vcpkg.json` manifest (RtAudio, Catch2) plus a dual CMake/pkg-config
  `FindRtAudioPortable` module so the same `CMakeLists.txt` works whether
  dependencies come from vcpkg (CI) or a system package manager (local
  development) -- both paths were built and tested locally before being
  committed.
- `.github/workflows/ci.yml`: macOS/Windows/Ubuntu build+test matrix via the
  vcpkg manifest and `jurplel/install-qt-action`. Not yet verified against a
  real GitHub Actions run -- see README's CI status note.
- Catch2 test suites for `mc_core` and `mc_engine` (7 tests), all passing
  locally via both the Homebrew/pkg-config path and the vcpkg path.
- `LICENSE` (GPL-3.0-or-later, verbatim from gnu.org), `ACKNOWLEDGEMENTS`.

### Known gaps (tracked in docs/ROADMAP.md)
- M0's CI matrix is written but unverified on Windows/Linux (no GitHub
  remote has been pushed to).
- Everything from M1 onward (disk streaming, cue runtime, routing matrix,
  spatial audio, loudness, video, control surfaces, installers) is not
  started.
