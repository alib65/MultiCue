# MultiCue

MultiCue is a cue-based, multichannel audio and video show player for
theatre, concerts, installations and electroacoustic music: QLab-class show
control with first-class support for multichannel speaker feeds made of
separate mono files (4, 8, 16, 64+ channels), spatial audio output (VBAP,
Ambisonics/AllRAD, binaural with SOFA HRTFs, stereo downmix, B-format), and
synchronized video. "MultiCue" is a working name.

It is free software, licensed GPL-3.0-or-later.

The project is pre-alpha: milestone **M0 (skeleton)** is in progress. See
`docs/ROADMAP.md` for what exists today versus what is still open, and
`docs/ARCHITECTURE.md` for the design.

## Status right now

- `mc_core`: application identity constants (`AppInfo.h`), a `versionString()`
  helper. The show-file data model arrives in M2.
- `mc_engine`: audio output device enumeration (RtAudio) and a single-channel
  test-tone generator, following the real-time audio-thread rules from
  `docs/ARCHITECTURE.md` from day one (no allocation/locks/I/O in the audio
  callback). Disk streaming, the cue runtime and the gain-matrix router
  arrive in M1.
- `multicue`: a Qt 6 Widgets window to pick a device/channel and play the
  test tone.
- `multicue-cli`: `list-devices` and `version` subcommands.

Nothing beyond this is implemented yet; `docs/ROADMAP.md` tracks every open
item explicitly rather than presenting stubs as finished features.

## Building

Requirements: CMake >= 3.25, a C++20 compiler, Qt 6 (Widgets), RtAudio,
Catch2 v3.

### macOS (Homebrew)

```sh
brew install qt rtaudio catch2
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j
ctest --test-dir build --output-on-failure
```

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
./build/apps/multicue/multicue
```

## Credits

The spatial-audio and loudness algorithms are ported from
[MultichannelPlayer](https://github.com/alib65/MultichannelPlayer) (MIT
license) by Ali Balighi -- see `ACKNOWLEDGEMENTS` and `docs/ARCHITECTURE.md`
section 0 for the detailed mapping.

## License

GPL-3.0-or-later. See `LICENSE`.
