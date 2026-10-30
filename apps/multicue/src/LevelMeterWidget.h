#pragma once

#include <QWidget>

namespace mc::app {

// A simple vertical VU-style level meter: current level (green/yellow/red
// by zone) plus a peak-hold line. Values are linear amplitude in [0, 1]
// (map dB with the same -60..0 dBFS convention the engine's meters use).
class LevelMeterWidget : public QWidget {
    Q_OBJECT

public:
    explicit LevelMeterWidget(QWidget* parent = nullptr);

    void setLevel(float linearAmplitude);
    void resetPeakHold();

    QSize sizeHint() const override { return QSize(14, 120); }
    QSize minimumSizeHint() const override { return QSize(8, 40); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    static float ampToMeter(float amp);

    float level_ = 0.0f;
    float peakHold_ = 0.0f;
};

} // namespace mc::app
