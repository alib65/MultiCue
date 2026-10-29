# MultiCue — Architecture

"MultiCue" is a working name for a cue-based, multichannel audio and video show
player: QLab-class functionality with first-class support for speaker feeds made
of separate mono files, spatial audio decoding, and synchronized video. This
document is the architecture reference. It is written before any code exists and
must be approved before implementation starts (see `docs/ROADMAP.md` for the
milestone plan).

The project name, license string and version are centralized in one place so
renaming is cheap:
- CMake: `project(MultiCue VERSION 0.1.0 LANGUAGES CXX)` at the top of the root
  `CMakeLists.txt`.
- `mc_core/include/mc/core/AppInfo.h`: `kAppName`, `kAppOrganization`,
  `kAppVersion`, `kShowFileMagic` constants. Every window title, show-file
  magic string, settings path and log header reads from here — never a literal
  string.

## 0. Reference implementation

The audio side of this app is a direct, deliberate port of
[**MultichannelPlayer**](https://github.com/alib65/MultichannelPlayer) (MIT
licensed), a SuperCollider playlist player for multichannel music by Ali
Balighi. It is cloned and read in full (`README.md` and
`MultichannelPlayer.scd`, 2491 lines) as the functional and algorithmic spec
for everything in §8 below. It is credited in `README.md` and
`ACKNOWLEDGEMENTS` once those files exist (M2).

### What is ported, and where it lands

| Reference (SuperCollider) | Ported to (C++) | Notes |
|---|---|---|
| `layoutPreset` | `mc_dsp::layoutPreset()` | Ring-front-left, ring-front-centre, pairs, ITU standard, two rings, dome, sphere, custom. Exact angle formulas kept. |
| `shSN3D` (recursive Legendre, ACN/SN3D real SH) | `mc_dsp::sphericalHarmonicsSN3D()` | Same recursion (stable to high order); extended from order 5 to order 7 (§5). |
| `maxReWeights` | `mc_dsp::maxReWeights()` | Same closed-form max-rE angle. |
| `fibSphere` | `mc_dsp::fibonacciSphere()` | Same Fibonacci-spiral sampling, used for AllRAD virtual speakers and energy normalisation test grids. |
| `vbapSetup` / `vbapGains` (convex hull + imaginary speakers) | `mc_dsp::VbapSetup`, `mc_dsp::vbapGains()` | Same triangulation algorithm: candidate directions at the 26 cube-corner/edge/face unit vectors, kept if more than 70° from every real speaker; brute-force O(n³) facet test (n is speaker count, always small). Imaginary-speaker gains are discarded and the result renormalised to unit power, exactly as the reference does. |
| `decoder2D` / `allradDecoder` | `mc_dsp::decode2D()`, `mc_dsp::allRadDecoder()` | 2D max-rE pairwise-panned decoder for horizontal layouts (all speakers within ±10° elevation); AllRAD (Zotter & Frank) for 3D layouts: sample to 240 Fibonacci points with max-rE weights, VBAP those to the real speakers, normalise for unit average energy over a 200-point test grid. |
| `virtualDecoder` | `mc_dsp::virtualSpeakerDecoder()` | Used to decode Ambisonic *pieces* to a small virtual-speaker array for binaural/stereo rendering. |
| `binCoefs` / `binArgs` (Brown & Duda spherical-head model) | `mc_dsp::SphericalHeadHrtf` | ITD (path-length delay model) + two first-order shelf/shadow filters per ear + a shared front/back high-frequency shelf. Kept as the **fallback** HRTF; SOFA/measured HRTFs are added as the preferred path (§5, §8 M3). |
| `stereoGains` (constant-power downmix, rear fold) | `mc_dsp::stereoDownmixGains()` | Same formula: full left/right at ±30°, rear sources folded to the front and attenuated by `1 − 0.16·rear`. |
| `fumaMap` (FuMa → ACN/SN3D) | `mc_dsp::fumaToAmbixGain()` | Same channel/gain table, extended because order is no longer capped at 3 for the AmbiX path (FuMa itself is still capped at 3rd order per its own definition). |
| `gatedLoudness`, `analyzeFile` (NRT render → K-weighted mean square + peak) | `mc_dsp::Bs1770Meter` + `mc_engine` offline render path | Same two-stage gating (−70 LUFS absolute, −10 LU relative) over 400 ms blocks at 75% overlap. The offline NRT scsynth process is replaced by `mc_engine`'s own offline-render path (§3.7) running the real production DSP, which is simpler, is tested once, and gives true peak for free. |
| `pieceStats` / `autoLevel` / `balancePiece` | `mc_dsp` loudness functions + `mc_engine::AutoLevelJob` | Same target-selection logic (Auto = loudest level every piece can reach under the ceiling; fixed LUFS targets 16/18/20/23/27; optional ±9 dB intra-piece balancing). |
| `detectSource` (filename sniffing) | `mc_core::detectSourceFormat()` | Same keyword rules (`ambix`/`acn`/`foa`/`hoa`, `fuma`/`bformat`/`wxyz`, `lfe`, square file counts). |
| `natKey` (natural sort) | `mc_core::naturalSortKey()` | Same digit-run zero-padding approach. |
| Track synth (`DiskIn`/`VDiskIn`, fixed gain vector, `SendPeakRMS`) | `mc_engine::Voice` | One voice per mono track; see §3.4. |
| `mcpMaster` (limiter, metering) | `mc_engine::MasterBus` | True-peak limiter replaces the sample-peak `Limiter.ar` (§5, §8 M1). |
| Speaker test synths | `mc_engine::SpeakerTestSignal` | Same signal set and RMS calibration convention. |
| Scripting interface (`~multichannelPlayer[...]`) | `multicue-cli` + OSC address space | Same verbs, exposed as CLI subcommands and OSC methods instead of a SuperCollider environment variable (§3.5, §4.8). |

