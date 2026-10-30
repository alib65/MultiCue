#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>

#include <memory>
#include <optional>
#include <string>

#include "mc/video/VideoDecoder.h"

class QOpenGLShaderProgram;

namespace mc::video {

// Qt presentation side of one attached video: owns a VideoDecoder and
// draws whichever decoded frame is due against the caller-supplied audio
// clock (docs/ARCHITECTURE.md Sec 3.6 -- video is the slave clock, never
// the other way round), as one textured quad scaled to fit the widget
// while preserving the source's aspect ratio. Resolution-agnostic by
// construction: HD/4K/8K are just differently-sized textures.
class VideoSurfaceWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    explicit VideoSurfaceWidget(QWidget* parent = nullptr);
    ~VideoSurfaceWidget() override;

    bool open(const std::string& path, double startSeconds = 0.0);
    void close();

    // Called once per UI poll tick (MainWindow::onPollTick). Picks the
    // frame due at `audioClockSeconds` via selectPresentationFrameIndex(),
    // drops every older queued frame, and schedules a repaint if the
    // presented frame changed. A no-op when nothing is open.
    void syncToClock(double audioClockSeconds);

    bool isOpen() const;
    std::string lastError() const;

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    void uploadFrame(const DecodedFrame& frame);
    void updateQuadGeometry();

    VideoDecoder decoder_;
    std::unique_ptr<QOpenGLShaderProgram> program_;
    std::optional<DecodedFrame> pendingFrame_; // uploaded in paintGL(), where the GL context is current
    QOpenGLVertexArrayObject vao_; // core profiles (the macOS default) require one bound to draw at all
    unsigned int vbo_ = 0;
    unsigned int textureId_ = 0;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    bool hasFrame_ = false;
};

} // namespace mc::video
