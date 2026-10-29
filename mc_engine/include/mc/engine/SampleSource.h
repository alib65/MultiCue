#pragma once

#include <cstddef>

namespace mc::engine {

// A pull source of mono float samples at a fixed, known rate. This is the
// seam between "where the samples come from" (a file read synchronously for
// offline render/tests, or a ring buffer fed by a background reader thread
// for realtime playback) and PieceMixer, which only ever calls read() and
// does not care which.
class SampleSource {
public:
    virtual ~SampleSource() = default;

    // Writes up to numFrames mono samples into `out`. Returns the number of
    // frames actually written; the caller is responsible for treating the
    // remainder (if any) as silence/underrun, since what a short read means
    // differs between "end of file" (offline) and "reader thread starved"
    // (realtime).
    virtual std::size_t read(float* out, std::size_t numFrames) = 0;

    virtual bool atEnd() const = 0;
};

} // namespace mc::engine
