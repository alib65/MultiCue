#include "mc/core/FileOrdering.h"

#include <cctype>

namespace mc::core {

std::string naturalSortKey(const std::string& filename) {
    std::string out;
    std::string digits;
    auto flushDigits = [&]() {
        if (!digits.empty()) {
            // Zero-pad to 10 digits so numeric runs compare correctly as
            // plain strings ("2" < "10"), matching the reference's
            // ("0000000000" ++ num).keep(-10).
            if (digits.size() < 10) {
                out.append(10 - digits.size(), '0');
            }
            out += digits;
            digits.clear();
        }
    };
    for (unsigned char ch : filename) {
        const char lower = static_cast<char>(std::tolower(ch));
        if (std::isdigit(ch)) {
            digits += lower;
        } else {
            flushDigits();
            out += lower;
        }
    }
    flushDigits();
    return out;
}

std::vector<Track> expandTracksFromProbedFiles(const std::vector<ProbedFile>& files, int startOutputChannel) {
    std::vector<Track> tracks;
    int nextOutput = startOutputChannel;
    for (const auto& f : files) {
        for (int ch = 0; ch < f.channels; ++ch) {
            Track t;
            t.path = f.path;
            t.channel = ch;
            t.fileChannelCount = f.channels;
            t.fileSampleRate = f.sampleRate;
            t.fileDurationSeconds = f.durationSeconds;
            t.outputChannel = nextOutput;
            tracks.push_back(t);
            ++nextOutput;
        }
    }
    return tracks;
}

} // namespace mc::core
