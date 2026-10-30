#include "mc/video/VideoDecoder.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace mc::video {

int selectPresentationFrameIndex(const std::vector<double>& queuedPtsSeconds, double audioClockSeconds) {
    int due = -1;
    for (std::size_t i = 0; i < queuedPtsSeconds.size(); ++i) {
        if (queuedPtsSeconds[i] <= audioClockSeconds) {
            due = static_cast<int>(i);
        } else {
            break;
        }
    }
    return due;
}

namespace {
constexpr std::size_t kQueueCapacity = 8;
}

struct VideoDecoder::Impl {
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx = nullptr;
    SwsContext* swsCtx = nullptr;
    int videoStreamIndex = -1;
    int width = 0;
    int height = 0;

    std::thread decodeThread;
    std::atomic<bool> stopRequested{false};
    std::atomic<bool> open{false};

    mutable std::mutex queueMutex;
    std::condition_variable queueNotFull;
    std::deque<DecodedFrame> queue;

    std::string lastError;

    ~Impl() { closeAll(); }

    void closeAll() {
        stopRequested.store(true, std::memory_order_release);
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            queueNotFull.notify_all();
        }
        if (decodeThread.joinable()) {
            decodeThread.join();
        }
        if (swsCtx) {
            sws_freeContext(swsCtx);
            swsCtx = nullptr;
        }
        if (codecCtx) {
            avcodec_free_context(&codecCtx);
        }
        if (formatCtx) {
            avformat_close_input(&formatCtx);
        }
        videoStreamIndex = -1;
        width = 0;
        height = 0;
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            queue.clear();
        }
        open.store(false, std::memory_order_release);
    }

    void decodeLoop() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        const AVStream* stream = formatCtx->streams[videoStreamIndex];

        while (!stopRequested.load(std::memory_order_acquire)) {
            if (av_read_frame(formatCtx, packet) < 0) {
                break; // end of file
            }
            if (packet->stream_index != videoStreamIndex) {
                av_packet_unref(packet);
                continue;
            }
            if (avcodec_send_packet(codecCtx, packet) == 0) {
                while (!stopRequested.load(std::memory_order_acquire) &&
                       avcodec_receive_frame(codecCtx, frame) == 0) {
                    const std::int64_t ts = (frame->pts != AV_NOPTS_VALUE) ? frame->pts : frame->best_effort_timestamp;
                    const double ptsSeconds = (ts != AV_NOPTS_VALUE) ? ts * av_q2d(stream->time_base) : 0.0;

                    DecodedFrame decoded;
                    decoded.width = codecCtx->width;
                    decoded.height = codecCtx->height;
                    decoded.ptsSeconds = ptsSeconds;
                    decoded.rgb.resize(static_cast<std::size_t>(codecCtx->width) * static_cast<std::size_t>(codecCtx->height) * 3);

                    unsigned char* dstPlanes[1] = {decoded.rgb.data()};
                    const int dstStride[1] = {codecCtx->width * 3};
                    sws_scale(swsCtx, frame->data, frame->linesize, 0, codecCtx->height, dstPlanes, dstStride);

                    std::unique_lock<std::mutex> lock(queueMutex);
                    queueNotFull.wait(lock, [this] {
                        return stopRequested.load(std::memory_order_acquire) || queue.size() < kQueueCapacity;
                    });
                    if (stopRequested.load(std::memory_order_acquire)) {
                        break;
                    }
                    queue.push_back(std::move(decoded));
                }
            }
            av_packet_unref(packet);
        }

        av_frame_free(&frame);
        av_packet_free(&packet);
    }
};

VideoDecoder::VideoDecoder() : impl_(std::make_unique<Impl>()) {}
VideoDecoder::~VideoDecoder() = default;

bool VideoDecoder::open(const std::string& path, double startSeconds) {
    close();

    if (avformat_open_input(&impl_->formatCtx, path.c_str(), nullptr, nullptr) != 0) {
        impl_->lastError = "Could not open \"" + path + "\".";
        return false;
    }
    if (avformat_find_stream_info(impl_->formatCtx, nullptr) < 0) {
        impl_->lastError = "Could not read stream info from \"" + path + "\".";
        close();
        return false;
    }

    const AVCodec* decoder = nullptr;
    const int streamIndex = av_find_best_stream(impl_->formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
    if (streamIndex < 0 || decoder == nullptr) {
        impl_->lastError = "\"" + path + "\" has no decodable video stream.";
        close();
        return false;
    }
    impl_->videoStreamIndex = streamIndex;

    impl_->codecCtx = avcodec_alloc_context3(decoder);
    if (avcodec_parameters_to_context(impl_->codecCtx, impl_->formatCtx->streams[streamIndex]->codecpar) < 0 ||
        avcodec_open2(impl_->codecCtx, decoder, nullptr) < 0) {
        impl_->lastError = "Could not open a decoder for \"" + path + "\".";
        close();
        return false;
    }
    impl_->width = impl_->codecCtx->width;
    impl_->height = impl_->codecCtx->height;

    impl_->swsCtx = sws_getContext(impl_->width, impl_->height, impl_->codecCtx->pix_fmt, impl_->width, impl_->height,
                                   AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!impl_->swsCtx) {
        impl_->lastError = "Could not set up pixel format conversion for \"" + path + "\".";
        close();
        return false;
    }

    if (startSeconds > 0.0) {
        const AVStream* stream = impl_->formatCtx->streams[streamIndex];
        const std::int64_t target = static_cast<std::int64_t>(startSeconds / av_q2d(stream->time_base));
        av_seek_frame(impl_->formatCtx, streamIndex, target, AVSEEK_FLAG_BACKWARD);
        avcodec_flush_buffers(impl_->codecCtx);
    }

    impl_->lastError.clear();
    impl_->stopRequested.store(false, std::memory_order_release);
    impl_->open.store(true, std::memory_order_release);
    impl_->decodeThread = std::thread([this] { impl_->decodeLoop(); });
    return true;
}

void VideoDecoder::close() {
    impl_->closeAll();
}

std::vector<double> VideoDecoder::queuedPtsSeconds() const {
    std::lock_guard<std::mutex> lock(impl_->queueMutex);
    std::vector<double> pts;
    pts.reserve(impl_->queue.size());
    for (const auto& f : impl_->queue) {
        pts.push_back(f.ptsSeconds);
    }
    return pts;
}

std::optional<DecodedFrame> VideoDecoder::popThrough(int index) {
    std::unique_lock<std::mutex> lock(impl_->queueMutex);
    if (index < 0 || static_cast<std::size_t>(index) >= impl_->queue.size()) {
        return std::nullopt;
    }
    DecodedFrame result = std::move(impl_->queue[static_cast<std::size_t>(index)]);
    for (int i = 0; i <= index; ++i) {
        impl_->queue.pop_front();
    }
    lock.unlock();
    impl_->queueNotFull.notify_one();
    return result;
}

bool VideoDecoder::isOpen() const noexcept {
    return impl_->open.load(std::memory_order_acquire);
}

std::string VideoDecoder::lastError() const {
    return impl_->lastError;
}

} // namespace mc::video
