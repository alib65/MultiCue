#include "MainWindow.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "mc/core/AppInfo.h"
#include "mc/core/Version.h"

namespace mc::app {

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent), engine_(std::make_unique<mc::engine::TestToneEngine>()) {
    setWindowTitle(QString::fromStdString(mc::core::versionString()));

    deviceCombo_ = new QComboBox(this);
    refreshButton_ = new QPushButton(tr("Refresh"), this);
    channelSpin_ = new QSpinBox(this);
    channelSpin_->setMinimum(1);
    channelSpin_->setMaximum(1);
    playButton_ = new QPushButton(tr("Play test tone"), this);
    statusLabel_ = new QLabel(tr("Select an output device."), this);
    statusLabel_->setWordWrap(true);

    auto* deviceRow = new QHBoxLayout();
    deviceRow->addWidget(new QLabel(tr("Output device:"), this));
    deviceRow->addWidget(deviceCombo_, 1);
    deviceRow->addWidget(refreshButton_);

    auto* channelRow = new QHBoxLayout();
    channelRow->addWidget(new QLabel(tr("Output channel:"), this));
    channelRow->addWidget(channelSpin_);
    channelRow->addStretch(1);
    channelRow->addWidget(playButton_);

    auto* root = new QVBoxLayout(this);
    root->addLayout(deviceRow);
    root->addLayout(channelRow);
    root->addWidget(statusLabel_);
    root->addStretch(1);

    connect(refreshButton_, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onTogglePlay);
    connect(deviceCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        applyDeviceChannelRange();
    });

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(200);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::onPollStatus);
    pollTimer_->start();

    onRefreshDevices();
    resize(420, 160);
}

MainWindow::~MainWindow() = default;

void MainWindow::onRefreshDevices() {
    const bool wasRunning = engine_->isRunning();
    engine_->stop();

    devices_ = mc::engine::listOutputDevices();
    deviceCombo_->clear();
    for (const auto& d : devices_) {
        deviceCombo_->addItem(QString::fromStdString(d.name) + (d.isDefaultOutput ? tr(" (default)") : QString()));
    }
    if (devices_.empty()) {
        statusLabel_->setText(tr("No audio output devices found."));
    } else {
        int defaultIndex = 0;
        for (std::size_t i = 0; i < devices_.size(); ++i) {
            if (devices_[i].isDefaultOutput) {
                defaultIndex = static_cast<int>(i);
                break;
            }
        }
        deviceCombo_->setCurrentIndex(defaultIndex);
        applyDeviceChannelRange();
        statusLabel_->setText(tr("%1 device(s) found.").arg(devices_.size()));
    }

    if (wasRunning) {
        playButton_->setText(tr("Play test tone"));
    }
}

void MainWindow::applyDeviceChannelRange() {
    const int index = deviceCombo_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= devices_.size()) {
        channelSpin_->setMaximum(1);
        return;
    }
    channelSpin_->setMaximum(static_cast<int>(devices_[static_cast<std::size_t>(index)].outputChannels));
    if (channelSpin_->value() > channelSpin_->maximum()) {
        channelSpin_->setValue(1);
    }
}

void MainWindow::onTogglePlay() {
    if (engine_->isRunning()) {
        engine_->stop();
        playButton_->setText(tr("Play test tone"));
        statusLabel_->setText(tr("Stopped."));
        return;
    }

    const int index = deviceCombo_->currentIndex();
    if (index < 0 || static_cast<std::size_t>(index) >= devices_.size()) {
        statusLabel_->setText(tr("No device selected."));
        return;
    }
    const auto& device = devices_[static_cast<std::size_t>(index)];
    const double sampleRate = device.preferredSampleRate > 0 ? static_cast<double>(device.preferredSampleRate) : 48000.0;
    const auto targetChannel = static_cast<std::uint32_t>(channelSpin_->value() - 1);

    const bool ok = engine_->start(device.id, device.outputChannels, targetChannel, sampleRate, 440.0, -20.0);
    if (ok) {
        playButton_->setText(tr("Stop"));
        statusLabel_->setText(tr("Playing 440 Hz on channel %1 of \"%2\".")
                                  .arg(channelSpin_->value())
                                  .arg(QString::fromStdString(device.name)));
    } else {
        statusLabel_->setText(tr("Could not open the stream: %1").arg(QString::fromStdString(engine_->lastError())));
    }
}

void MainWindow::onPollStatus() {
    if (!engine_->isRunning()) {
        return;
    }
    const auto underruns = engine_->underrunCount();
    if (underruns > 0) {
        statusLabel_->setText(tr("Playing -- %1 underrun(s) detected.").arg(underruns));
    }
}

} // namespace mc::app
