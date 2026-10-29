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

Dependencies not yet added (libsndfile, FFmpeg, libsamplerate/r8brain-free,
pffft, libmysofa, liblo/oscpack, RtMidi) get their own row here at the
milestone that first links them (M1, M3, M4, M6, M7 respectively), not
before — see each milestone's checklist below.

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

- [ ] Disk streaming: ring buffer per voice, reader thread pool, resampling
      when a file's sample rate differs from the device (libsamplerate or
      r8brain-free — version/license check logged before pinning).
- [ ] Sample-synced multi-file playback: N mono voices of one piece start on
      the same frame and stay locked for the piece's duration (test: offline
      render, assert zero inter-voice sample offset throughout).
- [ ] Direct routing matrix (crosspoint, arbitrary N×M) + transport
      (play/pause/stop/seek) with click-free fades on every transition
      (`docs/ARCHITECTURE.md` §3.4).
- [ ] Metering: per-voice and per-output peak/RMS snapshots to the UI at
      30+ fps without allocation on the audio thread.
- [ ] Master bus: gain, true-peak limiter (`mc_dsp::TruePeakMeter`-backed,
      fixing the reference's sample-peak-only limiter per §5).
- [ ] Offline render path (`docs/ARCHITECTURE.md` §3.7): same DSP, faster
      than real time, to a sound file.
- [ ] `multicue-cli render` subcommand using the offline path.
- [ ] Device-loss/change handling: show stays open, status message, recovery
      on device return (`docs/ARCHITECTURE.md` §3.8) — tested by simulating a
      stream-error callback, not just by hand.
- [ ] Underrun counting, exposed via a snapshot, never crashes.
- [ ] Engine tests (Catch2, via offline render): direct-mode bit-exactness,
      sample-aligned starts, click-free fades/seeks (§7 of the brief).
- [ ] ASan/UBSan in CI for all new test binaries; attempt Clang
      RealtimeSanitizer on the audio callback, note in CHANGELOG if the
      toolchain doesn't support it yet rather than skipping silently.

## M2 — Playlist mode

Goal: parity with the reference's direct-mode feature set (`docs/
ARCHITECTURE.md` §0 table), as a thin layer over the M1 engine.

- [ ] `mc_core` data model: piece, track, playlist; show-file JSON
      (de)serialisation with `schemaVersion` + migration table scaffold
      (even if there's only one version so far).
- [ ] Mixer per piece: fader/mute/solo/meter/peak-hold per track, piece trim,
      master fader + output meters (UI + engine wiring).
- [ ] Transport UI: play/pause/stop/prev/next, draggable position bar,
      auto-advance with configurable gap **and** crossfade (fixing the
      reference's gap-only limitation per §5).
- [ ] Playlist editing: add files/folder/folder-of-folders, reorder,
      duplicate, rename, missing-file ⚠ marking.
- [ ] Autosave (~1 s after last edit, matching the reference's cadence) +
      crash recovery on next launch.
- [ ] Natural-sort file ordering (`mc_core::naturalSortKey()`, ported per
      `docs/ARCHITECTURE.md` §0).
- [ ] Source-format auto-detection from filenames (`detectSourceFormat()`).
- [ ] Show-file round-trip tests; README/CHANGELOG updated with Playlist
      mode usage.

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
