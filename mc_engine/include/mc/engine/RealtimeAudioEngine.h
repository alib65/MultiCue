#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mc::engine {

// Plays a "piece" (one mono file per track, direct mode routing) live to an
// audio device: RtAudio I/O + a Voice (background reader thread + ring
// buffer) per track + the same PieceMixer the offline renderer uses, per
// docs/ARCHITECTURE.md §3. loadPiece() opens the files and prefills every
// voice's ring buffer *before* returning, so play() only has to flip a gate
// and trigger the start envelope -- the "GO is instant for a preloaded cue"
// requirement (project brief §6).
//
// Not copyable; owns real OS resources (an audio stream, background
// threads). All public methods except the audio callback itself are meant
// to be called from a single non-realtime thread (the UI thread).
class RealtimeAudioEngine {
public:
    RealtimeAudioEngine();
    ~RealtimeAudioEngine();

    RealtimeAudioEngine(const RealtimeAudioEngine&) = delete;
    RealtimeAudioEngine& operator=(const RealtimeAudioEngine&) = delete;

    // Opens the device stream (if not already open for this device) and
    // loads+prefills one voice per track. Tears down any previously loaded
    // piece first. Returns false and sets lastError() on failure.
    // `outputChannels` gives each track's direct-mode output (0-based); pass
    // an empty vector for the common case of track i -> output i.
    bool loadPiece(std::uint32_t deviceId,
                   const std::vector<std::string>& trackPaths,
                   std::size_t numOutputs,
                   const std::vector<int>& outputChannels = {},
                   double startSeconds = 0.0);

    void unload();

    void play();
    void pause();
    void stop();
    void seek(double seconds);

    bool isLoaded() const noexcept;
    bool isPlaying() const noexcept;
    double positionSeconds() const noexcept;
    double pieceDurationSeconds() const noexcept { return pieceDurationSeconds_; }

    std::uint64_t underrunCount() const noexcept;

    // Metering snapshot (see PieceMixer::trackPeak() et al.); 0 if nothing
    // is loaded or the index is out of range. Safe to poll from the UI
    // thread at any rate (e.g. a 30+ fps timer).
    float trackPeak(std::size_t track) const noexcept;
    float trackRms(std::size_t track) const noexcept;
    float outputPeak(std::size_t output) const noexcept;
    float outputRms(std::size_t output) const noexcept;
    std::size_t numOutputs() const noexcept;
    std::string lastError() const { return lastError_; }

    // True once the device has reported a disconnect (e.g. unplugged)
    // since the last loadPiece(). The engine does not crash or lose show
    // state when this happens (docs/ARCHITECTURE.md §3.8); the caller is
    // expected to show a message and offer to reload once a device is
    // available again.
    bool deviceWasLost() const noexcept;

    void setTrackGainDb(std::size_t track, float db);
    void setTrackMuted(std::size_t track, bool muted);
    void setTrackSolo(std::size_t track, bool solo);
    // Live routing change: re-point one track at a different output. Safe
    // while playing (see PieceMixer::setGainMatrix); a hard cut, not faded,
    // matching the reference's own un-faded direct-mode "-> out N" menu.
    void setTrackOutputChannel(std::size_t track, int outputChannel);
    void setMasterGainDb(float db);
    void setCeilingDb(float db);
    void setLimiterEnabled(bool enabled);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string lastError_;
    double pieceDurationSeconds_ = 0.0;
    bool everStarted_ = false; // owning-thread-only: has mixer->start() been called since the last loadPiece()?
};

} // namespace mc::engine
