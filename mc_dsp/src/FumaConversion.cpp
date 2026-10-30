#include "mc/dsp/FumaConversion.h"

#include <array>
#include <cmath>

namespace mc::dsp {

std::pair<int, double> fumaToAmbix(int fumaChannelIndex) {
    static const std::array<std::pair<int, double>, kFumaMaxChannels> table = {{
        {0, std::sqrt(2.0)},
        {3, 1.0},
        {1, 1.0},
        {2, 1.0},
        {6, 1.0},
        {7, 2.0 / std::sqrt(3.0)},
        {5, 2.0 / std::sqrt(3.0)},
        {8, 2.0 / std::sqrt(3.0)},
        {4, 2.0 / std::sqrt(3.0)},
        {12, 1.0},
        {13, std::sqrt(45.0 / 32.0)},
        {11, std::sqrt(45.0 / 32.0)},
        {14, std::sqrt(3.0 / 5.0)},
        {10, std::sqrt(3.0 / 5.0)},
        {15, std::sqrt(8.0 / 5.0)},
        {9, std::sqrt(8.0 / 5.0)},
    }};
    if (fumaChannelIndex < 0 || fumaChannelIndex >= kFumaMaxChannels) {
        return {-1, 0.0};
    }
    return table[static_cast<std::size_t>(fumaChannelIndex)];
}

} // namespace mc::dsp
