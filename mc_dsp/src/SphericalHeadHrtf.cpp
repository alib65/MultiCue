#include "mc/dsp/SphericalHeadHrtf.h"

#include <algorithm>
#include <cmath>

namespace mc::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

struct Vec3 {
    double x, y, z;
};
Vec3 dirVec(double azDeg, double elDeg) {
    const double a = azDeg * kDegToRad;
    const double e = elDeg * kDegToRad;
    return {std::cos(a) * std::cos(e), std::sin(a) * std::cos(e), std::sin(e)};
}
double dot3(const Vec3& a, const Vec3& b) {
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

EarFilterCoeffs earCoeffs(const Vec3& p, double earY, double beta, double k, double headRadius, double speedOfSound) {
    const Vec3 earDir{0.0, earY, 0.0};
    const double th = std::acos(std::clamp(dot3(p, earDir), -1.0, 1.0));
    const double alpha = 1.05 + (0.95 * std::cos((th / (150.0 * kDegToRad)) * kPi));
    double delay;
    if (th < 0.5 * kPi) {
        delay = (headRadius / speedOfSound) * (1.0 - std::cos(th));
    } else {
        delay = (headRadius / speedOfSound) * (1.0 + (th - (0.5 * kPi)));
    }
    EarFilterCoeffs ec;
    ec.delaySeconds = delay;
    ec.a = (beta + (alpha * k)) / (beta + k);
    ec.b = (beta - (alpha * k)) / (beta + k);
    ec.c = (k - beta) / (beta + k);
    return ec;
}
} // namespace

SphericalHeadHrtf sphericalHeadHrtfCoeffs(double azimuthDeg, double elevationDeg, double sampleRate) {
    const double sr = std::max(sampleRate, 8000.0);
    const Vec3 p = dirVec(azimuthDeg, elevationDeg);
    constexpr double headRadius = 0.0875; // metres, average adult head
    constexpr double speedOfSound = 343.0; // m/s
    const double beta = 2.0 * speedOfSound / headRadius;
    const double k = 2.0 * sr;

    SphericalHeadHrtf out;
    out.left = earCoeffs(p, 1.0, beta, k, headRadius, speedOfSound);
    out.right = earCoeffs(p, -1.0, beta, k, headRadius, speedOfSound);

    const double back = std::max(0.0, -p.x);
    const double g = 1.0 - (0.45 * back);
    const double wc = 2.0 * kPi * 3000.0;
    out.shelfA = ((g * k) + wc) / (k + wc);
    out.shelfB = (wc - (g * k)) / (k + wc);
    out.shelfC = (k - wc) / (k + wc);
    return out;
}

} // namespace mc::dsp
