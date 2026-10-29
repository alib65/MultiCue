#pragma once

#include <QWidget>
#include <memory>
#include <vector>

#include "mc/engine/DeviceInfo.h"
#include "mc/engine/TestToneEngine.h"

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QTimer;

namespace mc::app {

// M0 skeleton window: pick a device and output channel, play a test tone.
// Playlist/cue functionality arrives in later milestones (see
// docs/ROADMAP.md).
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRefreshDevices();
    void onTogglePlay();
    void onPollStatus();

private:
    void applyDeviceChannelRange();

    std::vector<mc::engine::DeviceInfo> devices_;
    std::unique_ptr<mc::engine::TestToneEngine> engine_;

    QComboBox* deviceCombo_ = nullptr;
    QSpinBox* channelSpin_ = nullptr;
    QPushButton* refreshButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;
    QTimer* pollTimer_ = nullptr;
};

} // namespace mc::app