The reference's "Known limitations" (its own README section, duplicated in
`docs/ROADMAP.md`) are tracked as explicit, numbered fixes — see §5.

## 1. Goals and non-goals

- Goal: a professional, cross-platform (macOS/Windows/Linux) cue-based show
  controller, audio-first, video-capable, GPU-optional.
- Goal: the engine (`mc_core`/`mc_dsp`/`mc_engine`) runs headless, with no Qt
  dependency, so it is testable in CI without a display and reusable by
  `multicue-cli`.
- Non-goal (for now): a full NLE-style video editor. Video is a presentation
  and sync layer, not an editor.
- Non-goal: real-time collaborative editing of a single show by multiple users.

## 2. Build targets

```
mc_core     data model, show file (de)serialisation + migrations, undo/redo,
            preferences, playlist/cue-list domain objects. No audio, no Qt.
mc_dsp      pure, allocation-aware-but-not-RT-constrained functions: spatial
            math (SH, VBAP, AllRAD, binaural filters, downmix), BS.1770
            loudness, biquad/FIR filters, resampling helpers. Exhaustively
            unit tested. No I/O.
mc_engine   the real-time audio engine: device I/O via RtAudio, disk
            streaming, the cue runtime (scheduling, timing, transport),
            the gain-matrix router, the offline/NRT render path. Depends on
            mc_core (data model) and mc_dsp (math), not on Qt.
mc_video    FFmpeg-based decode + presentation (via Qt RHI/OpenGL surfaces
            supplied by the app), A/V sync against the engine's clock.
mc_control  OSC server/client (liblo or oscpack), MIDI (RtMidi) incl. MSC,
            LTC/MTC timecode chase. Talks to mc_engine/mc_core through the
            same command queues the UI uses — it is just another client.
multicue    the Qt 6 Widgets application: playlist mode, cue list mode,
            inspector, speaker/map editors, meters, preferences, autosave,
            crash recovery, diagnostics window.
multicue-cli headless tool: device listing, loudness analysis, show
            validation, render-to-file (bounce) in any listening format.
```

Dependency direction is strictly top-to-bottom in the list above; nothing in
`mc_core`/`mc_dsp`/`mc_engine` includes a Qt header. `mc_control` and
`mc_video` depend on `mc_core`/`mc_engine` but not on `multicue`, so
`multicue-cli` can exercise OSC/MIDI and video-less rendering without Qt.

