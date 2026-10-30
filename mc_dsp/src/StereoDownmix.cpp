#include "mc/dsp/StereoDownmix.h"

#include <algorithm>
#include <cmath>

namespace mc::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double wrapAz(double az) {
    double w = std::fmod(az + 180.0, 360.0);
    if (w < 0.0) w += 360.0;
    return w - 180.0;
}
} // namespace

std::array<double, 2> stereoDownmixGains(double azimuthDeg, double elevationDeg) {
    const double a = wrapAz(azimuthDeg);
    const double elRad = elevationDeg * kDegToRad;
    const double rear = std::max(0.0, -(std::cos(a * kDegToRad) * std::cos(elRad)));

    double f;
    if (a > 90.0) {
        f = 180.0 - a;
    } else if (a < -90.0) {
        f = -180.0 - a;
    } else {
        f = a;
    }

    const double x = std::clamp((f * std::cos(elRad)) / 30.0, -1.0, 1.0);
    const double angle = (1.0 - x) * 0.25 * kPi;
    const double atten = 1.0 - (0.16 * rear);
    return {std::cos(angle) * atten, std::sin(angle) * atten};
}

} // namespace mc::dsp
