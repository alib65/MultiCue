#include "mc/dsp/AmbisonicDecoder.h"

#include <cmath>

#include "mc/dsp/FibonacciSphere.h"
#include "mc/dsp/SphericalHarmonics.h"
#include "mc/dsp/Vbap.h"

namespace mc::dsp {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kDegToRad = kPi / 180.0;

double factorialD(int n) {
    double r = 1.0;
    for (int i = 2; i <= n; ++i) r *= i;
    return r;
}

int acnDegree(int acn) {
    return static_cast<int>(std::floor(std::sqrt(static_cast<double>(acn))));
}
} // namespace

bool isLayout2D(const std::vector<std::pair<double, double>>& dirsAzEl) {
    for (const auto& [az, el] : dirsAzEl) {
        (void)az;
        if (std::abs(el) >= 10.0) {
            return false;
        }
    }
    return true;
}

std::vector<std::vector<double>> decode2D(const std::vector<std::pair<double, double>>& speakerDirsAzEl, int order) {
    const int nsh = (order + 1) * (order + 1);
    const auto setup = vbapSetup(speakerDirsAzEl);

    std::vector<double> w(static_cast<std::size_t>(order) + 1);
    std::vector<double> cn(static_cast<std::size_t>(order) + 1);
    for (int n = 0; n <= order; ++n) {
        w[static_cast<std::size_t>(n)] = std::cos(n * kPi / ((2.0 * order) + 2.0));
        if (n == 0) {
            cn[0] = 1.0;
        } else {
            double df = 1.0;
            for (int q = 0; q < n; ++q) df *= (2.0 * q) + 1.0;
            cn[static_cast<std::size_t>(n)] = std::sqrt(2.0 / factorialD(2 * n)) * df;
        }
    }

    std::vector<std::vector<double>> dm(speakerDirsAzEl.size(), std::vector<double>(static_cast<std::size_t>(nsh), 0.0));

    for (int i = 0; i < 72; ++i) {
        const double az = i * 5.0;
        const auto gv = vbapGains(setup, az, 0.0, true);
        std::vector<double> row(static_cast<std::size_t>(nsh), 0.0);
        row[0] = 1.0;
        for (int n = 1; n <= order; ++n) {
            row[static_cast<std::size_t>((n * n) + n + n)] = 2.0 * w[static_cast<std::size_t>(n)] / cn[static_cast<std::size_t>(n)] * std::cos(n * az * kDegToRad);
            row[static_cast<std::size_t>((n * n) + n - n)] = 2.0 * w[static_cast<std::size_t>(n)] / cn[static_cast<std::size_t>(n)] * std::sin(n * az * kDegToRad);
        }
        for (std::size_t k = 0; k < gv.size(); ++k) {
            if (gv[k] == 0.0) continue;
            for (int c = 0; c < nsh; ++c) {
                dm[k][static_cast<std::size_t>(c)] += row[static_cast<std::size_t>(c)] * gv[k];
            }
        }
    }

    double e = 0.0;
    for (int i = 0; i < 72; ++i) {
        const auto y = sphericalHarmonicsSN3D(order, i * 5.0, 0.0);
        double sum = 0.0;
        for (const auto& r : dm) {
            double dotv = 0.0;
            for (int c = 0; c < nsh; ++c) dotv += r[static_cast<std::size_t>(c)] * y[static_cast<std::size_t>(c)];
            sum += dotv * dotv;
        }
        e += sum;
    }
    e /= 72.0;
    if (e > 0.0) {
        const double scale = 1.0 / std::sqrt(e);
        for (auto& r : dm) {
            for (double& v : r) v *= scale;
        }
    }
    return dm;
}

std::vector<std::vector<double>> allRadDecoder(const std::vector<std::pair<double, double>>& speakerDirsAzEl, int order) {
    if (isLayout2D(speakerDirsAzEl) && speakerDirsAzEl.size() > 1) {
        return decode2D(speakerDirsAzEl, order);
    }

    const int nsh = (order + 1) * (order + 1);
    const auto setup = vbapSetup(speakerDirsAzEl);
    const auto w = maxReWeights(order);

    std::vector<std::vector<double>> dm(speakerDirsAzEl.size(), std::vector<double>(static_cast<std::size_t>(nsh), 0.0));

    const auto virtualPts = fibonacciSphere(240);
    for (const auto& [vaz, vel] : virtualPts) {
        const auto y = sphericalHarmonicsSN3D(order, vaz, vel);
        const auto gv = vbapGains(setup, vaz, vel, false);
        std::vector<double> row(static_cast<std::size_t>(nsh));
        for (int acn = 0; acn < nsh; ++acn) {
            const int n = acnDegree(acn);
            row[static_cast<std::size_t>(acn)] = w[static_cast<std::size_t>(n)] * ((2.0 * n) + 1.0) * y[static_cast<std::size_t>(acn)];
        }
        for (std::size_t k = 0; k < gv.size(); ++k) {
            if (gv[k] == 0.0) continue;
            for (int c = 0; c < nsh; ++c) {
                dm[k][static_cast<std::size_t>(c)] += row[static_cast<std::size_t>(c)] * gv[k];
            }
        }
    }

    const auto testPts = fibonacciSphere(200);
    double e = 0.0;
    for (const auto& [taz, tel] : testPts) {
        const auto y = sphericalHarmonicsSN3D(order, taz, tel);
        double sum = 0.0;
        for (const auto& r : dm) {
            double dotv = 0.0;
            for (int c = 0; c < nsh; ++c) dotv += r[static_cast<std::size_t>(c)] * y[static_cast<std::size_t>(c)];
            sum += dotv * dotv;
        }
        e += sum;
    }
    e /= static_cast<double>(testPts.size());
    if (e > 0.0) {
        const double scale = 1.0 / std::sqrt(e);
        for (auto& r : dm) {
            for (double& v : r) v *= scale;
        }
    }
    return dm;
}

std::vector<std::vector<double>> virtualSpeakerDecoder(const std::vector<std::pair<double, double>>& virtualDirsAzEl, int order) {
    const int nsh = (order + 1) * (order + 1);
    const auto w = maxReWeights(order);

    std::vector<std::vector<double>> dm;
    dm.reserve(virtualDirsAzEl.size());
    for (const auto& [az, el] : virtualDirsAzEl) {
        const auto y = sphericalHarmonicsSN3D(order, az, el);
        std::vector<double> row(static_cast<std::size_t>(nsh));
        for (int acn = 0; acn < nsh; ++acn) {
            const int n = acnDegree(acn);
            row[static_cast<std::size_t>(acn)] = w[static_cast<std::size_t>(n)] * ((2.0 * n) + 1.0) * y[static_cast<std::size_t>(acn)];
        }
        dm.push_back(std::move(row));
    }

    const auto testPts = fibonacciSphere(100);
    double e = 0.0;
    for (const auto& [taz, tel] : testPts) {
        const auto y = sphericalHarmonicsSN3D(order, taz, tel);
        double sum = 0.0;
        for (const auto& r : dm) {
            double dotv = 0.0;
            for (int c = 0; c < nsh; ++c) dotv += r[static_cast<std::size_t>(c)] * y[static_cast<std::size_t>(c)];
            sum += dotv * dotv;
        }
        e += sum;
    }
    e /= static_cast<double>(testPts.size());
    if (e > 0.0) {
        const double scale = 1.0 / std::sqrt(e);
        for (auto& r : dm) {
            for (double& v : r) v *= scale;
        }
    }
    return dm;
}

} // namespace mc::dsp
