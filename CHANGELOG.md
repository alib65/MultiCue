# Changelog

All notable changes to this project are documented in this file.

## [Unreleased] -- M0 skeleton in progress

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
