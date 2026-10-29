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
| *(none yet — first check happens in M0)* | | | | |

## M0 — Skeleton

Goal: a repo that builds and boots on all three OSes, with a device list and a
test tone — the minimum slice that proves the toolchain, CI, and real-time
audio path all work together.

- [ ] Root `CMakeLists.txt`: project name/version from `AppInfo.h` pattern
      (`docs/ARCHITECTURE.md` intro), C++20, targets for `mc_core`, `mc_dsp`,
      `mc_engine`, `multicue`, `multicue-cli` (empty/stub bodies acceptable
      here; `mc_video`/`mc_control` stubs can wait for M6/M7).
- [ ] vcpkg manifest (`vcpkg.json`) or FetchContent setup for: RtAudio,
      libsndfile, Catch2 v3 (first dependencies actually needed). Each gets a
      version/license check logged in the table above before being pinned.
- [ ] GitHub Actions CI: macOS (universal), Windows x64, Ubuntu — build +
      test matrix, no warnings treated as the pass bar.
- [ ] `mc_engine`: RtAudio device enumeration + open/close, a single test-tone
      voice to a chosen output, sample-accurate start scaffolding (even if
      only one voice exists yet).
- [ ] `multicue`: Qt 6 Widgets app window, dark theme default, device picker,
      "play test tone" button. HiDPI and full keyboard operability checked
      manually.
- [ ] `multicue-cli`: `list-devices` subcommand.
- [ ] Catch2 wired into CTake/CI; one real test (test-tone buffer content) to
      prove the harness works end to end.
- [ ] `README.md` created: project description, build instructions, credit to
      MultichannelPlayer (MIT) per `docs/ARCHITECTURE.md` §0.
- [ ] `CHANGELOG.md` created, `ACKNOWLEDGEMENTS` created crediting
      MultichannelPlayer and Ali Balighi.
- [ ] `LICENSE`: GPL-3.0-or-later.

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

*(empty — populated as milestones land short of full scope)*
