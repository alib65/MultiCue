#include "mc/dsp/FibonacciSphere.h"

#include <algorithm>
#include <cmath>

namespace mc::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kRadToDeg = 180.0 / kPi;
} // namespace

std::vector<std::pair<double, double>> fibonacciSphere(int n) {
    std::vector<std::pair<double, double>> out;
    if (n <= 0) {
        return out;
    }
    out.reserve(static_cast<std::size_t>(n));
    const double ga = kPi * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < n; ++i) {
        const double z = 1.0 - ((2.0 * i + 1.0) / n);
        const double r = std::sqrt(std::max(0.0, 1.0 - (z * z)));
        const double ph = i * ga;
        const double az = std::atan2(r * std::sin(ph), r * std::cos(ph)) * kRadToDeg;
        const double el = std::asin(std::clamp(z, -1.0, 1.0)) * kRadToDeg;
        out.emplace_back(az, el);
    }
    return out;
}

} // namespace mc::dsp