## 3. Real-time audio engine

### 3.1 Threads

| Thread | Responsibilities | Rules |
|---|---|---|
| **Audio callback** (RtAudio) | Pull audio from each active `Voice`'s ring buffer, apply per-voice and per-output gain with sample-accurate smoothing, sum into the output buffer, run the master limiter, write meters to atomics. | No allocation, no locks, no file I/O, no logging, no system calls, no exceptions crossing it, no dynamic dispatch surprises (vtables are fine; `new`/`delete`/`malloc` are not). Bounded-time only. |
| **Disk reader threads** (one pool, workers pulled per open file) | Fill each track's ring buffer from `libsndfile`/FFmpeg demux, resample on the fly if the file's rate differs from the device rate. | Allowed to block, allocate, and log. Never touch audio-thread state directly — only through the ring buffer and lock-free flags. |
| **UI thread** (Qt main thread) | Widgets, inspector, drag/drop, undo/redo commands, autosave scheduling. | Talks to the engine only through command queues; reads engine state only through snapshots/atomics, never by reaching into engine objects. |
| **Control thread(s)** (`mc_control`) | OSC/MIDI/timecode I/O. | Same discipline as the UI thread: commands in, snapshots out. |
| **Analysis thread pool** | BS.1770 loudness analysis, offline render for Bounce. | CPU-bound, not RT; parallel across files (reference mirrors this exactly, running three files at a time — default pool size follows `std::thread::hardware_concurrency()` clamped to a sane range). |
| **Video decode threads** | One decode thread per actively-playing video cue; hands decoded frames to the presentation layer via a frame queue. | Presents against the audio clock (§3.6), not its own wall clock. |

### 3.2 Thread communication

- **UI/control → engine (commands):** lock-free SPSC queues, one per producer
  thread writing into the engine, carrying small POD command structs (`Go`,
  `Stop`, `SetTrackGain`, `SwapRoutingMatrix`, `Seek`, …). The audio thread
  drains its queue at the top of each callback, applies commands immediately
  or schedules them for a specific frame (sample-accurate start, §3.6).
- **Engine → UI (state):** double-buffered snapshots (meters, playhead
  position, cue status, xrun counters) published with a sequence-lock or
  atomic pointer swap; the UI thread copies out at its own refresh rate
  (30+ fps). No queue needed here — the UI only ever wants the *latest* state,
  never a history, so single-slot atomic publish is simpler and allocation-free
  on both ends.
- **Engine → analysis/video (frames, buffers):** SPSC ring buffers per stream,
  sized generously (the reference's `diskBufFrames = 131072` samples per
  voice is the starting point, tuned per format in M1).

### 3.3 Routing: the gain matrix

Every listening format (direct, Ambisonics, VBAP, binaural, stereo, B-format)
reduces, exactly as in the reference, to a **gain matrix** computed up front in
non-RT code (`mc_dsp`) from the piece's track positions/ACN assignment and the
venue's speaker layout. `mc_engine::Router` holds the *current* matrix and a
*pending* matrix; swapping is a single atomic pointer exchange performed
between audio callbacks, and every coefficient is approached through a
per-sample (or per-block, whichever profiling prefers) linear smoother so a
format change, a fader move, or a `My speakers` edit never zippers or clicks.
This mirrors the reference's `Lag.kr(gains, 0.05)` on the server. Binaural adds
a small, fixed set of per-ear IIR filters (ITD delay + two one-pole
shelves) driven by the same matrix-swap mechanism, operating on the
Ambisonic-decoded or direct virtual-source signal.

### 3.4 Voices and disk streaming

