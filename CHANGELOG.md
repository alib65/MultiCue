# Changelog

All notable changes to this project are documented in this file.

## [Unreleased] -- M1 engine core

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
