#pragma once

#include <utility>

namespace mc::dsp {

// Maps a FuMa channel (file order W X Y Z R S T U V K L M N O P Q, 0-based
// index) to {ACN index, gain to apply to convert the FuMa-normalised signal
// to SN3D-normalised ACN (AmbiX)} -- ported verbatim from the reference's
// `fumaMap`. FuMa is defined only up to 3rd order (16 channels); returns
// {-1, 0.0} for an out-of-range index.
std::pair<int, double> fumaToAmbix(int fumaChannelIndex);

constexpr int kFumaMaxChannels = 16;

} // namespace mc::dsp
