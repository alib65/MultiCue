#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mc::video {

// One decoded frame, already colour-converted to tightly-packed RGB24
// (width*height*3 bytes, row-major, no padding) so the presentation layer
// can upload it straight to a GL texture regardless of the source's native
// pixel format (yuv420p, nv12, 10-bit, ...).
struct DecodedFrame {
    std::vector<unsigned char> rgb;
    int width = 0;
    int height = 0;
    double ptsSeconds = 0.0;
};

// Oldest-first queued frame PTS (seconds) vs. the current audio-clock
// seconds (docs/ARCHITECTURE.md Sec 3.6: video presents by comparing PTS to
// "now" and corrects drift by frame drop/repeat, never by adjusting audio).
// Returns the index of the frame that should be presented now -- dropping
// every earlier one as stale -- or -1 if none are due yet (the caller should
// keep showing whatever it last presented). Pure and Qt/FFmpeg-free so it's
// unit-testable without a real decode.
int selectPresentationFrameIndex(const std::vector<double>& queuedPtsSeconds, double audioClockSeconds);

// Decodes one video file's video stream on a background thread (FFmpeg
// demux + decode + sws_scale), handing frames to the caller through a
// small bounded queue. No audio of its own is decoded or exposed -- the
// multichannel tracks already playing through RealtimeAudioEngine are the
// piece's audio; this class only ever produces pictures. Qt-free, so it
// can be linked and unit-tested without pulling in Qt.
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    VideoDecoder(const VideoDecoder&) = delete;
    VideoDecoder& operator=(const VideoDecoder&) = delete;

    // Opens `path`, seeks close to startSeconds, and starts the decode
    // thread. Closes any previously open file first. Returns false and
    // sets lastError() on failure (missing file, no video stream,
    // unsupported codec).
    bool open(const std::string& path, double startSeconds = 0.0);

    // Stops the decode thread and releases FFmpeg resources. Safe to call
    // when nothing is open.
    void close();

    // Non-blocking: returns the oldest queued frame's PTS values without
    // removing them, oldest first. Used by the presentation layer to ask
    // selectPresentationFrameIndex() which one (if any) is due.
    std::vector<double> queuedPtsSeconds() const;

    // Removes and returns the frame at `index` plus every older frame in
    // the queue (the stale ones selectPresentationFrameIndex() said to
    // drop). Returns the frame at `index`; no-op / nullopt if out of range.
    std::optional<DecodedFrame> popThrough(int index);

    bool isOpen() const noexcept;
    std::string lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mc::video
