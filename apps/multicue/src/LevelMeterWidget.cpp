#include "LevelMeterWidget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace mc::app {

LevelMeterWidget::LevelMeterWidget(QWidget* parent) : QWidget(parent) {
    setMinimumWidth(8);
}

float LevelMeterWidget::ampToMeter(float amp) {
    const float db = 20.0f * std::log10(std::max(amp, 0.000001f));
    return std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f);
}

void LevelMeterWidget::setLevel(float linearAmplitude) {
    level_ = ampToMeter(linearAmplitude);
    peakHold_ = std::max(peakHold_, level_);
    update();
}

void LevelMeterWidget::resetPeakHold() {
    peakHold_ = level_;
    update();
}

void LevelMeterWidget::mousePressEvent(QMouseEvent* event) {
    resetPeakHold();
    event->accept();
}

void LevelMeterWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(20, 20, 20));

    const int h = height();
    const int filledHeight = static_cast<int>(level_ * static_cast<float>(h));
    const int warnY = static_cast<int>(h * (1.0f - 0.9f));
    const int critY = static_cast<int>(h * (1.0f - 0.99f));

    QRect filled(0, h - filledHeight, width(), filledHeight);
    for (int y = filled.top(); y < filled.bottom() + 1; ++y) {
        QColor c;
        if (y <= critY) {
            c = QColor(240, 75, 65);
        } else if (y <= warnY) {
            c = QColor(245, 205, 60);
        } else {
            c = QColor(70, 200, 110);
        }
        p.fillRect(0, y, width(), 1, c);
    }

    const int peakY = h - static_cast<int>(peakHold_ * static_cast<float>(h));
    p.fillRect(0, std::max(0, peakY - 1), width(), 2, QColor(230, 230, 230));
}

} // namespace mc::app
