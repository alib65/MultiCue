#include <catch2/catch_test_macros.hpp>

#include "mc/core/PlaylistIO.h"

using namespace mc::core;

namespace {
Playlist makeSamplePlaylist() {
    Playlist pl;
    pl.settings.autoAdvance = false;
    pl.settings.gapSeconds = 2.5;

    Piece p1;
    p1.title = "Piece One";
    p1.trimDb = -3.5f;
    Track t1;
    t1.path = "/music/piece1_ch1.wav";
    t1.channel = 0;
    t1.fileChannelCount = 1;
    t1.fileSampleRate = 48000.0;
    t1.fileDurationSeconds = 12.34;
    t1.outputChannel = 0;
    t1.gainDb = -1.5f;
    t1.mute = true;
    t1.solo = false;
    p1.tracks.push_back(t1);
    Track t2 = t1;
    t2.path = "/music/piece1_ch2.wav";
    t2.outputChannel = 1;
    t2.mute = false;
    t2.solo = true;
    p1.tracks.push_back(t2);

    Piece p2;
    p2.title = "Piece Two";
    pl.pieces = {p1, p2};
    return pl;
}
} // namespace

TEST_CASE("Playlist round-trips through JSON exactly", "[core][playlistio]") {
    const auto original = makeSamplePlaylist();
    const auto json = serializePlaylist(original);
    const auto result = deserializePlaylist(json);

    REQUIRE(result.success);
    REQUIRE(result.playlist.settings.autoAdvance == original.settings.autoAdvance);
    REQUIRE(result.playlist.settings.gapSeconds == original.settings.gapSeconds);
    REQUIRE(result.playlist.pieces.size() == original.pieces.size());

    const auto& p1 = result.playlist.pieces[0];
    REQUIRE(p1.title == "Piece One");
    REQUIRE(p1.trimDb == -3.5f);
    REQUIRE(p1.tracks.size() == 2);
    REQUIRE(p1.tracks[0].path == "/music/piece1_ch1.wav");
    REQUIRE(p1.tracks[0].mute == true);
    REQUIRE(p1.tracks[1].solo == true);
    REQUIRE(p1.tracks[1].outputChannel == 1);

    REQUIRE(result.playlist.pieces[1].title == "Piece Two");
    REQUIRE(result.playlist.pieces[1].tracks.empty());
}

TEST_CASE("deserializePlaylist rejects invalid JSON", "[core][playlistio]") {
    const auto result = deserializePlaylist("{ not json");
    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.error.empty());
}

TEST_CASE("deserializePlaylist rejects a document with the wrong magic", "[core][playlistio]") {
    const auto result = deserializePlaylist(R"({"format":"SomeOtherApp","schemaVersion":1,"pieces":[]})");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("deserializePlaylist rejects a document with no schema version", "[core][playlistio]") {
    const auto result = deserializePlaylist(R"({"format":"MultiCue-Playlist","pieces":[]})");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("deserializePlaylist rejects a schema version newer than this build understands", "[core][playlistio]") {
    const auto result = deserializePlaylist(R"({"format":"MultiCue-Playlist","schemaVersion":999,"pieces":[]})");
    REQUIRE_FALSE(result.success);
}

TEST_CASE("Save/load round-trip through a real file", "[core][playlistio]") {
    const auto original = makeSamplePlaylist();
    const std::string path = "/tmp/mc_core_test_playlist.mcplist";
    std::string error;
    REQUIRE(savePlaylistToFile(original, path, error));

    const auto loaded = loadPlaylistFromFile(path);
    REQUIRE(loaded.success);
    REQUIRE(loaded.playlist.pieces.size() == 2);
}

TEST_CASE("A v1 document migrates to v2 with the layout heuristic, not just blanket defaults", "[core][playlistio]") {
    // Two tracks -> the "Standard" stereo preset (layoutIdx 3), matching
    // the reference's own behaviour when it first assigns a layout.
    const auto stereoDoc = R"({
        "format": "MultiCue-Playlist", "schemaVersion": 1,
        "settings": {"autoAdvance": true, "gapSeconds": 3.0},
        "pieces": [{"title": "Old Stereo Piece", "trimDb": 0.0,
            "tracks": [{"path": "/a.wav"}, {"path": "/b.wav"}]}]
    })";
    const auto stereoResult = deserializePlaylist(stereoDoc);
    REQUIRE(stereoResult.success);
    REQUIRE(stereoResult.playlist.pieces[0].layoutIdx == 3);
    REQUIRE(stereoResult.playlist.pieces[0].srcFormat == SourceFormat::Multi);
    REQUIRE(stereoResult.playlist.pieces[0].tracks[0].azimuthDeg == 0.0);
    REQUIRE(stereoResult.playlist.pieces[0].tracks[0].lfe == false);

    // Four tracks -> ring-from-front-left (layoutIdx 0).
    const auto quadDoc = R"({
        "format": "MultiCue-Playlist", "schemaVersion": 1,
        "settings": {"autoAdvance": true, "gapSeconds": 3.0},
        "pieces": [{"title": "Old Quad Piece", "trimDb": 0.0,
            "tracks": [{"path": "/a.wav"}, {"path": "/b.wav"}, {"path": "/c.wav"}, {"path": "/d.wav"}]}]
    })";
    const auto quadResult = deserializePlaylist(quadDoc);
    REQUIRE(quadResult.success);
    REQUIRE(quadResult.playlist.pieces[0].layoutIdx == 0);
}

TEST_CASE("A v2 document's spatial fields round-trip exactly", "[core][playlistio]") {
    Playlist pl;
    Piece p;
    p.title = "Spatial Piece";
    p.srcFormat = SourceFormat::AmbiX;
    p.layoutIdx = -1;
    Track t;
    t.path = "/a.wav";
    t.azimuthDeg = 45.0;
    t.elevationDeg = -20.0;
    t.lfe = true;
    p.tracks.push_back(t);
    pl.pieces.push_back(p);

    const auto json = serializePlaylist(pl);
    const auto result = deserializePlaylist(json);
    REQUIRE(result.success);
    REQUIRE(result.playlist.pieces[0].srcFormat == SourceFormat::AmbiX);
    REQUIRE(result.playlist.pieces[0].layoutIdx == -1);
    REQUIRE(result.playlist.pieces[0].tracks[0].azimuthDeg == 45.0);
    REQUIRE(result.playlist.pieces[0].tracks[0].elevationDeg == -20.0);
    REQUIRE(result.playlist.pieces[0].tracks[0].lfe == true);
}

TEST_CASE("loadPlaylistFromFile reports an error for a missing file", "[core][playlistio]") {
    const auto result = loadPlaylistFromFile("/no/such/playlist.mcplist");
    REQUIRE_FALSE(result.success);
    REQUIRE_FALSE(result.error.empty());
}
