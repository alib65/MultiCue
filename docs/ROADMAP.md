# MultiCue — Roadmap

Milestones match `docs/ARCHITECTURE.md`'s module breakdown and the project
brief's §8 list. A milestone is **done** only when, per the project brief §1:

- it builds with no warnings on macOS, Windows and Linux in CI,
- all tests pass and the app runs,
- README and CHANGELOG are updated,
- the work is committed in small, clearly described commits.

Anything not finished when a milestone is declared done goes into this file's
"Open items" list rather than being presented as working.

## Dependency version/license check log

Per `docs/ARCHITECTURE.md` §10, every dependency's current version and
license is verified against its upstream source at the point it is first
added — not recalled from memory. Logged here as it happens:

| Dependency | Checked | Version | License | Notes |
|---|---|---|---|---|
| RtAudio | 2026-10-29 | 6.0.1 | MIT | Via Homebrew bottle (local dev) and vcpkg port (CI); both export the same `rtaudio` pkg-config module, vcpkg additionally exports a `RtAudio::rtaudio` CMake target. Both paths built and tested locally. |
| Catch2 | 2026-10-29 | 3.16.1 | BSL-1.0 (Boost) | Same dual-path story as RtAudio; `Catch2::Catch2WithMain` CMake target confirmed from both Homebrew and vcpkg. |
| Qt | 2026-10-29 | 6.11.1 (local Homebrew) / 6.11.3 (pinned in CI via aqtinstall) | BSD-3-Clause AND GPL-2.0-only AND (GPL-3.0-only WITH Qt-GPL-exception-1.0) AND LGPL-3.0-only (multi-licensed; Widgets module used here under LGPL-3.0-only, linked dynamically, per the project brief) | `brew info qt` / aqtinstall. Homebrew's current stable is 6.12.0; 6.11.x was kept locally to match what was already installed and what CI pins — bump to 6.12.x is a deliberate future decision, not an oversight. |
| vcpkg | 2026-10-29 | git commit `a27c8b7e…` (builtin-baseline in `vcpkg.json`) | MIT | Bootstrapped locally outside the repo (`~/vcpkg`) to verify the CI mechanism end-to-end before committing to it; GitHub-hosted runners have it preinstalled at `$VCPKG_INSTALLATION_ROOT`. |
| libsndfile | 2026-10-29 | 1.2.2 (port-version 2) | LGPL-2.1-or-later | `brew info libsndfile` and vcpkg's `vcpkg.json` agree on 1.2.2. Same dual CMake-config (`SndFile::sndfile`) / pkg-config (`sndfile`) story as RtAudio; `cmake/FindSndFilePortable.cmake`. |
| libsamplerate | 2026-10-29 | 0.2.2 | BSD-2-Clause | `brew info libsamplerate` and vcpkg's `vcpkg.json` agree on 0.2.2. `SampleRate::samplerate` CMake target / `samplerate` pkg-config module; `cmake/FindSampleRatePortable.cmake`. Used for the real-time streaming resampler (`src_process`, not the one-shot `src_simple`); bypassed entirely when the file's rate already matches the device's, which is what keeps direct mode bit-exact. |
| nlohmann-json | 2026-10-29 | 3.12.0 | MIT | `brew info nlohmann-json` and vcpkg's `vcpkg.json` agree on 3.12.0. Header-only; `nlohmann_json::nlohmann_json` CMake target found reliably from both. **Local-dev gotcha:** if Anaconda is on `PATH`, CMake's CONFIG search can find Anaconda's own (older, 3.11.2) copy first via its package registry; pin explicitly with `-Dnlohmann_json_DIR=<brew prefix>/share/cmake/nlohmann_json` if that happens (noted in README). Used in `mc_core` only (the show/playlist-file (de)serialisation), never exposed in a public header, so it stays an implementation detail other modules don't need to know about. |

