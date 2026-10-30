#include "mc/engine/SpatialRenderer.h"

#include <algorithm>
#include <cmath>

#include "mc/dsp/AmbisonicDecoder.h"
#include "mc/dsp/FibonacciSphere.h"
#include "mc/dsp/FumaConversion.h"
#include "mc/dsp/SphericalHarmonics.h"
#include "mc/dsp/StereoDownmix.h"
#include "mc/dsp/Vbap.h"

namespace mc::engine {

namespace {

using mc::core::Piece;
using mc::core::SourceFormat;

// ACN index + gain for track i of an Ambisonic-source piece, ported from
// the reference's `trackAcn`.
std::pair<int, double> trackAcn(const Piece& p, int trackIndex) {
    if (p.srcFormat == SourceFormat::Fuma) {
        return mc::dsp::fumaToAmbix(trackIndex);
    }
    if (trackIndex < 36) {
        return {trackIndex, 1.0};
    }
    return {-1, 0.0};
}

// Ported from the reference's `srcOrder`: the order implied by an
// Ambisonic-source piece's own track count.
int srcOrder(const Piece& p) {
    const int n = std::max<int>(static_cast<int>(p.tracks.size()), 1);
    int o = static_cast<int>(std::floor(std::sqrt(static_cast<double>(n)))) - 1;
    o = std::clamp(o, 0, 7);
    if (p.srcFormat == SourceFormat::Fuma) {
        o = std::min(o, 3);
    }
    return o;
}

// Ported from the reference's `effectiveOrder`, extended to order 7
// (docs/ARCHITECTURE.md §5: the reference capped at 5th).
int effectiveOrder(const Piece& p, bool isBFormatMode, int orderOverride, const VenueLayout& venue, bool venueIs2D) {
    const std::size_t nl = std::max<std::size_t>(venue.realDirections.size(), 1);
    const int so = (p.srcFormat != SourceFormat::Multi) ? srcOrder(p) : 7;

    int lim;
    if (isBFormatMode) {
        const int n = static_cast<int>(std::floor(std::sqrt(static_cast<double>(std::max<std::size_t>(venue.numOutputs, 1)))));
        lim = std::clamp(n - 1, 0, 7);
    } else if (venueIs2D) {
        lim = std::clamp(static_cast<int>((nl - 1) / 2), 1, 7);
    } else {
        lim = std::clamp(static_cast<int>(std::floor(std::sqrt(static_cast<double>(nl)))) - 1, 1, 7);
    }

    if (isBFormatMode) {
        const int want = (orderOverride > 0) ? orderOverride : 3;
        return std::min({want, lim, so});
    }
    const int want = (orderOverride > 0) ? orderOverride : lim;
    return std::min(want, so);
}

void addLfeSpread(GainMatrix& matrix, std::size_t track, const VenueLayout& venue) {
    const auto& targets = venue.subOutputIndices.empty() ? venue.realOutputIndices : venue.subOutputIndices;
    if (targets.empty()) {
        return;
    }
    const double g = 1.0 / std::sqrt(static_cast<double>(targets.size()));
    for (auto out : targets) {
        matrix.setGain(track, out, static_cast<float>(g));
    }
}

} // namespace

SpatialRenderResult renderListeningFormat(ListeningFormat format,
                                          int orderOverride,
                                          const Piece& piece,
                                          const VenueLayout& venue,
                                          int stereoStartChannel) {
    SpatialRenderResult result;
    const std::size_t numTracks = piece.tracks.size();
    const bool isMulti = piece.srcFormat == SourceFormat::Multi;
    const bool venueIs2D = mc::dsp::isLayout2D(venue.realDirections);

    // Stereo/Binaural need at least 2 real output channels; fall back to
    // Ambisonics (to however many outputs exist) otherwise, matching the
    // reference's own fallback when nOuts < 2.
    if ((format == ListeningFormat::Stereo || format == ListeningFormat::Binaural) && venue.numOutputs < 2) {
        format = ListeningFormat::Ambisonics;
        result.note = "(only 1 output) ";
    }
    // VBAP needs speaker feeds; decode Ambisonic-source pieces instead.
    if (format == ListeningFormat::Vbap && !isMulti) {
        format = ListeningFormat::Ambisonics;
        result.note += "VBAP needs speaker feeds, so this Ambisonic piece is decoded: ";
    }

    switch (format) {
        case ListeningFormat::Ambisonics: {
            const int order = effectiveOrder(piece, false, orderOverride, venue, venueIs2D);
            const int nsh = (order + 1) * (order + 1);
            const auto dm = mc::dsp::allRadDecoder(venue.realDirections, order);
            result.matrix = GainMatrix(numTracks, venue.numOutputs);
            for (std::size_t i = 0; i < numTracks; ++i) {
                const auto& t = piece.tracks[i];
                if (isMulti) {
                    if (t.lfe) {
                        addLfeSpread(result.matrix, i, venue);
                        continue;
                    }
                    const auto y = mc::dsp::sphericalHarmonicsSN3D(order, t.azimuthDeg, t.elevationDeg);
                    for (std::size_t r = 0; r < venue.realOutputIndices.size(); ++r) {
                        double g = 0.0;
                        for (int c = 0; c < nsh; ++c) g += dm[r][static_cast<std::size_t>(c)] * y[static_cast<std::size_t>(c)];
                        result.matrix.setGain(i, venue.realOutputIndices[r], static_cast<float>(g));
                    }
                } else {
                    const auto [acn, gain] = trackAcn(piece, static_cast<int>(i));
                    if (acn >= 0 && acn < nsh) {
                        for (std::size_t r = 0; r < venue.realOutputIndices.size(); ++r) {
                            result.matrix.setGain(i, venue.realOutputIndices[r], static_cast<float>(dm[r][static_cast<std::size_t>(acn)] * gain));
                        }
                    }
                }
            }
            result.effectiveOrder = order;
            result.note += "3D Ambisonics order " + std::to_string(order) + " (AllRAD decoder) -> " +
                           std::to_string(venue.realOutputIndices.size()) + " speaker(s)";
            break;
        }

        case ListeningFormat::Vbap: {
            const auto setup = mc::dsp::vbapSetup(venue.realDirections);
            result.matrix = GainMatrix(numTracks, venue.numOutputs);
            for (std::size_t i = 0; i < numTracks; ++i) {
                const auto& t = piece.tracks[i];
                if (t.lfe) {
                    addLfeSpread(result.matrix, i, venue);
                    continue;
                }
                const auto g = mc::dsp::vbapGains(setup, t.azimuthDeg, t.elevationDeg);
                for (std::size_t r = 0; r < venue.realOutputIndices.size(); ++r) {
                    result.matrix.setGain(i, venue.realOutputIndices[r], static_cast<float>(g[r]));
                }
            }
            result.note += "Amplitude panning (VBAP) -> " + std::to_string(venue.realOutputIndices.size()) + " speaker(s)";
            break;
        }

        case ListeningFormat::Stereo: {
            result.matrix = GainMatrix(numTracks, venue.numOutputs);
            const auto l = static_cast<std::size_t>(stereoStartChannel);
            const auto r = static_cast<std::size_t>(stereoStartChannel + 1);
            if (isMulti) {
                for (std::size_t i = 0; i < numTracks; ++i) {
                    const auto& t = piece.tracks[i];
                    if (t.lfe) {
                        result.matrix.setGain(i, l, 0.7f);
                        result.matrix.setGain(i, r, 0.7f);
                        continue;
                    }
                    const auto g = mc::dsp::stereoDownmixGains(t.azimuthDeg, t.elevationDeg);
                    result.matrix.setGain(i, l, static_cast<float>(g[0]));
                    result.matrix.setGain(i, r, static_cast<float>(g[1]));
                }
            } else {
                const int order = srcOrder(piece);
                const int nsh = (order + 1) * (order + 1);
                const int numVirt = std::clamp(2 * nsh, 8, 50);
                const auto vpts = mc::dsp::fibonacciSphere(numVirt);
                const auto dv = mc::dsp::virtualSpeakerDecoder(vpts, order);
                std::vector<std::vector<double>> m2(2, std::vector<double>(static_cast<std::size_t>(nsh), 0.0));
                for (std::size_t v = 0; v < vpts.size(); ++v) {
                    const auto sg = mc::dsp::stereoDownmixGains(vpts[v].first, vpts[v].second);
                    for (int c = 0; c < nsh; ++c) {
                        m2[0][static_cast<std::size_t>(c)] += dv[v][static_cast<std::size_t>(c)] * sg[0];
                        m2[1][static_cast<std::size_t>(c)] += dv[v][static_cast<std::size_t>(c)] * sg[1];
                    }
                }
                double e = 0.0;
                const auto testPts = mc::dsp::fibonacciSphere(100);
                for (const auto& [taz, tel] : testPts) {
                    const auto y = mc::dsp::sphericalHarmonicsSN3D(order, taz, tel);
                    for (int ch = 0; ch < 2; ++ch) {
                        double dotv = 0.0;
                        for (int c = 0; c < nsh; ++c) dotv += m2[static_cast<std::size_t>(ch)][static_cast<std::size_t>(c)] * y[static_cast<std::size_t>(c)];
                        e += dotv * dotv;
                    }
                }
                e /= static_cast<double>(testPts.size());
                const double scale = (e > 0.0) ? (1.0 / std::sqrt(e)) : 1.0;
                for (std::size_t i = 0; i < numTracks; ++i) {
                    const auto [acn, gain] = trackAcn(piece, static_cast<int>(i));
                    if (acn >= 0 && acn < nsh) {
                        result.matrix.setGain(i, l, static_cast<float>(m2[0][static_cast<std::size_t>(acn)] * gain * scale));
                        result.matrix.setGain(i, r, static_cast<float>(m2[1][static_cast<std::size_t>(acn)] * gain * scale));
                    }
                }
            }
            result.stereoStartChannelUsed = stereoStartChannel;
            result.note += "Stereo downmix -> outputs " + std::to_string(stereoStartChannel + 1) + "-" + std::to_string(stereoStartChannel + 2);
            break;
        }

        case ListeningFormat::BFormat: {
            const int order = effectiveOrder(piece, true, orderOverride, venue, venueIs2D);
            const int nsh = (order + 1) * (order + 1);
            result.matrix = GainMatrix(numTracks, venue.numOutputs);
            for (std::size_t i = 0; i < numTracks; ++i) {
                const auto& t = piece.tracks[i];
                if (isMulti) {
                    if (t.lfe) {
                        result.matrix.setGain(i, 0, 1.0f);
                        continue;
                    }
                    const auto y = mc::dsp::sphericalHarmonicsSN3D(order, t.azimuthDeg, t.elevationDeg);
                    for (int c = 0; c < nsh && static_cast<std::size_t>(c) < venue.numOutputs; ++c) {
                        result.matrix.setGain(i, static_cast<std::size_t>(c), static_cast<float>(y[static_cast<std::size_t>(c)]));
                    }
                } else {
                    const auto [acn, gain] = trackAcn(piece, static_cast<int>(i));
                    if (acn >= 0 && acn < nsh && static_cast<std::size_t>(acn) < venue.numOutputs) {
                        result.matrix.setGain(i, static_cast<std::size_t>(acn), static_cast<float>(gain));
                    }
                }
            }
            result.effectiveOrder = order;
            result.note += "Ambisonics B-format (AmbiX ACN/SN3D), order " + std::to_string(order) + " -> outputs 1-" + std::to_string(nsh);
            break;
        }

        case ListeningFormat::Binaural: {
            result.isBinaural = true;
            if (isMulti) {
                // One virtual source per track, whether LFE or not: LFE
                // tracks still need to reach both ears, just without HRTF
                // filtering (virtualIsLfe == true tells the binaural stage
                // to bypass the per-direction filter for that slot).
                result.sourceToVirtual = GainMatrix(numTracks, std::max<std::size_t>(numTracks, 1));
                std::size_t nonLfeCount = 0;
                for (std::size_t i = 0; i < numTracks; ++i) {
                    const auto& t = piece.tracks[i];
                    result.sourceToVirtual.setGain(i, i, 1.0f);
                    result.virtualDirections.emplace_back(t.azimuthDeg, t.elevationDeg);
                    result.virtualIsLfe.push_back(t.lfe);
                    if (!t.lfe) ++nonLfeCount;
                }
                result.note = "Binaural (spherical-head model): " + std::to_string(nonLfeCount) + " virtual source(s) -> outputs " +
                              std::to_string(stereoStartChannel + 1) + "-" + std::to_string(stereoStartChannel + 2) + ", use headphones";
            } else {
                const int order = srcOrder(piece);
                const int nsh = (order + 1) * (order + 1);
                const int numVirt = std::clamp(2 * nsh, 8, 50);
                const auto vpts = mc::dsp::fibonacciSphere(numVirt);
                const auto dv = mc::dsp::virtualSpeakerDecoder(vpts, order);
                result.sourceToVirtual = GainMatrix(numTracks, static_cast<std::size_t>(numVirt));
                for (std::size_t i = 0; i < numTracks; ++i) {
                    const auto [acn, gain] = trackAcn(piece, static_cast<int>(i));
                    if (acn < 0 || acn >= nsh) continue;
                    for (std::size_t v = 0; v < vpts.size(); ++v) {
                        result.sourceToVirtual.setGain(i, v, static_cast<float>(dv[v][static_cast<std::size_t>(acn)] * gain));
                    }
                }
                result.virtualDirections = vpts;
                result.virtualIsLfe.assign(vpts.size(), false);
                result.note = "Binaural: Ambisonics order " + std::to_string(order) + " -> " + std::to_string(vpts.size()) +
                              " virtual speakers -> outputs " + std::to_string(stereoStartChannel + 1) + "-" + std::to_string(stereoStartChannel + 2) +
                              ", use headphones";
            }
            result.stereoStartChannelUsed = stereoStartChannel;
            break;
        }
    }

    return result;
}

} // namespace mc::engine
