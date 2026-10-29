#pragma once

#include <string>
#include <vector>

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

} // namespace mc::core