Dependencies not yet added (FFmpeg, r8brain-free [if libsamplerate's quality
turns out insufficient for some format], pffft, libmysofa, liblo/oscpack,
RtMidi) get their own row here at the milestone that first links them (M3,
M4, M6, M7 respectively), not before — see each milestone's checklist below.

## M0 — Skeleton

Goal: a repo that builds and boots on all three OSes, with a device list and a
test tone — the minimum slice that proves the toolchain, CI, and real-time
audio path all work together.

**Deliberate scope adjustment vs. the original plan:** `mc_dsp` is not
created in M0. Nothing in M0 needs spatial/loudness math, so an empty
placeholder module would just be scaffolding presented as progress; it is
created in M3 when `layoutPreset()` etc. are first implemented. Likewise
`mc_video`/`mc_control` arrive in M6/M7. The module *list* in
`docs/ARCHITECTURE.md` §2 is unchanged — this only affects when each
`add_subdirectory()` actually appears.

- [x] Root `CMakeLists.txt`: project name/version from `AppInfo.h` pattern
      (`docs/ARCHITECTURE.md` intro), C++20, targets for `mc_core`,
      `mc_engine`, `multicue`, `multicue-cli`.
- [x] vcpkg manifest (`vcpkg.json`): RtAudio (with a `platform: linux` feature
      block for ALSA/PulseAudio), Catch2. Paired with a dual CMake-config /
      pkg-config lookup (`cmake/FindRtAudioPortable.cmake`) so the identical
      `CMakeLists.txt` works against vcpkg (CI) or a system package manager
      (local dev) — both paths were configured, built and tested locally
      before being committed (see the dependency log above).
- [x] GitHub Actions CI: macOS, Windows x64, Ubuntu — build + test matrix via
      the vcpkg manifest and `jurplel/install-qt-action`, `-Wall -Wextra
      -Wpedantic` (no warnings observed locally on macOS). **Not yet verified
      by an actual GitHub Actions run** — this repository has not been
      pushed to any remote (the user explicitly asked for it to stay local),
      so the Windows and Ubuntu legs are reviewed-but-unrun. Carried forward
      as an open item below rather than claimed as passing.
- [x] `mc_engine`: RtAudio device enumeration (`DeviceInfo.h`/`.cpp`), a
      single test-tone voice to a chosen output and channel
      (`TestToneEngine`), built on a pure, allocation-free sine generator
      (`SineGenerator.h`/`.cpp`) called directly from the RtAudio callback —
      i.e. the audio-thread rules from `docs/ARCHITECTURE.md` §3.1 are
      followed starting now, not retrofitted later. Sample-accurate *cue*
      scheduling (vs. this single ad hoc stream) is still M1 scope.
- [x] `multicue`: Qt 6 Widgets app window, dark theme by default
      (`DarkTheme.cpp`, Fusion style + a dark `QPalette`), device picker,
      channel picker, play/stop test-tone button, live underrun count.
      Keyboard operability relies on Qt's native tab/focus handling (no
      custom keyboard traps yet to verify); HiDPI is on by default in Qt6.
      Launched locally and confirmed it starts, stays running, and exits
      cleanly; visual/keyboard verification was by process-level check only
      (no screenshot tool available for a native desktop window in this
      environment) -- a manual visual pass is still worth doing before M0 is
      called fully done.
- [x] `multicue-cli`: `list-devices` and `version` subcommands. Verified
      against real hardware (`list-devices` correctly listed 4 real output
      devices on the dev machine).
- [x] Catch2 wired into CTest; 7 real tests across `mc_core`/`mc_engine`
      (version string, sine-generator silence/amplitude/phase-continuity/
      zero-channel safety, device enumeration never throws) — all passing
      locally via both dependency paths.
- [x] `README.md` created: project description, build instructions (both
      dependency paths), credit to MultichannelPlayer (MIT) per
      `docs/ARCHITECTURE.md` §0, explicit CI-status caveat.
- [x] `CHANGELOG.md` created, `ACKNOWLEDGEMENTS` created crediting
      MultichannelPlayer and Ali Balighi.
