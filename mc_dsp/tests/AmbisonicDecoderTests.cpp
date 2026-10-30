#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "mc/dsp/AmbisonicDecoder.h"
#include "mc/dsp/SphericalHarmonics.h"

using namespace mc::dsp;

namespace {
double decodeAtSpeaker(const std::vector<std::vector<double>>& dm, std::size_t speaker, const std::vector<double>& encoded) {
    double sum = 0.0;
    for (std::size_t c = 0; c < encoded.size(); ++c) {
        sum += dm[speaker][c] * encoded[c];
    }
    return sum;
}
} // namespace

TEST_CASE("isLayout2D is true only when every elevation is within +-10 degrees", "[dsp][decoder]") {
    REQUIRE(isLayout2D({{0, 0}, {90, 5}, {-90, -9}}));
    REQUIRE_FALSE(isLayout2D({{0, 0}, {90, 11}}));
    REQUIRE_FALSE(isLayout2D({{0, 35}}));
}

TEST_CASE("decode2D returns a [numSpeakers][(order+1)^2] matrix", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    for (int order = 1; order <= 3; ++order) {
        auto dm = decode2D(quad, order);
        REQUIRE(dm.size() == 4);
        for (const auto& row : dm) {
            REQUIRE(row.size() == static_cast<std::size_t>((order + 1) * (order + 1)));
        }
    }
}

TEST_CASE("A front-encoded source decodes louder to the front speakers than the rear ones", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto dm = decode2D(quad, 2);
    auto encoded = sphericalHarmonicsSN3D(2, 0.0, 0.0); // straight ahead

    const double front = std::abs(decodeAtSpeaker(dm, 0, encoded)) + std::abs(decodeAtSpeaker(dm, 1, encoded));
    const double rear = std::abs(decodeAtSpeaker(dm, 2, encoded)) + std::abs(decodeAtSpeaker(dm, 3, encoded));
    REQUIRE(front > rear);
}

TEST_CASE("AllRAD falls back to decode2D for a flat (horizontal) layout", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> quad = {{45, 0}, {-45, 0}, {-135, 0}, {135, 0}};
    auto a = allRadDecoder(quad, 2);
    auto b = decode2D(quad, 2);
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        for (std::size_t c = 0; c < a[i].size(); ++c) {
            REQUIRE(a[i][c] == Catch::Approx(b[i][c]).margin(1e-9));
        }
    }
}

TEST_CASE("AllRAD on a 3D layout decodes a top-encoded source loudest to the top speaker", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> dome = {{0, 0}, {90, 0}, {180, 0}, {-90, 0}, {0, 90}};
    auto dm = allRadDecoder(dome, 1);
    REQUIRE(dm.size() == 5);
    auto encodedTop = sphericalHarmonicsSN3D(1, 0.0, 90.0);
    double top = std::abs(decodeAtSpeaker(dm, 4, encodedTop));
    double maxOther = 0.0;
    for (std::size_t i = 0; i < 4; ++i) {
        maxOther = std::max(maxOther, std::abs(decodeAtSpeaker(dm, i, encodedTop)));
    }
    REQUIRE(top > maxOther);
}

TEST_CASE("virtualSpeakerDecoder returns a [numVirtual][(order+1)^2] matrix and is directionally sane", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> virt = {{0, 0}, {180, 0}};
    auto dm = virtualSpeakerDecoder(virt, 1);
    REQUIRE(dm.size() == 2);
    REQUIRE(dm[0].size() == 4);

    auto encodedFront = sphericalHarmonicsSN3D(1, 0.0, 0.0);
    REQUIRE(std::abs(decodeAtSpeaker(dm, 0, encodedFront)) > std::abs(decodeAtSpeaker(dm, 1, encodedFront)));
}

TEST_CASE("Decoder outputs are finite across orders 1-5", "[dsp][decoder]") {
    std::vector<std::pair<double, double>> octo = {{22.5, 0}, {-22.5, 0}, {-67.5, 0}, {67.5, 0}, {112.5, 0}, {-112.5, 0}, {157.5, 0}, {-157.5, 0}};
    for (int order = 1; order <= 5; ++order) {
        auto dm = decode2D(octo, order);
        for (const auto& row : dm) {
            for (double v : row) {
                REQUIRE(std::isfinite(v));
            }
        }
    }
}
