#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

#include "mc/core/FileOrdering.h"

using namespace mc::core;

TEST_CASE("naturalSortKey orders numeric runs numerically, not lexically", "[core][fileordering]") {
    std::vector<std::string> names = {"track_10.wav", "track_2.wav", "track_1.wav"};
    std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) {
        return naturalSortKey(a) < naturalSortKey(b);
    });
    REQUIRE(names == std::vector<std::string>{"track_1.wav", "track_2.wav", "track_10.wav"});
}

TEST_CASE("naturalSortKey is case-insensitive", "[core][fileordering]") {
    REQUIRE(naturalSortKey("Track_1.WAV") == naturalSortKey("track_1.wav"));
}

TEST_CASE("expandTracksFromProbedFiles gives one track per mono file", "[core][fileordering]") {
    std::vector<ProbedFile> files = {
        {"/a.wav", 1, 48000.0, 1.0},
        {"/b.wav", 1, 48000.0, 2.0},
    };
    const auto tracks = expandTracksFromProbedFiles(files, 0);
    REQUIRE(tracks.size() == 2);
    REQUIRE(tracks[0].path == "/a.wav");
    REQUIRE(tracks[0].outputChannel == 0);
    REQUIRE(tracks[1].path == "/b.wav");
    REQUIRE(tracks[1].outputChannel == 1);
}

TEST_CASE("expandTracksFromProbedFiles splits an interleaved multichannel file into one track per channel", "[core][fileordering]") {
    std::vector<ProbedFile> files = {{"/quad.wav", 4, 48000.0, 5.0}};
    const auto tracks = expandTracksFromProbedFiles(files, 0);
    REQUIRE(tracks.size() == 4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(tracks[static_cast<std::size_t>(i)].path == "/quad.wav");
        REQUIRE(tracks[static_cast<std::size_t>(i)].channel == i);
        REQUIRE(tracks[static_cast<std::size_t>(i)].outputChannel == i);
    }
}

TEST_CASE("expandTracksFromProbedFiles honours a non-zero start output channel", "[core][fileordering]") {
    std::vector<ProbedFile> files = {{"/a.wav", 1, 48000.0, 1.0}};
    const auto tracks = expandTracksFromProbedFiles(files, 5);
    REQUIRE(tracks[0].outputChannel == 5);
}
