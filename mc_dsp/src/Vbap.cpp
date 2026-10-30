#include "mc/dsp/Vbap.h"

#include <algorithm>
#include <cmath>

namespace mc::dsp {

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
using Vec3 = std::array<double, 3>;

Vec3 dirVec(double azDeg, double elDeg) {
    const double a = azDeg * kDegToRad;
    const double e = elDeg * kDegToRad;
    return {std::cos(a) * std::cos(e), std::sin(a) * std::cos(e), std::sin(e)};
}
double dot3(const Vec3& a, const Vec3& b) {
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}
Vec3 cross3(const Vec3& a, const Vec3& b) {
    return {(a[1] * b[2]) - (a[2] * b[1]), (a[2] * b[0]) - (a[0] * b[2]), (a[0] * b[1]) - (a[1] * b[0])};
}
Vec3 sub3(const Vec3& a, const Vec3& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
Vec3 scale3(const Vec3& a, double s) {
    return {a[0] * s, a[1] * s, a[2] * s};
}
double clampAcos(double x) {
    return std::acos(std::clamp(x, -1.0, 1.0));
}
} // namespace

VbapSetup vbapSetup(const std::vector<std::pair<double, double>>& realSpeakerDirsAzEl) {
    VbapSetup out;
    std::vector<Vec3> pts;
    pts.reserve(realSpeakerDirsAzEl.size());
    for (const auto& [az, el] : realSpeakerDirsAzEl) {
        pts.push_back(dirVec(az, el));
    }
    out.numReal = pts.size();

    // 26 cube corner/edge/face unit directions, sorted by |z| descending to
    // match the reference's deterministic tie-breaking order.
    std::vector<Vec3> candidates;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                if (x == 0 && y == 0 && z == 0) {
                    continue;
                }
                const double l = std::sqrt(static_cast<double>((x * x) + (y * y) + (z * z)));
                candidates.push_back({x / l, y / l, z / l});
            }
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Vec3& a, const Vec3& b) {
        return std::abs(a[2]) > std::abs(b[2]);
    });

    for (const auto& c : candidates) {
        double minAngle = 1e18;
        for (const auto& q : pts) {
            minAngle = std::min(minAngle, clampAcos(dot3(q, c)));
        }
        if (minAngle > 70.0 * kDegToRad) {
            pts.push_back(c);
        }
    }

    const std::size_t np = pts.size();
    for (std::size_t i = 0; i + 2 < np; ++i) {
        for (std::size_t j = i + 1; j + 1 < np; ++j) {
            for (std::size_t k = j + 1; k < np; ++k) {
                const Vec3& a = pts[i];
                const Vec3& b = pts[j];
                const Vec3& c = pts[k];
                const double det = dot3(a, cross3(b, c));
                if (std::abs(det) <= 0.0001) {
                    continue;
                }
                const Vec3 nrm = cross3(sub3(b, a), sub3(c, a));
                int side = 0;
                bool ok = true;
                for (std::size_t m = 0; ok && m < np; ++m) {
                    if (m == i || m == j || m == k) {
                        continue;
                    }
                    const double d = dot3(nrm, sub3(pts[m], a));
                    if (d > 0.000001) {
                        if (side < 0) {
                            ok = false;
                        } else {
                            side = 1;
                        }
                    } else if (d < -0.000001) {
                        if (side > 0) {
                            ok = false;
                        } else {
                            side = -1;
                        }
                    }
                }
                if (ok) {
                    VbapSetup::Facet f;
                    f.indices = {i, j, k};
                    const Vec3 bc = scale3(cross3(b, c), 1.0 / det);
                    const Vec3 ca = scale3(cross3(c, a), 1.0 / det);
                    const Vec3 ab = scale3(cross3(a, b), 1.0 / det);
                    f.inverseRows = {bc, ca, ab};
                    out.facets.push_back(f);
                }
            }
        }
    }
    out.points = std::move(pts);
    return out;
}

std::vector<double> vbapGains(const VbapSetup& setup, double azimuthDeg, double elevationDeg, bool renorm) {
    const std::size_t nReal = setup.numReal;
    if (nReal <= 1) {
        return std::vector<double>(nReal, 1.0);
    }
    const Vec3 p = dirVec(azimuthDeg, elevationDeg);

    double bestMin = -1e18;
    const VbapSetup::Facet* best = nullptr;
    std::array<double, 3> bestGains{};
    for (const auto& f : setup.facets) {
        std::array<double, 3> gi{};
        for (int q = 0; q < 3; ++q) {
            gi[static_cast<std::size_t>(q)] = dot3(p, f.inverseRows[static_cast<std::size_t>(q)]);
        }
        const double mn = *std::min_element(gi.begin(), gi.end());
        if (mn > bestMin) {
            bestMin = mn;
            best = &f;
            bestGains = gi;
        }
    }

    std::vector<double> g(setup.points.size(), 0.0);
    if (best != nullptr) {
        for (int q = 0; q < 3; ++q) {
            g[best->indices[static_cast<std::size_t>(q)]] = std::max(0.0, bestGains[static_cast<std::size_t>(q)]);
        }
    }
    double norm = 0.0;
    for (double v : g) norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 0.0) {
        for (double& v : g) v /= norm;
    }

    std::vector<double> out(g.begin(), g.begin() + static_cast<long>(nReal));
    if (renorm) {
        double outNorm = 0.0;
        for (double v : out) outNorm += v * v;
        outNorm = std::sqrt(outNorm);
        if (outNorm > 0.000001) {
            for (double& v : out) v /= outNorm;
        } else {
            const double equal = 1.0 / std::sqrt(static_cast<double>(nReal));
            std::fill(out.begin(), out.end(), equal);
        }
    }
    return out;
}

} // namespace mc::dsp