- [x] `LICENSE`: GPL-3.0-or-later, verbatim text from gnu.org.

## M1 — Engine core

Goal: `mc_engine` can actually play something — disk-streamed, sample-synced,
routed, metered, offline-renderable.

**Scope adjustment, logged as it was made (project brief §1):** the master
limiter uses *sample* peak in M1, matching the reference, not the true-peak
(4x oversampled) meter the brief's fix-list (§5) calls for. Building the
oversampled true-peak filter now, before `mc_dsp` exists (M3) and before the
loudness-analysis path that will share it exists (M4), would mean writing it
twice. It is upgraded to true peak at M4 when `mc_dsp::Bs1770Meter` is built,
sharing one implementation instead of two. Tracked as an open item below so
it is not silently forgotten.

- [x] Disk streaming: `RingBuffer` (lock-free SPSC, fixed capacity, no
      allocation in `read()`/`write()`), one background reader thread per
      `Voice` filling it from `FileSampleSource` (libsndfile), resampling via
      libsamplerate only when the file's rate differs from the device's —
      when it matches, samples pass through untouched (needed for direct-mode
      bit-exactness, verified by test).
- [x] Sample-synced multi-file playback: verified two ways — (1) an offline
      render test with three identical-content tracks routed to three
      different outputs, asserting every output channel is sample-identical
      across the whole render; (2) real hardware playback via
      `multicue-cli play`, 0 underruns over 6 s with two live voices on a
      real CoreAudio device.
- [x] Direct routing matrix (`GainMatrix`, arbitrary N×M, `identityDirect()`
      builds the common track-i-to-output-i case) + transport
      (`RealtimeAudioEngine`: play/pause/stop/seek). Click-free fades: a
      per-track attack/release `Envelope` for start/stop (shared by the
      offline and realtime paths), plus a short pause/resume transport-gain
      ramp that preserves exact playback position across a pause (verified
      against real hardware: paused and resumed positions line up to within
      the ~30 ms fade window, sample-accurate seek lands within one audio
      buffer of the requested time).
- [x] Metering: `Voice::underrunCount()` / `RealtimeAudioEngine::underrunCount()`
      are plain atomics, incremented without allocation/logging from the
      audio thread; polled from the CLI in the live smoke test above (0
      throughout). A UI-facing 30+ fps snapshot widget is M2/M5 scope (no UI
      consumer exists yet to feed).
- [x] Master bus: `MasterLimiter` — smoothed master gain + a real lookahead
      (sliding-window-maximum, preallocated, allocation-free `process()`)
      brick-wall limiter, *sample*-peak for now (see the scope-adjustment
      note above). Tested: passthrough below ceiling, master gain applied,
      sustained clamp to ceiling, zero overshoot during the attack transient,
      disable-limiter bypass.
- [x] Offline render path (`OfflineRenderer`): drives the identical
      `PieceMixer`/`MasterLimiter` code the realtime engine uses, faster than
      real time, to a WAV file — not a parallel implementation.
- [x] `multicue-cli render` subcommand using the offline path; exercised
      end-to-end with real generated WAV fixtures (`render … --gain -3`),
      output verified with `afinfo`/`afplay`, not just "it compiled".
- [x] Device-loss handling: `RtAudio::setErrorCallback` wired to an atomic
      flag (`RealtimeAudioEngine::deviceWasLost()`), allocation-free so it is
      safe regardless of which thread RtAudio invokes it from. **Not** yet
      wired to an automatic reconnect-on-return or a UI message — that lands
      when there is a UI transport to show it in (M2), tracked as an open
      item below rather than silently assumed.
- [x] Underrun counting, exposed via a snapshot, never crashes — see Metering
      above.
- [x] Engine tests (Catch2, via offline render): direct-mode bit-exactness
      (sustained-region sample equality against the source file), sample-
      aligned multi-voice starts, click-free attack (monotonic ramp from
      silence, no overshoot) — 37 tests total across `mc_core`/`mc_engine`,
      all passing.