Each mono track (one file, or one channel of an interleaved file, or one
stream of a video's embedded audio) is a `Voice`: a ring buffer owned by the
audio thread for reading and by a reader thread for writing, plus resampler
state (libsamplerate or r8brain-free) used only when the file's native rate
differs from the device rate. `Voice` start/stop/pause/seek always ride a
short asymmetric envelope (fast-ish attack, ~80 ms release, matching the
reference's `Env.asr(fadeIn, 1, 0.08)`) so transport actions are click-free.
Preloading a cue means: open the file(s), allocate ring buffers, prefill them,
and park the voices gated-closed — so `GO` only has to flip the gate and start
the sample-accurate clock, meeting the "one buffer + device latency" target
(§6 of the brief).

### 3.5 Cues, groups, and the runtime

`mc_core` holds the declarative model (cue lists, cues, fades, groups,
triggers). `mc_engine::CueRuntime` is what actually walks it: it turns a `GO`
into a set of `Voice`/`Router` commands with a target start frame, handles
pre-wait/post-wait, continue modes (none/auto-continue/auto-follow),
loop/slice/trim, and fade curves. Group cue types (start-all, start-first,
playlist, random) are runtime compositions over the same primitives, not
special-cased in the audio thread. Playlist mode (§4.4 of the project brief)
is the simplest possible cue list: one audio cue per piece, auto-continue, and
a single auto-level/mixer view bolted on top — it is *not* a separate engine
path, which keeps parity between Playlist mode and Cue mode free.

### 3.6 Clock and synchronization

The **audio device clock** (the sample counter the callback is driven by) is
the single master clock for the whole application. A `GO` command carries a
target start frame a few callbacks in the future (the reference's
`s.makeBundle(0.1, …)` lookahead, adapted to a lock-free scheduled-command
model instead of OSC bundles); every voice of that cue begins at exactly that
frame, so multi-file pieces stay sample-locked for their entire duration
(there is no per-voice drift correction needed once started, only resampler
drift *within* a voice when its native rate differs from the device — handled
by the continuous resampler, not by periodic re-sync). Video frames are
presented by comparing their PTS, translated into device-clock time via the
same frame counter, against "now"; `mc_video` corrects drift by frame
drop/repeat and exposes the measured A/V offset for the diagnostics overlay
(§4.6, §8 M6). Device loss/change re-opens RtAudio against the same logical
clock position where possible, or pauses the transport and shows a clear
message — it never resets cue state (§3.8).

### 3.7 Offline rendering

`mc_engine` has a second entry point that drives the *same* voice/router/DSP
code faster than real time into a sink that is a sound file writer instead of
an audio device. This path is used for: BS.1770 analysis (replacing the
reference's per-file NRT scsynth process with one pass through the production
DSP), the CLI `render` subcommand ("bounce"), and the engine's own offline
correctness tests (bit-exactness, sample-aligned starts, click detection).
Driving analysis through the real DSP rather than a parallel implementation
is a deliberate simplification over the reference and removes a whole class
of "analysis and playback disagree" bugs.

### 3.8 Device changes and robustness

RtAudio stream-error and device-list-changed callbacks post a message to the
UI thread (never touch engine state directly from the audio thread's error
path); the engine transitions to a "device unavailable" state that keeps the
show, cue list, and cue positions intact, shows a status banner, and offers a
manual or automatic (on reappearance) reconnect. Disk underruns increment an
atomic counter the audio thread can bump without logging or allocating; the
UI/diagnostics window reads and displays it, and an underrun **never** stalls
or crashes the callback — a starved voice fades to silence and flags itself,
rather than blocking.

## 4. Spatial audio data model

### 4.1 Source formats (per piece/audio cue)

- **Multichannel speaker feeds** with a `Layout` (ring-front-left,
  ring-front-centre, pairs, ITU standard up to 7.1.4, two rings, dome, sphere,
  custom az/el per track) — `mc_dsp::layoutPreset()`, ported verbatim from the
  reference with 7.1.4 added (reference tops out at 7.1; dome/sphere already
  support arbitrary N so 7.1.4 is a new named ITU preset, not new math).
- **AmbiX** (ACN/SN3D), 1st–7th order (reference: 1st–5th; extended per §5).
- **FuMa** (W X Y Z … up to 3rd order), converted internally to AmbiX via the
  same channel/gain table as the reference.
- Format is auto-detected from filenames (`mc_core::detectSourceFormat()`,
  same keyword rules as the reference) and always user-overridable.
- Per-track **LFE** flag: not placed in space, routed to subwoofer outputs (or
  spread across all real outputs if none are marked), same as the reference.

### 4.2 Listening formats (global, switchable live)

Direct (with a full crosspoint matrix, not just an output-offset popup —
this generalises the reference's per-track `→ out N` to arbitrary N×M),
3D Ambisonics → speakers (AllRAD for 3D layouts, 2D max-rE for horizontal
ones, order Auto or forced 1–7), VBAP → speakers (with imaginary speakers),
Binaural → headphones (SOFA-driven by default, spherical-head fallback, room
reverb, OSC head tracking), Stereo downmix, Ambisonic B-format out. Switching
formats while something is playing is seamless when only the gain matrix
changes size/values at fixed topology; otherwise the engine restarts the cue
at its current position under a short crossfade (reference: hard restart —
this is one of the explicit fixes, §5).

### 4.3 My speakers / speaker layout files

A speaker layout (output count, per-output az/el/distance, subwoofer flags,
first-output offset) is its own file format, independent of the show file, so
a show can move venues without touching cue data — directly answering §4.9's
"Collect show" and "Relink missing media" requirements by keeping venue
geometry out of the portable show bundle. Distance adds per-output delay and
gain compensation (manual override available) — new vs. the reference (§5).

## 5. Fixing the reference's known limitations

| Reference limitation | Fix in this app |
|---|---|
| No SOFA/measured HRTFs, no head tracking | `mc_dsp` binaural renderer loads SOFA files via libmysofa for the primary HRTF path; the spherical-head model becomes the fallback when no SOFA is loaded. Head tracking: OSC yaw/pitch/roll rotates the virtual source field before the gain matrix is computed (a rotation of ACN/SN3D coefficients, or a position rotation for speaker-feed pieces), recomputed on the audio thread's smoothing cadence, not per-sample. |
| No speaker delay/level compensation for distance | Speaker layout carries a `distance` per output; `mc_engine::Router` applies a fixed delay line (distance/speed-of-sound) and 1/distance-derived gain per output, both overridable per output. |
| Sample-peak only, no true peak | `mc_dsp::TruePeakMeter` does 4× oversampled peak detection (polyphase FIR upsampler + peak-hold), used by both the master limiter and the loudness analysis path. |
| No crossfades between pieces | Playlist auto-advance and cue-to-cue continuation support an equal-power or configurable-curve crossfade across the gap, not just a silent gap. |
| Binaural/stereo always start at output 1 | Listening-format output assignment carries its own configurable start channel, independent of "first output" used for direct/speaker formats. |
| Ambisonic order capped at 5th | Spherical harmonics, max-rE weighting and AllRAD are implemented generally up to 7th order; `trackAcn`-equivalent channel mapping is extended to 64 ACN channels (8th-order-sized table) even though the UI caps user-facing order at 7. |

## 6. File formats

### 6.1 Show file

Human-readable JSON, UTF-8, with `"schemaVersion"` at the root. Migrations are
pure functions `(JSON, fromVersion) -> JSON` registered in a table in
`mc_core`, applied in sequence on load; round-trip and migration are
first-class unit tests (§7). Media paths are stored relative to the show file
with a fallback search (same directory, a `Media/` subfolder, and the last
known absolute path) for "Relink missing media". "Collect show" copies the
show file plus every referenced media file (deduplicated) into one folder,
rewriting paths to be relative — this is export, not a live mode.

### 6.2 Playlists inside a show

Playlist mode persists as a particular, restricted shape of a cue list (one
media cue per piece, see §3.5) so there is exactly one serialization path to
test, not two.

### 6.3 Speaker layout file

Separate JSON document: output count, first-output offset, per-output
az/el/distance/subwoofer flag, named preset (if any) or `"custom"`. Referenced
from a show by filename/relative path but editable and shareable
independently (§4.3).

### 6.4 Analysis cache

Keyed like the reference (`path | mtime | size` hash), stored per-file; a
cache entry holds the per-channel 100 ms K-weighted mean-square series and
peak needed to recompute gated loudness instantly when faders or the
listening format change, without re-decoding the file. Safe to delete at any
time (rebuilt on demand), same guarantee as the reference.

## 7. Video

`mc_video` decodes via FFmpeg (hardware acceleration where available:
VideoToolbox, D3D11VA, VAAPI/NVDEC) on its own thread per active video cue,
and presents through Qt (QRhi preferred, OpenGL fallback) onto one or more
output surfaces with z-order/opacity/position/scale/rotation. It is an
optional link for `multicue-cli`'s headless paths — `mc_engine` and the
offline render path never require a GPU context. Video cue audio (embedded
or an attached set of external mono files) is just another set of `Voice`s
registered with the same `CueRuntime`, started on the same sample-accurate
frame as the picture; sync is maintained by presenting frames against the
audio clock rather than by adjusting audio to match video (§3.6).

## 8. Control and integration

`mc_control` exposes one OSC address space and one command-queue surface used
by the UI, OSC, and MIDI uniformly, so "go", "panic", "set level", etc. all
funnel through the same engine commands regardless of trigger source (hotkey,
OSC message, MIDI note/CC/MSC, timecode chase event). This also means
`multicue-cli`'s scripting-equivalent subcommands (`--play`, `--set-level`,
…, mirroring the reference's `~multichannelPlayer[...]` interface, §0 table)
share the exact same command path as the GUI, so there is only one thing to
test.

## 9. Testing strategy (maps to project brief §7)

- **`mc_dsp` unit tests (Catch2):** spherical harmonics vs. closed-form
  reference values at several orders; VBAP/AllRAD gain-sum and energy-
  normalisation invariants; downmix rule table; every layout preset's
  angles against hand-computed expected values (the reference's own angle
  formulas, which are exact fractions of 360°, are good fixtures);
  FuMa↔AmbiX round-trip.
- **Loudness compliance:** EBU Tech 3341/3342 reference signal fixtures run
  through `mc_dsp::Bs1770Meter`, checked against published expected LUFS/LRA.
- **`mc_engine` offline-render tests:** direct mode bit-exact against input;
  multi-file cue starts land on the same sample frame; fade/seek operations
  produce no sample-to-sample discontinuity beyond a tight threshold (a
  simple second-derivative/slew check across the transition).
- **Show file tests:** round-trip every schema version's fixture; migration
  chain produces the same result as a direct load of the current version.
- **Sanitizers in CI:** ASan + UBSan on all test binaries; Clang
  RealtimeSanitizer on the audio callback specifically, where the toolchain
  supports it, to catch any allocation/lock/syscall regression in the one
  place it is non-negotiable.

## 10. Dependencies

Chosen per the project brief's default stack (C++20, CMake, vcpkg manifest or
FetchContent, Qt 6 Widgets LGPL, RtAudio, libsndfile, FFmpeg LGPL build,
libsamplerate/r8brain-free, pffft, libmysofa, liblo/oscpack, RtMidi, Catch2
v3). **Before any dependency is actually added to the vcpkg manifest (M0),
its current released version and license are checked directly against its
upstream repository/registry — not recalled from training knowledge** — per
the project brief's explicit instruction; this is a standing action item for
whichever milestone first pulls a given dependency in, tracked per-dependency
in `docs/ROADMAP.md`. The ASIO optional build flag additionally requires
checking the Steinberg ASIO SDK's own license terms before enabling it, since
that SDK is not redistributable the way the other dependencies are.

## 11. Open questions / flags on the default stack

None at this time — the brief's stack (Qt 6 Widgets, RtAudio, libsndfile +
FFmpeg, pffft, libmysofa, liblo/oscpack, RtMidi, Catch2 v3) is coherent, all
permissively/LGPL licensed consistent with a GPL-3.0-or-later app, and each
piece maps cleanly onto one module above. If a specific version/license check
in M0 turns up a problem (e.g. a dependency has relicensed, or FFmpeg's LGPL
[ARCHITECTURE.md](ARCHITECTURE.md)build excludes a codec this project needs), that will be raised at that point
rather than guessed at here.
