#include "mc/dsp/LayoutPreset.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

#include "mc/dsp/FibonacciSphere.h"

namespace mc::dsp {

namespace {

double wrapAz(double az) {
    double w = std::fmod(az + 180.0, 360.0);
    if (w < 0.0) {
        w += 360.0;
    }
    return w - 180.0;
}

double round1(double v) {
    return std::round(v * 10.0) / 10.0;
}

using Dirs = std::vector<SpeakerDirection>;

Dirs ringFrontLeft(int n) {
    Dirs out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::size_t>(i)].azimuthDeg = (180.0 / n) - (i * 360.0 / n);
    }
    return out;
}

Dirs ringFrontCentre(int n) {
    Dirs out(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        out[static_cast<std::size_t>(i)].azimuthDeg = -(i * 360.0 / n);
    }
    return out;
}

Dirs pairs(int n) {
    Dirs out(static_cast<std::size_t>(n));
    const int m = std::max(n / 2, 1);
    for (int i = 0; i < n; ++i) {
        if ((n % 2 == 1) && (i == n - 1)) {
            out[static_cast<std::size_t>(i)].azimuthDeg = 180.0;
        } else {
            const double mag = (((2.0 * (i / 2)) + 1.0) * 180.0) / (2.0 * m);
            out[static_cast<std::size_t>(i)].azimuthDeg = mag * ((i % 2 == 0) ? 1.0 : -1.0);
        }
    }
    return out;
}

std::optional<Dirs> standard(int n) {
    auto mk = [](std::initializer_list<std::pair<double, bool>> azsAndLfe) {
        Dirs out;
        for (auto& p : azsAndLfe) {
            SpeakerDirection d;
            d.azimuthDeg = p.first;
            d.lfe = p.second;
            out.push_back(d);
        }
        return out;
    };
    switch (n) {
        case 2:
            return mk({{30, false}, {-30, false}});
        case 3:
            return mk({{30, false}, {-30, false}, {0, false}});
        case 4:
            return mk({{45, false}, {-45, false}, {135, false}, {-135, false}});
        case 5:
            return mk({{30, false}, {-30, false}, {0, false}, {110, false}, {-110, false}});
        case 6:
            return mk({{30, false}, {-30, false}, {0, false}, {0, true}, {110, false}, {-110, false}});
        case 7:
            return mk({{30, false}, {-30, false}, {0, false}, {90, false}, {-90, false}, {150, false}, {-150, false}});
        case 8:
            return mk({{30, false}, {-30, false}, {0, false}, {0, true}, {90, false}, {-90, false}, {150, false}, {-150, false}});
        case 12: {
            // 7.1.4: the 7.1 base (case 8) plus four elevated "height"
            // speakers -- an extension beyond the reference, which topped
            // out at 7.1 (docs/ARCHITECTURE.md §4.1).
            auto base = *standard(8);
            Dirs height(4);
            height[0] = {45.0, 45.0, false};
            height[1] = {-45.0, 45.0, false};
            height[2] = {135.0, 45.0, false};
            height[3] = {-135.0, 45.0, false};
            base.insert(base.end(), height.begin(), height.end());
            return base;
        }
        default:
            return std::nullopt;
    }
}

} // namespace

std::optional<std::vector<SpeakerDirection>> layoutPreset(Layout layout, int n) {
    n = std::max(n, 1);

    std::optional<Dirs> res;
    if (layout == Layout::Custom) {
        return std::nullopt;
    }
    if (n == 1) {
        res = Dirs{SpeakerDirection{0.0, 0.0, false}};
    } else {
        switch (layout) {
            case Layout::RingFrontLeft:
                res = ringFrontLeft(n);
                break;
            case Layout::RingFrontCentre:
                res = ringFrontCentre(n);
                break;
            case Layout::Pairs:
                res = pairs(n);
                break;
            case Layout::Standard: {
                auto s = standard(n);
                res = s.has_value() ? s : std::make_optional(pairs(n));
                break;
            }
            case Layout::TwoRings: {
                const int lower = static_cast<int>(std::ceil(n / 2.0));
                const int upper = n - lower;
                Dirs d = ringFrontLeft(lower);
                if (upper > 0) {
                    auto up = ringFrontLeft(upper);
                    for (auto& s : up) {
                        s.elevationDeg = 35.0;
                    }
                    d.insert(d.end(), up.begin(), up.end());
                }
                res = d;
                break;
            }
            case Layout::Dome: {
                if (n < 5) {
                    res = layoutPreset(Layout::TwoRings, n);
                } else {
                    const int m = n - 1;
                    const int lower = std::max(static_cast<int>(std::lround(m * 2.0 / 3.0)), 3);
                    const int upper = m - lower;
                    Dirs d = ringFrontLeft(lower);
                    if (upper > 0) {
                        auto up = ringFrontLeft(upper);
                        for (auto& s : up) {
                            s.elevationDeg = 40.0;
                        }
                        d.insert(d.end(), up.begin(), up.end());
                    }
                    d.push_back(SpeakerDirection{0.0, 90.0, false});
                    res = d;
                }
                break;
            }
            case Layout::Sphere: {
                const auto pts = fibonacciSphere(n);
                Dirs d(pts.size());
                for (std::size_t i = 0; i < pts.size(); ++i) {
                    d[i].azimuthDeg = pts[i].first;
                    d[i].elevationDeg = pts[i].second;
                }
                res = d;
                break;
            }
            case Layout::Custom:
                res = std::nullopt;
                break;
        }
    }

    if (!res.has_value()) {
        return std::nullopt;
    }
    for (auto& d : *res) {
        d.azimuthDeg = round1(wrapAz(d.azimuthDeg));
        d.elevationDeg = round1(d.elevationDeg);
    }
    return res;
}

} // namespace mc::dsp