- [x] ASan/UBSan: a local `-DMULTICUE_ENABLE_ASAN=ON` build was run against
      the full test suite (clean) *and* against the live
      `multicue-cli play`/pause/resume/seek smoke test on real hardware
      (clean) — the one place sanitizers matter most, since that is where
      the actual background-thread/ring-buffer/stream-teardown code runs.
      Added as a second CI job (macOS + Ubuntu; MSVC's ASan is different
      enough to defer rather than guess at, see the open item below).
      Clang RealtimeSanitizer on the callback itself was not attempted this
      milestone — tracked below, not skipped silently.

## M2 — Playlist mode

Goal: parity with the reference's direct-mode feature set (`docs/
ARCHITECTURE.md` §0 table), as a thin layer over the M1 engine.

**Scope adjustment, logged as made:** source-format auto-detection
(`detectSourceFormat()`, AmbiX/FuMa/layout guessing) is deferred to M3, same
reasoning as `mc_dsp` itself (see M0's note) — direct mode literally does not
use az/el/layout/srcFormat, so adding those fields now would be schema
churn with no behaviour behind it. M2's `Track`/`Piece` carry exactly what
direct mode needs; M3 adds a schema migration (v1→v2) alongside the spatial
math that gives the new fields meaning, which is also the first *real* test
of the migration mechanism built in M2 (see below).

- [x] `mc_core` data model: `Track`, `Piece`, `Playlist`/`PlaylistSettings`;
      JSON (de)serialisation (`PlaylistIO`, nlohmann-json) with
      `schemaVersion` + a migration-step scaffold (empty today, exercised for
      real at M3's v1→v2 migration).
- [x] Mixer per piece: fader/mute/solo per track (UI + `RealtimeAudioEngine`
      wiring, live while playing), piece trim, master fader + per-track and
      per-output peak meters. Peak-hold is UI-side decay (`0.97` per tick),
      matching how the reference itself does peak-hold at its UI layer, not
      inside the audio synth.
- [x] Transport UI: play/pause/stop/prev/next, draggable position bar,
      auto-advance with a configurable gap. **Crossfade between pieces is
      not implemented** — auto-advance still only gaps, same as the
      reference; fixing that (project brief §5) is still open, carried
      forward below rather than silently dropped.
- [x] Playlist editing: add files/folder/folder-of-many-pieces, reorder,
      duplicate, remove, rename (title field), missing-file marking in the
      list. "+ Tracks..." appends to the selected piece (blocked while that
      piece is the one currently loaded, matching the reference).
- [x] Autosave: `AutosaveScheduler` (debounced ~1 s after the last edit, unit
      tested with a fake clock) writes to the current playlist path on a
      30 fps UI timer tick. Crash recovery is implicit rather than a
      separate flow: the app always opens whatever
      `~/Library/Application Support/MultiCue Project/MultiCue/Autosave.mcplist`
      (or platform equivalent) last had on disk, same mechanism the
      reference uses.
- [x] Natural-sort file ordering (`mc_core::naturalSortKey()`), plus
      `guessPieceTitle()` and `expandTracksFromProbedFiles()` (both ported
      from the reference and unit tested) wired into the actual add-files/
      add-folder/add-many-folders flows.
- [ ] Source-format auto-detection: deliberately deferred to M3 (see above).
- [x] Playlist round-trip + rejection tests (bad magic, missing/too-new
      schema version), 24 new tests across `mc_core`/`mc_engine` this
      milestone (65 total). README/CHANGELOG updated.

**Found and fixed during this milestone, before any of it shipped:**
- `PieceMixer`'s gain matrix was originally going to be an
  `std::atomic<std::shared_ptr<const GainMatrix>>` swap (the "obvious"
  C++20 way to do this) -- it does not actually compile under Apple's
  libc++, confirmed by compiling a two-line reproduction before writing 70
  lines of code around a design that would never build. Replaced with a
  flat `std::vector<std::atomic<float>>`, one genuinely lock-free cell per
  source×output pair, needed anyway for the UI's live "-> out N" per-track
  reassignment control.
- `RealtimeAudioEngine::stop()` blocked the calling thread for up to 500 ms
  waiting out the release fade -- fine for the M1 CLI, but would have
  frozen the GUI's Stop button for that long on every press. Split into a
  non-blocking `requestStop()` + `isStopFadeComplete()` pair that the UI's
  existing poll timer drives (mirroring how auto-advance's gap countdown
  already worked), keeping the blocking `stop()` only for the CLI's
  synchronous convenience.
- The gap countdown's first implementation measured elapsed time with a
  function-local `static` "last tick" variable that was only ever
  initialized once for the process's whole lifetime, not reset per gap --
  so every gap after the first would have read a stale timestamp and fired
  instantly. Fixed by computing an absolute deadline once when the gap
  begins and comparing against "now" every tick, which has no
  accumulation/staleness failure mode to get wrong.
- `QScrollArea::setWidget()`'s ownership of the widget it replaces was not
  something to guess at from memory; `takeWidget()` + explicit
  `deleteLater()` before every `setWidget()` call removes the ambiguity
  entirely regardless of the actual Qt6 behaviour.

## M3 — Spatial

Goal: every listening format from the reference, plus the fixes from §5.

- [ ] `mc_dsp`: spherical harmonics (ACN/SN3D, order 1–7), max-rE weights,
      Fibonacci-sphere sampling — unit tested against closed-form values.
- [ ] Layout presets (ring ×2, pairs, ITU standard incl. 7.1.4, two rings,
      dome, sphere, custom) — unit tested against hand-computed angles.
- [ ] "My speakers" editor + speaker layout file format
      (`docs/ARCHITECTURE.md` §6.3), independent of the show file.
- [ ] Distance compensation: per-output delay + gain, manual override (new
      vs. reference, §5).
- [ ] VBAP with imaginary speakers (convex hull + 70°-threshold candidate
      directions, ported from the reference) — gain-sum/energy tests.
- [ ] AllRAD (3D layouts) and 2D max-rE decoder (horizontal layouts) —
      energy-normalisation tests.
- [ ] Stereo downmix (constant-power, rear fold) — ported formula, unit
      tested against the reference's table of expected gains at a few
      azimuths.
- [ ] Ambisonic B-format out (AmbiX, order Auto/1–7).
- [ ] FuMa↔AmbiX conversion, unit tested round-trip.
- [ ] Binaural: spherical-head model (Brown & Duda, ported) as the fallback;
      SOFA loading via libmysofa as the primary path (version/license check
      logged before pinning); optional room reverb; OSC head tracking
      (yaw/pitch/roll rotating the source field) — all fixing reference gaps
      per §5.
- [ ] Seamless vs. crossfade format switching (`docs/ARCHITECTURE.md` §4.2).
- [ ] Configurable binaural/stereo start channel (fixing reference's
      fixed-output-1 limitation per §5).
- [ ] Top-view + elevation map UI showing speakers and the selected cue's
      tracks.
- [ ] Full test suite from `docs/ARCHITECTURE.md` §9's spatial bullet.

## M4 — Loudness

- [ ] `mc_dsp::Bs1770Meter`: K-weighting, 400 ms/75%-overlap gated loudness,
      LRA, true peak (4× oversampled) — compliance-tested against EBU Tech
      3341/3342 reference signals.
- [ ] Per-file analysis cache (keyed on path+mtime+size, invalidated on
      change), multithreaded, faster than real time — reusing the M1 offline
      render path rather than a parallel implementation
      (`docs/ARCHITECTURE.md` §0, §3.7).
- [ ] Auto-level: target (Auto / −16 / −18 / −20 / −23 / −27 LUFS), peak
      ceiling, optional ±9 dB intra-piece balancing, undo — ported logic from
      the reference's `autoLevel`/`balancePiece`.
- [ ] Speaker test: pink/band-limited-pink/white/sine, RMS dBFS level,
      single/all/cycle, meters.
- [ ] `multicue-cli analyze-loudness` subcommand.

## M5 — Cue engine

- [ ] `mc_core`: workspaces, cue lists, cues (number/name/notes/color), full
      cue-type set (Media: Audio/Video/Image; Group: start-all/start-first/
      playlist/random; Fade; Control: Start/Stop/Pause/Load/Reset/Goto/Wait/
      Panic; Memo/OSC/MIDI).
- [ ] `mc_engine::CueRuntime`: pre/post-wait, continue modes, loops/slices,
      start/end trim, per-cue fade in/out, preloading for instant GO.
- [ ] QLab-style cue-list UI: GO button, standby display, playhead, Space=GO,
      Esc=panic (double-Esc=hard stop), Active Cues panel with per-cue
      pause/stop.
- [ ] Edit mode / Show mode lock.
- [ ] Inspector tabs: Basics; Triggers (hotkey/MIDI/OSC/timecode — stubs OK
      until M7 wires the transports); Levels & routing (crosspoint matrix in
      direct, az/el/distance in spatial); Time & loops; Video geometry (stub
      until M6).
- [ ] Spatial automation: Fade cues moving az/el/distance over time.
- [ ] Full undo/redo for every edit.
- [ ] Playlist mode re-verified as a restricted cue-list shape, no
      behavioural regression (`docs/ARCHITECTURE.md` §6.2).

## M6 — Video

- [ ] `mc_video`: FFmpeg decode (H.264/HEVC/ProRes/VP9/AV1, image sequences,
      stills) with hardware acceleration (VideoToolbox/D3D11VA/VAAPI/NVDEC).
- [ ] Presentation via Qt (QRhi preferred, OpenGL fallback) to any connected
      screen, fullscreen or windowed; multiple outputs, layered surfaces
      (z-order/opacity/position/scale/rotation/fades).
- [ ] Video cue audio: embedded stream and/or attached external mono files,
      both as ordinary `Voice`s on the shared sample-accurate clock.
- [ ] A/V sync: present-against-audio-clock with drift correction, per-output
      latency offset, on-screen diagnostics (A/V offset, dropped/repeated
      frames).
- [ ] Output monitoring: preview tiles in main window; black/test-pattern
      when idle.
- [ ] Verify audio-only shows still run with zero GPU/video dependency
      (`docs/ARCHITECTURE.md` §7).

## M7 — Control

- [ ] `mc_control`: OSC server + documented address space (go/stop/panic/
      select/fire-by-number/set-levels/set-positions/transport), OSC
      feedback out.
- [ ] MIDI: note/CC/program-change triggers, MIDI Show Control.
- [ ] Timecode: LTC/MTC chase for cue lists.
- [ ] Head tracking wired from OSC into the M3 binaural rotation.
- [ ] `multicue-cli`: device list / loudness analyse / validate-show /
      render — confirmed to share the same command path as OSC/MIDI/GUI
      (`docs/ARCHITECTURE.md` §8).

## M8 — Release quality

- [ ] Relink missing media; Collect show (copy show + dedup media to one
      folder, paths rewritten relative).
- [ ] Crash recovery verified end-to-end (kill -9 during a show, relaunch).
- [ ] Diagnostics window: CPU load, xruns, disk buffer health, decoder
      status.
- [ ] Preferences, recent shows, per-show vs. global settings.
- [ ] Rotating log files.
- [ ] User manual in `docs/`.
- [ ] Installers: DMG (signing/notarization-ready), MSI or NSIS, AppImage or
      Flatpak.
- [ ] Full performance-target pass (brief §6): 128 mono streams @ 48 kHz /
      64 outputs / 128-sample buffer with no dropouts on a mid-range laptop;
      preloaded-cue GO latency ≤ one buffer + device latency; 4K60 HW-decoded
      video concurrent with that audio load; UI responsive (30+ fps meters)
      during analysis and playback. Each target gets a reproducible benchmark
      committed under `tests/` or `tools/bench/`, not just a one-off manual
      check.
- [ ] Version 1.0 tag.

## Open items (carried forward, not yet done)

- **M0 CI is unverified on Windows and Ubuntu.** `.github/workflows/ci.yml`
  is written and its mechanism (vcpkg manifest install + CMake config) was
  validated locally on macOS against both the Homebrew/pkg-config path and
  the actual vcpkg/CMake-config path, but no GitHub Actions run has happened
  because this repository has intentionally not been pushed to a remote.
  Close this item the first time the repo is pushed and the workflow runs
  green on all three OSes.
- **M0's "fully keyboard-operable" and "HiDPI" bullets are asserted, not
  independently tested.** Qt's defaults cover both, and the window was
  launched and exercised at the process level, but no one has tabbed through
  every control by hand or checked it on a HiDPI display yet.
- RtAudio's Linux vcpkg feature set (`alsa`, `pulse`) is specified in
  `vcpkg.json` and matches documented vcpkg manifest syntax, but has not
  been built on an actual Linux machine (none available in this
  environment) — first real signal comes from the first Ubuntu CI run, per
  the item above.
- **Master limiter is sample-peak, not true-peak.** Deliberate M1 scope cut
  (see M1's note above) — upgrade at M4 alongside `mc_dsp::Bs1770Meter`,
  sharing one oversampling implementation instead of writing it twice.
- **Still no automatic reconnect or UI message when `deviceWasLost()` fires,
  even though M2's transport UI now exists.** Carried forward again rather
  than silently dropped — the M2 UI work didn't get to it; genuinely next.
- **Windows has not run the engine at all.** Everything in M1 was built,
  tested (including under ASan/UBSan), and run against real audio hardware
  only on macOS (the only machine available in this environment). The CI
  workflow covers Windows in principle; nothing has confirmed RtAudio's
  WASAPI backend, MSVC warnings-as-errors, or the vcpkg Windows triplet
  actually work until that CI run happens (see the M0 CI item above, which
  this depends on).
- **Clang RealtimeSanitizer was not attempted on the audio callback.** Said
  at the end of M1 this would happen "at the start of M2" and it didn't --
  logged here again rather than quietly re-deferred a second time. ASan/
  UBSan were run (clean, including live hardware) and cover a lot of the
  same ground, but RealtimeSanitizer's allocation/syscall/lock checks
  *inside* the callback are a more targeted signal the brief explicitly
  asks for (§7).
- **`RealtimeAudioEngine`'s pause/resume/seek paths are verified by one
  manual scripted run against real hardware** (`multicue-cli play`'s
  built-in pause→resume→seek sequence, positions checked by eye against the
  expected arithmetic), not by an automated test — a real RtAudio device
  isn't available/deterministic in CI, so an automated equivalent (if one is
  worth building) would need to mock the device layer, which hasn't been
  attempted yet.
- **No crossfade between pieces on auto-advance.** Project brief §5
  explicitly lists this as a reference limitation to fix; M2 only ported the
  reference's gap behaviour, not the fix. Open.
- **The M2 playlist/mixer UI (`MainWindow`) has no automated test
  coverage.** It was built, then manually re-read end to end looking for
  bugs (three were found and fixed this way: the gain-matrix thread-safety
  issue, the blocking Stop button, and a stale-timestamp bug in the gap
  countdown — see M2's "found and fixed" list above), and launched to
  confirm it starts/stops cleanly. It was not exercised by clicking through
  it by hand, and there is no automated UI test (Qt's private-slot methods
  are reachable via `QMetaObject::invokeMethod` for this kind of testing,
  but the file-picker dialogs in the way of the add-piece flows make that
  nontrivial; not attempted this milestone).
- **Scrubbing the position bar while fully stopped doesn't stick.** Dragging
  it updates the displayed value but the next Play still starts from 0,
  since nothing currently remembers "seek to here" independent of an
  active/paused engine session. The reference keeps this working while
  stopped; minor, not fixed yet.
