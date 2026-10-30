#include "mc/video/VideoSurfaceWidget.h"

#include <QOpenGLShaderProgram>

#include <algorithm>
#include <array>

namespace mc::video {

namespace {
const char* kVertexShader = R"(
attribute vec2 aPos;
attribute vec2 aTex;
varying vec2 vTex;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    vTex = aTex;
}
)";

const char* kFragmentShader = R"(
varying vec2 vTex;
uniform sampler2D uTex;
void main() {
    gl_FragColor = texture2D(uTex, vTex);
}
)";
} // namespace

VideoSurfaceWidget::VideoSurfaceWidget(QWidget* parent) : QOpenGLWidget(parent) {}

VideoSurfaceWidget::~VideoSurfaceWidget() {
    close();
    makeCurrent();
    if (textureId_ != 0) {
        glDeleteTextures(1, &textureId_);
    }
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
    }
    doneCurrent();
}

bool VideoSurfaceWidget::open(const std::string& path, double startSeconds) {
    close();
    return decoder_.open(path, startSeconds);
}

void VideoSurfaceWidget::close() {
    decoder_.close();
    pendingFrame_.reset();
    hasFrame_ = false;
    textureWidth_ = 0;
    textureHeight_ = 0;
    update();
}

void VideoSurfaceWidget::syncToClock(double audioClockSeconds) {
    if (!decoder_.isOpen()) {
        return;
    }
    const auto pts = decoder_.queuedPtsSeconds();
    const int index = selectPresentationFrameIndex(pts, audioClockSeconds);
    if (index < 0) {
        return;
    }
    if (auto frame = decoder_.popThrough(index)) {
        pendingFrame_ = std::move(frame);
        update();
    }
}

bool VideoSurfaceWidget::isOpen() const {
    return decoder_.isOpen();
}

std::string VideoSurfaceWidget::lastError() const {
    return decoder_.lastError();
}

void VideoSurfaceWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    program_ = std::make_unique<QOpenGLShaderProgram>();
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, kVertexShader);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, kFragmentShader);
    program_->bindAttributeLocation("aPos", 0);
    program_->bindAttributeLocation("aTex", 1);
    program_->link();

    vao_.create();

    glGenTextures(1, &textureId_);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenBuffers(1, &vbo_);
}

void VideoSurfaceWidget::resizeGL(int, int) {
    updateQuadGeometry();
}

void VideoSurfaceWidget::updateQuadGeometry() {
    float sx = 1.0f;
    float sy = 1.0f;
    if (textureWidth_ > 0 && textureHeight_ > 0 && width() > 0 && height() > 0) {
        const float widgetAspect = static_cast<float>(width()) / static_cast<float>(height());
        const float frameAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (frameAspect > widgetAspect) {
            sy = widgetAspect / frameAspect;
        } else {
            sx = frameAspect / widgetAspect;
        }
    }
    // Triangle strip: bottom-left, bottom-right, top-left, top-right; v
    // flipped (1 at top) so decoded top-row-first RGB data isn't upside down.
    const std::array<float, 16> vertices = {
        -sx, -sy, 0.0f, 1.0f,
        sx,  -sy, 1.0f, 1.0f,
        -sx, sy,  0.0f, 0.0f,
        sx,  sy,  1.0f, 0.0f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<qint64>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
}

void VideoSurfaceWidget::uploadFrame(const DecodedFrame& frame) {
    glBindTexture(GL_TEXTURE_2D, textureId_);
    if (frame.width != textureWidth_ || frame.height != textureHeight_) {
        textureWidth_ = frame.width;
        textureHeight_ = frame.height;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, textureWidth_, textureHeight_, 0, GL_RGB, GL_UNSIGNED_BYTE, frame.rgb.data());
        updateQuadGeometry();
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth_, textureHeight_, GL_RGB, GL_UNSIGNED_BYTE, frame.rgb.data());
    }
    hasFrame_ = true;
}

void VideoSurfaceWidget::paintGL() {
    if (pendingFrame_) {
        uploadFrame(*pendingFrame_);
        pendingFrame_.reset();
    }

    glClear(GL_COLOR_BUFFER_BIT);
    if (!hasFrame_ || !program_ || !program_->isLinked()) {
        return;
    }

    QOpenGLVertexArrayObject::Binder vaoBinder(&vao_);
    program_->bind();
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId_);
    program_->setUniformValue("uTex", 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    program_->release();
}

} // namespace mc::video
