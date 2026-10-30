#pragma once

namespace mc::engine {

// TODO(M3): realtime HRTF filtering stage that turns a SpatialRenderResult's
// per-direction virtual sources into the final stereo output (ROADMAP.md M3).
// Stubbed out so mc_engine links; nothing calls this yet.
class BinauralStage {
public:
    BinauralStage() = default;
    ~BinauralStage() = default;
};

} // namespace mc::engine
