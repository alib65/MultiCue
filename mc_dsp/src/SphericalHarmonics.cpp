#include "mc/dsp/SphericalHarmonics.h"

#include <algorithm>
#include <cmath>

namespace mc::dsp {

namespace {
constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

double factorial(int n) {
    double result = 1.0;
    for (int i = 2; i <= n; ++i) {
        result *= static_cast<double>(i);
    }
    return result;
}
} // namespace

double legendreP(int n, double x) {
    if (n == 0) {
        return 1.0;
    }
    double p0 = 1.0;
    double p1 = x;
    for (int k = 0; k < n - 1; ++k) {
        const int kk = k + 1;
        const double p2 = ((((2.0 * kk) + 1.0) * x * p1) - (kk * p0)) / (kk + 1.0);
        p0 = p1;
        p1 = p2;
    }
    return p1;
}

std::vector<double> maxReWeights(int order) {
    const double x = std::cos((137.9 / (order + 1.51)) * kDegToRad);
    std::vector<double> w(static_cast<std::size_t>(order) + 1);
    for (int n = 0; n <= order; ++n) {
        w[static_cast<std::size_t>(n)] = legendreP(n, x);
    }
    return w;
}

std::vector<double> sphericalHarmonicsSN3D(int order, double azimuthDeg, double elevationDeg) {
    const double x = std::sin(elevationDeg * kDegToRad);
    const double c = std::max(std::cos(elevationDeg * kDegToRad), 0.0);
    const double a = azimuthDeg * kDegToRad;

    std::vector<double> res(static_cast<std::size_t>((order + 1) * (order + 1)), 0.0);

    for (int m = 0; m <= order; ++m) {
        std::vector<double> p(static_cast<std::size_t>(order) + 1, 0.0);
        double dfact = 1.0;
        for (int q = 0; q < m; ++q) {
            dfact *= (2.0 * q) + 1.0;
        }
        p[static_cast<std::size_t>(m)] = dfact * std::pow(c, m);
        if (m < order) {
            p[static_cast<std::size_t>(m + 1)] = x * ((2.0 * m) + 1.0) * p[static_cast<std::size_t>(m)];
        }
        for (int n = m + 2; n <= order; ++n) {
            p[static_cast<std::size_t>(n)] =
                ((((2.0 * n) - 1.0) * x * p[static_cast<std::size_t>(n - 1)]) - ((n + m - 1.0) * p[static_cast<std::size_t>(n - 2)])) /
                (n - m);
        }
        for (int n = m; n <= order; ++n) {
            const double norm = std::sqrt((m == 0 ? 1.0 : 2.0) * (factorial(n - m) / factorial(n + m)));
            res[static_cast<std::size_t>((n * n) + n + m)] = norm * p[static_cast<std::size_t>(n)] * std::cos(m * a);
            if (m > 0) {
                res[static_cast<std::size_t>((n * n) + n - m)] = norm * p[static_cast<std::size_t>(n)] * std::sin(m * a);
            }
        }
    }
    return res;
}

} // namespace mc::dsp
