#pragma once

#include <string>
#include <vector>

#include "mc/core/Piece.h"
#include "mc/core/Track.h"

namespace mc::core {

// A sort key such that natural ("…_1, …_2, …_10") ordering falls out of a
// plain string comparison -- ported from the reference's `natKey` (pads
// every run of digits to a fixed width). docs/ARCHITECTURE.md §0.
std::string naturalSortKey(const std::string& filename);

// Already-probed per-file info (see mc::engine::probeAudioFile for the
// part of this that actually touches a file). Kept here, separate from
// Track, so expandTracksFromProbedFiles below can be a pure function with
// no file I/O of its own -- straightforward to unit test.
struct ProbedFile {
    std::string path;
    int channels = 1;
    double sampleRate = 0.0;
    double durationSeconds = 0.0;
};

// Splits each file into one Track per channel (mono files contribute one
// track, an interleaved stereo/multichannel file contributes one per
// channel), assigning sequential output channels starting at
// `startOutputChannel` -- ported from the reference's `makeTracksFromFiles`.
// Does not sort `files`; sort by naturalSortKey(path) first if that is the
// desired track order (as it is when adding files from a file dialog).
std::vector<Track> expandTracksFromProbedFiles(const std::vector<ProbedFile>& files, int startOutputChannel = 0);

// Guesses a piece title from its file paths: the longest common filename
// prefix (trimming trailing separators like "_"/"-"/"."/" "), falling back
// to the containing folder's name -- ported from the reference's
// `guessTitle`.
std::string guessPieceTitle(const std::vector<std::string>& paths);

struct SourceDetectionResult {
    SourceFormat format = SourceFormat::Multi;
    int layoutIdx = 0; // mc::dsp::Layout numbering; meaningful only for SourceFormat::Multi
};

// Guesses a piece's source format and initial layout from its file names
// and track count -- ported from the reference's `detectSource`. Pure
// string logic: mc_core does not depend on mc_dsp
// (docs/ARCHITECTURE.md §2), so this only picks *which* layout preset
// index to use -- turning that into actual per-track azimuth/elevation
// values is the caller's job (mc_engine has both mc_core and mc_dsp).
SourceDetectionResult detectSourceFormat(const std::vector<std::string>& filenames, int trackCount);

} // namespace mc::core
