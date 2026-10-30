# MultiCue

https://github.com/alib65/MultiCue

Ali Balighi © 2026
alibalighi.com

MultiCue is a cue-based, multichannel audio and video show player for
theatre, concerts, installations and electroacoustic music: QLab-class show
control with first-class support for multichannel speaker feeds made of
separate mono files (4, 8, 16, 64+ channels), spatial audio output (VBAP,
Ambisonics/AllRAD, binaural with SOFA HRTFs, stereo downmix, B-format), and
synchronized video. "MultiCue" is a working name.

It is free software, licensed GPL-3.0-or-later.

The project is pre-alpha: milestones **M0 (skeleton)**, **M1 (engine core)**
and **M2 (playlist mode)** are substantially complete on macOS, and **M3
(spatial)**'s math core exists and is tested but not yet wired into the app
(see `docs/ROADMAP.md` for the exact checklist, including what is *not* yet
verified -- notably Windows and Linux, which have not run at all in this
environment, and the playlist UI, which has no automated test coverage).
`docs/ARCHITECTURE.md` has the design.

## Status right now

- `mc_core`: application identity constants; the playlist data model
  (`Track`, `Piece`, `Playlist`), JSON persistence with a schema-version +
  migration scaffold, natural-sort file ordering, autosave scheduling. The
  full cue-list show file arrives in M5.
- `mc_engine`: device enumeration; disk streaming (`Voice`, lock-free
  `RingBuffer`, on-the-fly resampling that is bypassed entirely when rates
  match, so direct mode is bit-exact); a direct-mode routing matrix
  (`GainMatrix`) with live per-track output reassignment; click-free fades
  (`Envelope`) for start/stop and for pause/resume/seek; a lookahead master
  limiter (`MasterLimiter`, sample-peak for now -- true peak arrives at M4);
  per-track/per-output peak+RMS metering; the shared mixing core used by
  both the realtime engine and the offline renderer (`PieceMixer`); and
  `RealtimeAudioEngine` (live load/play/pause/stop/seek against a real audio
  device, with device-loss detection). The cue runtime and spatial/
  listening-format math arrive in M3/M5.
- `multicue`: a Qt 6 Widgets Playlist-mode window, dark by default -- add
  pieces from files/folders, reorder/duplicate/remove, a per-piece mixer
  (fader/mute/solo/output-routing per track, live while playing), transport
  with a draggable position bar and auto-advance, master fader + limiter,
  level meters, autosave, open/save-as.
- `multicue-cli`: `list-devices`, `version`, `render` (direct-mode offline
  mix to a WAV file), `play` (live playback for manual testing, including a
  built-in pause/resume/seek sequence).
- `mc_dsp`: spherical harmonics, VBAP, AllRAD/2D Ambisonic decoders, stereo
  downmix, FuMa<->AmbiX conversion, layout presets, and the spherical-head
  binaural model's filter coefficients -- all ported from the reference and
  unit tested. Not yet wired into a live listening format in the app; see
  M3 in `docs/ROADMAP.md`.

`docs/ROADMAP.md` tracks every open item explicitly, including things that
look done but are only manually/locally verified rather than proven in CI --
nothing here is presented as more finished than it is.

## Building

Requirements: CMake >= 3.25, a C++20 compiler, Qt 6 (Widgets), RtAudio,
Catch2 v3.

### macOS (Homebrew)

```sh
brew install qt rtaudio catch2 libsndfile libsamplerate nlohmann-json
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If Anaconda/conda is on your `PATH`, CMake may find its (older) copy of
nlohmann-json instead of Homebrew's; if `cmake` output says it found a
version other than the one in `docs/ROADMAP.md`'s dependency log, pin it
explicitly: add `-Dnlohmann_json_DIR="$(brew --prefix nlohmann-json)/share/cmake/nlohmann_json"`.

### Any platform, via the vcpkg manifest

The repository ships a `vcpkg.json` manifest so dependencies can be built
reproducibly instead of relying on whatever a system package manager has.
This is the path CI uses (`.github/workflows/ci.yml`).

```sh
git clone https://github.com/microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh    # bootstrap-vcpkg.bat on Windows
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-osx   # or x64-windows / x64-linux
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Qt itself is not in the vcpkg manifest (it is large and best installed as a
prebuilt SDK); install it separately and pass its CMake package directory,
e.g. `-DQt6_DIR=/path/to/Qt/6.11.3/<arch>/lib/cmake/Qt6`.

CI status: the GitHub Actions workflow builds and tests on macOS, Windows
and Ubuntu on every push, but as of this writing it has not yet been run
against GitHub (the project has not been pushed to a remote). Treat the
Windows/Linux legs as written-but-unverified until the first real CI run.

## Running

```sh
./build/apps/multicue-cli/multicue-cli list-devices
./build/apps/multicue-cli/multicue-cli render out.wav track1.wav track2.wav --gain -3
./build/apps/multicue-cli/multicue-cli play <device_id> track1.wav track2.wav --seconds 5
./build/apps/multicue/multicue
```

## Credits

The spatial-audio and loudness algorithms are ported from
[MultichannelPlayer](https://github.com/alib65/MultichannelPlayer) (MIT
license) by Ali Balighi -- see `ACKNOWLEDGEMENTS` and `docs/ARCHITECTURE.md`
section 0 for the detailed mapping.

## License

GPL-3.0-or-later. See `LICENSE`.
