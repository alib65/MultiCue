#include "MainWindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

#include "LevelMeterWidget.h"
#include "mc/core/FileOrdering.h"
#include "mc/core/PlaylistIO.h"
#include "mc/core/Version.h"
#include "mc/engine/AudioFileProbe.h"
#include "mc/engine/SpatialLayout.h"

namespace mc::app {

namespace {

constexpr float kMinDb = -60.0f;
constexpr float kMaxDb = 12.0f;
constexpr float kTrimMinDb = -30.0f;
constexpr float kTrimMaxDb = 24.0f;
constexpr int kSliderSteps = 1000;

int dbToSlider(float db, float minDb, float maxDb) {
    const float t = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
    return static_cast<int>(std::lround(t * kSliderSteps));
}
float sliderToDb(int value, float minDb, float maxDb) {
    const float t = static_cast<float>(value) / static_cast<float>(kSliderSteps);
    return minDb + (t * (maxDb - minDb));
}

QString formatDb(float db) {
    if (db <= kMinDb) {
        return QStringLiteral("-inf");
    }
    return (db >= 0 ? QStringLiteral("+") : QString()) + QString::number(db, 'f', 1);
}

QString formatTime(double seconds) {
    const int total = static_cast<int>(std::max(0.0, std::floor(seconds)));
    return QStringLiteral("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QChar('0'));
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QWidget(parent), engine_(std::make_unique<mc::engine::RealtimeAudioEngine>()) {
    setWindowTitle(QString::fromStdString(mc::core::versionString()));
    startTime_ = std::chrono::steady_clock::now();

    // ---------------- top bar
    outputsBox_ = new QSpinBox(this);
    outputsBox_->setRange(1, 128);
    outputsBox_->setValue(numOutputs_);
    deviceCombo_ = new QComboBox(this);
    formatCombo_ = new QComboBox(this);
    formatCombo_->addItem(tr("Direct"));
    formatCombo_->addItem(tr("Ambisonics"));
    formatCombo_->addItem(tr("VBAP"));
    formatCombo_->addItem(tr("Stereo"));
    formatCombo_->addItem(tr("B-Format"));
    formatCombo_->addItem(tr("Binaural"));
    if (auto* model = qobject_cast<QStandardItemModel*>(formatCombo_->model())) {
        if (auto* binauralItem = model->item(5)) {
            binauralItem->setEnabled(false);
        }
    }
    formatCombo_->setItemData(5, tr("Binaural HRTF rendering is not implemented yet"), Qt::ToolTipRole);
    applyButton_ = new QPushButton(tr("Apply"), this);
    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    connect(applyButton_, &QPushButton::clicked, this, &MainWindow::onApplyOutputs);
    connect(deviceCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index >= 0 && static_cast<std::size_t>(index) < devices_.size()) {
            selectedDeviceId_ = devices_[static_cast<std::size_t>(index)].id;
        }
    });
    connect(formatCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        directMode_ = (index == 0);
        switch (index) {
            case 1: selectedFormat_ = mc::engine::ListeningFormat::Ambisonics; break;
            case 2: selectedFormat_ = mc::engine::ListeningFormat::Vbap; break;
            case 3: selectedFormat_ = mc::engine::ListeningFormat::Stereo; break;
            case 4: selectedFormat_ = mc::engine::ListeningFormat::BFormat; break;
            default: break; // Direct (0) and Binaural (5, disabled) don't set selectedFormat_
        }
        for (const auto& strip : strips_) {
            if (strip.outputSpin) strip.outputSpin->setEnabled(directMode_);
        }
    });

    showVideoButton_ = new QPushButton(tr("Show Video"), this);
    connect(showVideoButton_, &QPushButton::clicked, this, &MainWindow::onToggleVideoWindow);

    videoWindow_ = new QWidget(nullptr);
    videoWindow_->setWindowTitle(tr("MultiCue Video"));
    videoWindow_->resize(960, 540);
    videoWidget_ = new mc::video::VideoSurfaceWidget(videoWindow_);
    auto* videoWindowLayout = new QVBoxLayout(videoWindow_);
    videoWindowLayout->setContentsMargins(0, 0, 0, 0);
    videoWindowLayout->addWidget(videoWidget_);

    auto* topBar = new QHBoxLayout();
    topBar->addWidget(new QLabel(tr("Outputs"), this));
    topBar->addWidget(outputsBox_);
    topBar->addWidget(new QLabel(tr("  Device"), this));
    topBar->addWidget(deviceCombo_, 1);
    topBar->addWidget(new QLabel(tr("  Format"), this));
    topBar->addWidget(formatCombo_);
    topBar->addWidget(showVideoButton_);
    topBar->addWidget(applyButton_);
    topBar->addSpacing(12);
    topBar->addWidget(statusLabel_, 2);

    // ---------------- playlist panel (left)
    playlistList_ = new QListWidget(this);
    connect(playlistList_, &QListWidget::currentRowChanged, this, &MainWindow::onPlaylistRowChanged);
    connect(playlistList_, &QListWidget::itemActivated, this, &MainWindow::onPlaylistItemActivated);

    playlistInfoLabel_ = new QLabel(this);
    playlistInfoLabel_->setStyleSheet("color: #999;");

    auto* addRow = new QHBoxLayout();
    auto* addFilesBtn = new QPushButton(tr("+ Piece (files)..."), this);
    auto* addFolderBtn = new QPushButton(tr("+ Piece (folder)..."), this);
    auto* addManyBtn = new QPushButton(tr("+ Many pieces..."), this);
    connect(addFilesBtn, &QPushButton::clicked, this, &MainWindow::onAddFiles);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    connect(addManyBtn, &QPushButton::clicked, this, &MainWindow::onAddManyFolders);
    addRow->addWidget(addFilesBtn);
    addRow->addWidget(addFolderBtn);
    addRow->addWidget(addManyBtn);

    auto* editRow = new QHBoxLayout();
    auto* moveUpBtn = new QPushButton(tr("Move up"), this);
    auto* moveDownBtn = new QPushButton(tr("Move down"), this);
    auto* duplicateBtn = new QPushButton(tr("Duplicate"), this);
    auto* removeBtn = new QPushButton(tr("Remove"), this);
    connect(moveUpBtn, &QPushButton::clicked, this, &MainWindow::onMoveUp);
    connect(moveDownBtn, &QPushButton::clicked, this, &MainWindow::onMoveDown);
    connect(duplicateBtn, &QPushButton::clicked, this, &MainWindow::onDuplicate);
    connect(removeBtn, &QPushButton::clicked, this, &MainWindow::onRemovePiece);
    editRow->addWidget(moveUpBtn);
    editRow->addWidget(moveDownBtn);
    editRow->addWidget(duplicateBtn);
    editRow->addWidget(removeBtn);

    auto* fileRow = new QHBoxLayout();
    auto* newBtn = new QPushButton(tr("New playlist"), this);
    auto* openBtn = new QPushButton(tr("Open..."), this);
    auto* saveAsBtn = new QPushButton(tr("Save as..."), this);
    connect(newBtn, &QPushButton::clicked, this, &MainWindow::onNewPlaylist);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpenPlaylist);
    connect(saveAsBtn, &QPushButton::clicked, this, &MainWindow::onSaveAs);
    fileRow->addWidget(newBtn);
    fileRow->addWidget(openBtn);
    fileRow->addWidget(saveAsBtn);

    auto* leftPanel = new QWidget(this);
    leftPanel->setFixedWidth(380);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->addWidget(playlistInfoLabel_);
    leftLayout->addWidget(playlistList_, 1);
    leftLayout->addLayout(addRow);
    leftLayout->addLayout(editRow);
    leftLayout->addLayout(fileRow);

    // ---------------- transport
    prevButton_ = new QPushButton(tr("|< Prev"), this);
    playButton_ = new QPushButton(tr("Play"), this);
    stopButton_ = new QPushButton(tr("Stop"), this);
    nextButton_ = new QPushButton(tr("Next >|"), this);
    stateLabel_ = new QLabel(tr("STOPPED"), this);
    stateLabel_->setMinimumWidth(140);
    autoAdvanceBox_ = new QCheckBox(tr("Auto-advance"), this);
    autoAdvanceBox_->setChecked(true);
    gapBox_ = new QDoubleSpinBox(this);
    gapBox_->setRange(0.0, 60.0);
    gapBox_->setValue(3.0);
    gapBox_->setSuffix(tr(" s gap"));
    positionSlider_ = new QSlider(Qt::Horizontal, this);
    positionSlider_->setRange(0, kSliderSteps);
    timeLabel_ = new QLabel(QStringLiteral("0:00 / 0:00"), this);
    timeLabel_->setMinimumWidth(100);

    connect(prevButton_, &QPushButton::clicked, this, &MainWindow::onPrev);
    connect(playButton_, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(nextButton_, &QPushButton::clicked, this, &MainWindow::onNext);
    connect(autoAdvanceBox_, &QCheckBox::toggled, this, &MainWindow::onAutoAdvanceToggled);
    connect(gapBox_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onGapChanged);
    connect(positionSlider_, &QSlider::sliderPressed, this, &MainWindow::onPositionSliderPressed);
    connect(positionSlider_, &QSlider::sliderReleased, this, &MainWindow::onPositionSliderReleased);

    auto* transportRow = new QHBoxLayout();
    transportRow->addWidget(prevButton_);
    transportRow->addWidget(playButton_);
    transportRow->addWidget(stopButton_);
    transportRow->addWidget(nextButton_);
    transportRow->addSpacing(12);
    transportRow->addWidget(stateLabel_);
    transportRow->addStretch(1);
    transportRow->addWidget(autoAdvanceBox_);
    transportRow->addWidget(gapBox_);

    auto* positionRow = new QHBoxLayout();
    positionRow->addWidget(positionSlider_, 1);
    positionRow->addWidget(timeLabel_);

    // ---------------- piece header
    titleField_ = new QLineEdit(this);
    connect(titleField_, &QLineEdit::editingFinished, this, &MainWindow::onTitleEdited);
    pieceInfoLabel_ = new QLabel(this);
    pieceInfoLabel_->setStyleSheet("color: #999;");
    trimSlider_ = new QSlider(Qt::Horizontal, this);
    trimSlider_->setRange(0, kSliderSteps);
    trimSlider_->setFixedWidth(160);
    trimSpin_ = new QDoubleSpinBox(this);
    trimSpin_->setRange(kTrimMinDb, kTrimMaxDb);
    trimSpin_->setDecimals(1);
    trimSpin_->setSuffix(tr(" dB"));
    connect(trimSlider_, &QSlider::valueChanged, this, &MainWindow::onTrimSliderMoved);
    connect(trimSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onTrimSpinChanged);
    auto* addTracksBtn = new QPushButton(tr("+ Tracks..."), this);
    auto* allZeroBtn = new QPushButton(tr("All faders 0 dB"), this);
    connect(addTracksBtn, &QPushButton::clicked, this, &MainWindow::onAddTracksToSelected);
    connect(allZeroBtn, &QPushButton::clicked, this, &MainWindow::onAllFadersZero);

    addVideoButton_ = new QPushButton(tr("Video..."), this);
    auto* clearVideoBtn = new QPushButton(tr("Clear video"), this);
    videoStatusLabel_ = new QLabel(tr("No video"), this);
    videoStatusLabel_->setStyleSheet("color: #999;");
    connect(addVideoButton_, &QPushButton::clicked, this, &MainWindow::onAddVideo);
    connect(clearVideoBtn, &QPushButton::clicked, this, &MainWindow::onClearVideo);

    auto* headerRow1 = new QHBoxLayout();
    headerRow1->addWidget(new QLabel(tr("Piece"), this));
    headerRow1->addWidget(titleField_, 1);
    headerRow1->addWidget(addTracksBtn);
    headerRow1->addWidget(allZeroBtn);
    headerRow1->addWidget(addVideoButton_);
    headerRow1->addWidget(clearVideoBtn);
    auto* headerRow2 = new QHBoxLayout();
    headerRow2->addWidget(pieceInfoLabel_, 1);
    headerRow2->addWidget(videoStatusLabel_);
    headerRow2->addWidget(new QLabel(tr("Trim"), this));
    headerRow2->addWidget(trimSlider_);
    headerRow2->addWidget(trimSpin_);

    // ---------------- mixer + master
    mixerScroll_ = new QScrollArea(this);
    mixerScroll_->setWidgetResizable(true);
    mixerScroll_->setFrameShape(QFrame::NoFrame);

    masterFader_ = new QSlider(Qt::Vertical, this);
    masterFader_->setRange(0, kSliderSteps);
    masterFader_->setValue(dbToSlider(0.0f, kMinDb, kMaxDb));
    masterGainSpin_ = new QDoubleSpinBox(this);
    masterGainSpin_->setRange(kMinDb, kMaxDb);
    masterGainSpin_->setDecimals(1);
    masterGainSpin_->setSuffix(tr(" dB"));
    connect(masterFader_, &QSlider::valueChanged, this, [this](int v) {
        masterGainSpin_->blockSignals(true);
        masterGainSpin_->setValue(sliderToDb(v, kMinDb, kMaxDb));
        masterGainSpin_->blockSignals(false);
        onMasterGainChanged(masterGainSpin_->value());
    });
    connect(masterGainSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this](double db) {
        masterFader_->blockSignals(true);
        masterFader_->setValue(dbToSlider(static_cast<float>(db), kMinDb, kMaxDb));
        masterFader_->blockSignals(false);
        onMasterGainChanged(db);
    });

    limiterBox_ = new QCheckBox(tr("Safety limiter"), this);
    limiterBox_->setChecked(true);
    connect(limiterBox_, &QCheckBox::toggled, this, &MainWindow::onLimiterToggled);
    ceilingSpin_ = new QDoubleSpinBox(this);
    ceilingSpin_->setRange(-12.0, 0.0);
    ceilingSpin_->setValue(-0.5);
    ceilingSpin_->setDecimals(1);
    ceilingSpin_->setSuffix(tr(" dB ceiling"));
    connect(ceilingSpin_, qOverload<double>(&QDoubleSpinBox::valueChanged), this, &MainWindow::onCeilingChanged);

    outMetersScroll_ = new QScrollArea(this);
    outMetersScroll_->setWidgetResizable(true);
    outMetersScroll_->setFrameShape(QFrame::NoFrame);
    outMetersScroll_->setMinimumWidth(90);

    auto* masterPanel = new QWidget(this);
    masterPanel->setFixedWidth(220);
    auto* masterLayout = new QVBoxLayout(masterPanel);
    masterLayout->addWidget(new QLabel(tr("MASTER / OUTPUTS"), this));
    auto* masterRow = new QHBoxLayout();
    auto* masterFaderCol = new QVBoxLayout();
    masterFaderCol->addWidget(masterFader_, 1);
    masterFaderCol->addWidget(masterGainSpin_);
    masterRow->addLayout(masterFaderCol);
    masterRow->addWidget(outMetersScroll_, 1);
    masterLayout->addLayout(masterRow, 1);
    masterLayout->addWidget(limiterBox_);
    masterLayout->addWidget(ceilingSpin_);

    auto* mixerRow = new QHBoxLayout();
    mixerRow->addWidget(mixerScroll_, 1);
    mixerRow->addWidget(masterPanel);

    auto* rightLayout = new QVBoxLayout();
    rightLayout->addLayout(transportRow);
    rightLayout->addLayout(positionRow);
    rightLayout->addLayout(headerRow1);
    rightLayout->addLayout(headerRow2);
    rightLayout->addLayout(mixerRow, 1);

    auto* body = new QHBoxLayout();
    body->addWidget(leftPanel);
    body->addLayout(rightLayout, 1);

    auto* titleLabel = new QLabel(tr("MultiCue"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignHCenter);

    auto* authorLabel = new QLabel(tr("Ali Balighi © 2026 · alibalighi.com"), this);
    authorLabel->setAlignment(Qt::AlignHCenter);

    auto* root = new QVBoxLayout(this);
    root->addWidget(titleLabel);
    root->addWidget(authorLabel);
    root->addLayout(topBar);
    root->addLayout(body, 1);

    resize(1280, 860);

    pollTimer_ = new QTimer(this);
    pollTimer_->setInterval(33); // ~30 fps, matching the project brief's meter-rate target
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::onPollTick);
    pollTimer_->start();

    onRefreshDevices();

    const auto defaultPath = autosavePath();
    QFileInfo info(defaultPath);
    if (info.exists()) {
        loadPlaylistFile(defaultPath);
    } else {
        playlistPath_ = defaultPath;
    }
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
}

MainWindow::~MainWindow() {
    if (engine_) {
        engine_->unload();
    }
    videoWidget_->close();
    delete videoWindow_;
}

double MainWindow::nowSeconds() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
}

QString MainWindow::autosavePath() const {
    const auto dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/Autosave.mcplist";
}

void MainWindow::onRefreshDevices() {
    devices_ = mc::engine::listOutputDevices();
    deviceCombo_->clear();
    for (const auto& d : devices_) {
        deviceCombo_->addItem(QString::fromStdString(d.name) + (d.isDefaultOutput ? tr(" (default)") : QString()));
    }
    if (devices_.empty()) {
        statusLabel_->setText(tr("No audio output devices found."));
        return;
    }
    std::size_t defaultIndex = 0;
    for (std::size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].isDefaultOutput) {
            defaultIndex = i;
            break;
        }
    }
    deviceCombo_->setCurrentIndex(static_cast<int>(defaultIndex));
    selectedDeviceId_ = devices_[defaultIndex].id;
    statusLabel_->setText(tr("%1 device(s) found.").arg(devices_.size()));
}

void MainWindow::onApplyOutputs() {
    numOutputs_ = outputsBox_->value();
    if (engine_->isLoaded()) {
        engine_->unload();
        videoWidget_->close();
        transport_ = Transport::Stopped;
        currentIndex_ = -1;
        refreshPlaylistList();
    }
    rebuildMixer();
    statusLabel_->setText(tr("%1 output(s) on \"%2\".")
                              .arg(numOutputs_)
                              .arg(deviceCombo_->currentText()));
    refreshTransportLabel();
}

// ---------------- adding pieces

void MainWindow::addPieceFromFilePaths(const std::vector<std::string>& paths, const QString& titleOverride) {
    std::vector<mc::core::ProbedFile> probed;
    for (const auto& p : paths) {
        const auto info = mc::engine::probeAudioFile(p);
        if (info.valid) {
            probed.push_back({p, info.channels, info.sampleRate, info.durationSeconds});
        }
    }
    if (probed.empty()) {
        statusLabel_->setText(tr("No readable audio files were found."));
        return;
    }
    std::sort(probed.begin(), probed.end(), [](const mc::core::ProbedFile& a, const mc::core::ProbedFile& b) {
        return mc::core::naturalSortKey(a.path) < mc::core::naturalSortKey(b.path);
    });

    mc::core::Piece piece;
    piece.title = titleOverride.isEmpty()
        ? QString::fromStdString(mc::core::guessPieceTitle([&] {
              std::vector<std::string> onlyPaths;
              for (const auto& f : probed) onlyPaths.push_back(f.path);
              return onlyPaths;
          }())).toStdString()
        : titleOverride.toStdString();
    piece.tracks = mc::core::expandTracksFromProbedFiles(probed, 0);
    mc::engine::applyLayoutToPiece(piece, piece.layoutIdx);

    playlist_.pieces.push_back(piece);
    selectedIndex_ = static_cast<int>(playlist_.pieces.size()) - 1;
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
    markDirty();

    if (piece.tracks.size() > static_cast<std::size_t>(numOutputs_)) {
        statusLabel_->setText(tr("\"%1\" has %2 track(s) but only %3 output(s).")
                                  .arg(QString::fromStdString(piece.title))
                                  .arg(piece.tracks.size())
                                  .arg(numOutputs_));
    } else {
        statusLabel_->setText(tr("Added \"%1\" (%2 track(s)).").arg(QString::fromStdString(piece.title)).arg(piece.tracks.size()));
    }
}

void MainWindow::addPieceFromFolder(const QString& folder) {
    QDir dir(folder);
    auto entries = dir.entryList(QDir::Files, QDir::Name);
    std::vector<std::string> paths;
    for (const auto& e : entries) {
        paths.push_back(dir.filePath(e).toStdString());
    }
    if (paths.empty()) {
        statusLabel_->setText(tr("No files in %1").arg(folder));
        return;
    }
    addPieceFromFilePaths(paths, QFileInfo(folder).fileName());
}

void MainWindow::onAddFiles() {
    const auto files = QFileDialog::getOpenFileNames(this, tr("Add piece: select the mono files of one piece"));
    if (files.isEmpty()) {
        return;
    }
    std::vector<std::string> paths;
    for (const auto& f : files) {
        paths.push_back(f.toStdString());
    }
    addPieceFromFilePaths(paths);
}

void MainWindow::onAddFolder() {
    const auto folder = QFileDialog::getExistingDirectory(this, tr("Add piece: select a folder of mono files"));
    if (folder.isEmpty()) {
        return;
    }
    addPieceFromFolder(folder);
}

void MainWindow::onAddManyFolders() {
    const auto parent = QFileDialog::getExistingDirectory(this, tr("Add many pieces: select a folder containing one sub-folder per piece"));
    if (parent.isEmpty()) {
        return;
    }
    QDir dir(parent);
    auto subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    std::sort(subdirs.begin(), subdirs.end(), [](const QString& a, const QString& b) {
        return mc::core::naturalSortKey(a.toStdString()) < mc::core::naturalSortKey(b.toStdString());
    });
    int added = 0;
    for (const auto& sub : subdirs) {
        const auto before = playlist_.pieces.size();
        addPieceFromFolder(dir.filePath(sub));
        if (playlist_.pieces.size() > before) {
            ++added;
        }
    }
    if (added == 0) {
        addPieceFromFolder(parent);
    } else {
        statusLabel_->setText(tr("Added %1 piece(s).").arg(added));
    }
}

// ---------------- playlist editing

void MainWindow::onMoveUp() {
    if (selectedIndex_ <= 0) {
        return;
    }
    std::swap(playlist_.pieces[static_cast<std::size_t>(selectedIndex_)], playlist_.pieces[static_cast<std::size_t>(selectedIndex_ - 1)]);
    if (currentIndex_ == selectedIndex_) {
        currentIndex_--;
    } else if (currentIndex_ == selectedIndex_ - 1) {
        currentIndex_++;
    }
    selectedIndex_--;
    refreshPlaylistList();
    markDirty();
}

void MainWindow::onMoveDown() {
    if (selectedIndex_ < 0 || selectedIndex_ >= static_cast<int>(playlist_.pieces.size()) - 1) {
        return;
    }
    std::swap(playlist_.pieces[static_cast<std::size_t>(selectedIndex_)], playlist_.pieces[static_cast<std::size_t>(selectedIndex_ + 1)]);
    if (currentIndex_ == selectedIndex_) {
        currentIndex_++;
    } else if (currentIndex_ == selectedIndex_ + 1) {
        currentIndex_--;
    }
    selectedIndex_++;
    refreshPlaylistList();
    markDirty();
}

void MainWindow::onDuplicate() {
    if (selectedIndex_ < 0) {
        return;
    }
    auto copy = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
    copy.title += " (copy)";
    playlist_.pieces.insert(playlist_.pieces.begin() + selectedIndex_ + 1, copy);
    if (currentIndex_ > selectedIndex_) {
        currentIndex_++;
    }
    selectedIndex_++;
    refreshPlaylistList();
    markDirty();
}

void MainWindow::onRemovePiece() {
    if (selectedIndex_ < 0) {
        return;
    }
    if (selectedIndex_ == currentIndex_) {
        stopImmediately();
    }
    playlist_.pieces.erase(playlist_.pieces.begin() + selectedIndex_);
    if (currentIndex_ > selectedIndex_) {
        currentIndex_--;
    }
    selectedIndex_ = playlist_.pieces.empty() ? -1 : std::min(selectedIndex_, static_cast<int>(playlist_.pieces.size()) - 1);
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
    markDirty();
}

void MainWindow::onNewPlaylist() {
    stopImmediately();
    playlist_ = mc::core::Playlist{};
    selectedIndex_ = -1;
    playlistPath_ = autosavePath();
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
    markDirty();
}

void MainWindow::loadPlaylistFile(const QString& path) {
    const auto result = mc::core::loadPlaylistFromFile(path.toStdString());
    if (!result.success) {
        statusLabel_->setText(tr("Could not load \"%1\": %2").arg(path).arg(QString::fromStdString(result.error)));
        return;
    }
    stopImmediately();
    playlist_ = result.playlist;
    playlistPath_ = path;
    selectedIndex_ = playlist_.pieces.empty() ? -1 : 0;
    autoAdvanceBox_->setChecked(playlist_.settings.autoAdvance);
    gapBox_->setValue(playlist_.settings.gapSeconds);
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
}

void MainWindow::onOpenPlaylist() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Open playlist"), {}, tr("MultiCue playlists (*.mcplist)"));
    if (path.isEmpty()) {
        return;
    }
    loadPlaylistFile(path);
}

void MainWindow::onSaveAs() {
    auto path = QFileDialog::getSaveFileName(this, tr("Save playlist as"), {}, tr("MultiCue playlists (*.mcplist)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(".mcplist")) {
        path += ".mcplist";
    }
    std::string error;
    if (!mc::core::savePlaylistToFile(playlist_, path.toStdString(), error)) {
        statusLabel_->setText(tr("Save failed: %1").arg(QString::fromStdString(error)));
        return;
    }
    playlistPath_ = path;
    statusLabel_->setText(tr("Playlist saved: %1").arg(path));
}

// ---------------- selection / list

void MainWindow::refreshPlaylistList() {
    playlistList_->blockSignals(true);
    playlistList_->clear();
    for (std::size_t i = 0; i < playlist_.pieces.size(); ++i) {
        const auto& p = playlist_.pieces[i];
        QString mark = "    ";
        if (static_cast<int>(i) == currentIndex_) {
            mark = (transport_ == Transport::Paused) ? "|| " : "> ";
        }
        bool missing = false;
        for (const auto& t : p.tracks) {
            if (!QFileInfo::exists(QString::fromStdString(t.path))) {
                missing = true;
                break;
            }
        }
        const auto duration = mc::core::pieceDurationSeconds(p);
        QString text = QStringLiteral("%1%2. %3   (%4 tr - %5)%6")
                           .arg(mark)
                           .arg(i + 1)
                           .arg(QString::fromStdString(p.title))
                           .arg(p.tracks.size())
                           .arg(formatTime(duration))
                           .arg(missing ? tr("  [missing file]") : QString());
        playlistList_->addItem(text);
    }
    if (selectedIndex_ >= 0 && selectedIndex_ < playlistList_->count()) {
        playlistList_->setCurrentRow(selectedIndex_);
    }
    playlistList_->blockSignals(false);
    playlistInfoLabel_->setText(tr("%1 piece(s)").arg(playlist_.pieces.size()));
}

void MainWindow::onPlaylistRowChanged(int row) {
    if (row == selectedIndex_) {
        return;
    }
    selectedIndex_ = row;
    refreshPieceHeader();
    rebuildMixer();
}

void MainWindow::onPlaylistItemActivated(QListWidgetItem*) {
    if (awaitingStopFade_) {
        return;
    }
    if (selectedIndex_ >= 0) {
        startPieceAt(selectedIndex_, 0.0);
    }
}

// ---------------- transport

void MainWindow::startPieceAt(int index, double seconds) {
    if (index < 0 || index >= static_cast<int>(playlist_.pieces.size())) {
        return;
    }
    const auto& piece = playlist_.pieces[static_cast<std::size_t>(index)];
    if (piece.tracks.empty()) {
        statusLabel_->setText(tr("This piece has no tracks."));
        return;
    }
    std::vector<std::string> paths;
    std::vector<int> outs;
    for (const auto& t : piece.tracks) {
        paths.push_back(t.path);
        outs.push_back(t.outputChannel);
    }
    if (!engine_->loadPiece(selectedDeviceId_, paths, static_cast<std::size_t>(numOutputs_), outs, seconds,
                             directMode_ ? nullptr : &piece,
                             directMode_ ? std::nullopt : std::optional(selectedFormat_))) {
        statusLabel_->setText(tr("Could not play \"%1\": %2")
                                  .arg(QString::fromStdString(piece.title))
                                  .arg(QString::fromStdString(engine_->lastError())));
        return;
    }
    for (std::size_t i = 0; i < piece.tracks.size(); ++i) {
        const auto& t = piece.tracks[i];
        engine_->setTrackGainDb(i, t.gainDb);
        engine_->setTrackMuted(i, t.mute);
        engine_->setTrackSolo(i, t.solo);
    }
    engine_->setMasterGainDb(static_cast<float>(masterGainSpin_->value()));
    engine_->setCeilingDb(static_cast<float>(ceilingSpin_->value()));
    engine_->setLimiterEnabled(limiterBox_->isChecked());
    engine_->play();

    if (!piece.videoPath.empty()) {
        if (!videoWidget_->open(piece.videoPath, seconds)) {
            statusLabel_->setText(tr("Could not open video \"%1\": %2")
                                      .arg(QFileInfo(QString::fromStdString(piece.videoPath)).fileName())
                                      .arg(QString::fromStdString(videoWidget_->lastError())));
        }
    } else {
        videoWidget_->close();
    }

    currentIndex_ = index;
    selectedIndex_ = index;
    transport_ = Transport::Playing;
    refreshPlaylistList();
    rebuildMixer();
    refreshTransportLabel();
}

void MainWindow::onPrev() {
    if (awaitingStopFade_) {
        return; // let the pending stop's release fade finish first
    }
    if (transport_ == Transport::Playing || transport_ == Transport::Paused) {
        if (engine_->positionSeconds() > 3.0 || currentIndex_ <= 0) {
            startPieceAt(currentIndex_, 0.0);
        } else {
            startPieceAt(currentIndex_ - 1, 0.0);
        }
    } else if (selectedIndex_ > 0) {
        selectedIndex_--;
        refreshPlaylistList();
        refreshPieceHeader();
        rebuildMixer();
    }
}

void MainWindow::onNext() {
    if (awaitingStopFade_) {
        return;
    }
    if (transport_ == Transport::Playing || transport_ == Transport::Paused) {
        if (currentIndex_ + 1 < static_cast<int>(playlist_.pieces.size())) {
            startPieceAt(currentIndex_ + 1, 0.0);
        }
    } else if (selectedIndex_ + 1 < static_cast<int>(playlist_.pieces.size())) {
        selectedIndex_++;
        refreshPlaylistList();
        refreshPieceHeader();
        rebuildMixer();
    }
}

void MainWindow::onPlayPause() {
    if (awaitingStopFade_) {
        return;
    }
    switch (transport_) {
        case Transport::Playing:
            engine_->pause();
            transport_ = Transport::Paused;
            refreshTransportLabel();
            refreshPlaylistList();
            break;
        case Transport::Paused:
            if (currentIndex_ == selectedIndex_) {
                engine_->play();
                transport_ = Transport::Playing;
                refreshTransportLabel();
                refreshPlaylistList();
            } else {
                startPieceAt(selectedIndex_, 0.0);
            }
            break;
        case Transport::Gap:
        case Transport::Stopped:
            if (selectedIndex_ >= 0) {
                startPieceAt(selectedIndex_, 0.0);
            }
            break;
    }
}

void MainWindow::onStop() {
    // Non-blocking: requestStop() triggers the release fade and returns
    // immediately so the UI thread never freezes on the Stop button;
    // onPollTick() finishes the teardown once the fade completes. Internal
    // callers that need the old piece fully gone *right now* (New/Open/
    // Remove) use the blocking engine_->stop() directly instead.
    if (engine_->isLoaded()) {
        engine_->requestStop();
        awaitingStopFade_ = true;
    } else {
        transport_ = Transport::Stopped;
        currentIndex_ = -1;
    }
    refreshPlaylistList();
    refreshTransportLabel();
}

void MainWindow::stopImmediately() {
    if (engine_->isLoaded()) {
        engine_->stop(); // blocking: acceptable for New/Open/Remove, not for the Stop button
    }
    videoWidget_->close();
    awaitingStopFade_ = false;
    transport_ = Transport::Stopped;
    currentIndex_ = -1;
}

void MainWindow::onPieceFinished() {
    const int finished = currentIndex_;
    engine_->unload();
    videoWidget_->close();
    currentIndex_ = -1;
    const int next = finished + 1;
    if (autoAdvanceBox_->isChecked() && next < static_cast<int>(playlist_.pieces.size())) {
        beginGap();
        selectedIndex_ = next;
        refreshPlaylistList();
        refreshPieceHeader();
        rebuildMixer();
    } else {
        transport_ = Transport::Stopped;
        refreshPlaylistList();
    }
    refreshTransportLabel();
}

void MainWindow::beginGap() {
    transport_ = Transport::Gap;
    gapDeadlineSeconds_ = nowSeconds() + gapBox_->value();
    gapRemainingSeconds_ = gapBox_->value();
}

void MainWindow::onPositionSliderPressed() {
    positionDragging_ = true;
}

void MainWindow::onPositionSliderReleased() {
    positionDragging_ = false;
    const int idx = (transport_ == Transport::Playing || transport_ == Transport::Paused) ? currentIndex_ : selectedIndex_;
    if (idx < 0) {
        return;
    }
    const double duration = mc::core::pieceDurationSeconds(playlist_.pieces[static_cast<std::size_t>(idx)]);
    const double frac = static_cast<double>(positionSlider_->value()) / kSliderSteps;
    const double seconds = duration * frac;
    if (transport_ == Transport::Playing) {
        startPieceAt(idx, seconds);
    } else if (transport_ == Transport::Paused) {
        engine_->unload();
        videoWidget_->close();
        transport_ = Transport::Stopped;
        currentIndex_ = -1;
        refreshTransportLabel();
    }
}

void MainWindow::onAutoAdvanceToggled(bool checked) {
    playlist_.settings.autoAdvance = checked;
    markDirty();
}

void MainWindow::onGapChanged(double seconds) {
    playlist_.settings.gapSeconds = seconds;
    markDirty();
}

// ---------------- piece header

void MainWindow::refreshPieceHeader() {
    const bool hasSelection = selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(playlist_.pieces.size());
    titleField_->setEnabled(hasSelection);
    trimSlider_->setEnabled(hasSelection);
    trimSpin_->setEnabled(hasSelection);
    if (!hasSelection) {
        titleField_->setText({});
        pieceInfoLabel_->setText({});
        videoStatusLabel_->setText(tr("No video"));
        return;
    }
    const auto& p = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
    titleField_->setText(QString::fromStdString(p.title));
    trimSlider_->blockSignals(true);
    trimSlider_->setValue(dbToSlider(p.trimDb, kTrimMinDb, kTrimMaxDb));
    trimSlider_->blockSignals(false);
    trimSpin_->blockSignals(true);
    trimSpin_->setValue(p.trimDb);
    trimSpin_->blockSignals(false);
    pieceInfoLabel_->setText(tr("%1 track(s) - %2").arg(p.tracks.size()).arg(formatTime(mc::core::pieceDurationSeconds(p))));
    videoStatusLabel_->setText(p.videoPath.empty()
                                    ? tr("No video")
                                    : tr("Video: %1").arg(QFileInfo(QString::fromStdString(p.videoPath)).fileName()));
}

void MainWindow::onTitleEdited() {
    if (selectedIndex_ < 0) {
        return;
    }
    playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].title = titleField_->text().toStdString();
    refreshPlaylistList();
    markDirty();
}

void MainWindow::onTrimSliderMoved(int value) {
    if (selectedIndex_ < 0) {
        return;
    }
    const float db = sliderToDb(value, kTrimMinDb, kTrimMaxDb);
    trimSpin_->blockSignals(true);
    trimSpin_->setValue(db);
    trimSpin_->blockSignals(false);
    playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].trimDb = db;
    markDirty();
}

void MainWindow::onTrimSpinChanged(double db) {
    if (selectedIndex_ < 0) {
        return;
    }
    trimSlider_->blockSignals(true);
    trimSlider_->setValue(dbToSlider(static_cast<float>(db), kTrimMinDb, kTrimMaxDb));
    trimSlider_->blockSignals(false);
    playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].trimDb = static_cast<float>(db);
    markDirty();
}

void MainWindow::onAddTracksToSelected() {
    if (selectedIndex_ < 0) {
        return;
    }
    if (selectedIndex_ == currentIndex_) {
        statusLabel_->setText(tr("Stop this piece before adding tracks to it."));
        return;
    }
    const auto files = QFileDialog::getOpenFileNames(this, tr("Add tracks to this piece"));
    if (files.isEmpty()) {
        return;
    }
    auto& piece = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
    std::vector<mc::core::ProbedFile> probed;
    for (const auto& f : files) {
        const auto info = mc::engine::probeAudioFile(f.toStdString());
        if (info.valid) {
            probed.push_back({f.toStdString(), info.channels, info.sampleRate, info.durationSeconds});
        }
    }
    const auto newTracks = mc::core::expandTracksFromProbedFiles(probed, static_cast<int>(piece.tracks.size()));
    piece.tracks.insert(piece.tracks.end(), newTracks.begin(), newTracks.end());
    refreshPlaylistList();
    refreshPieceHeader();
    rebuildMixer();
    markDirty();
}

void MainWindow::onAddVideo() {
    if (selectedIndex_ < 0) {
        return;
    }
    const auto file = QFileDialog::getOpenFileName(this, tr("Attach a video to this piece"), {},
                                                     tr("Video files (*.mp4 *.mov *.mkv *.webm *.avi *.m4v)"));
    if (file.isEmpty()) {
        return;
    }
    playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].videoPath = file.toStdString();
    refreshPieceHeader();
    markDirty();
}

void MainWindow::onClearVideo() {
    if (selectedIndex_ < 0) {
        return;
    }
    playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].videoPath.clear();
    if (selectedIndex_ == currentIndex_) {
        videoWidget_->close();
    }
    refreshPieceHeader();
    markDirty();
}

void MainWindow::onToggleVideoWindow() {
    videoWindow_->setVisible(!videoWindow_->isVisible());
    if (videoWindow_->isVisible()) {
        videoWindow_->raise();
    }
}

void MainWindow::onAllFadersZero() {
    if (selectedIndex_ < 0) {
        return;
    }
    auto& piece = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
    for (auto& t : piece.tracks) {
        t.gainDb = 0.0f;
    }
    rebuildMixer();
    if (selectedIndex_ == currentIndex_) {
        for (std::size_t i = 0; i < piece.tracks.size(); ++i) {
            engine_->setTrackGainDb(i, 0.0f);
        }
    }
    markDirty();
}

// ---------------- mixer

void MainWindow::rebuildMixer() {
    strips_.clear();
    trackPeakHold_.clear();
    auto* canvas = new QWidget();
    auto* hLayout = new QHBoxLayout(canvas);

    if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(playlist_.pieces.size())) {
        auto& piece = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
        for (std::size_t i = 0; i < piece.tracks.size(); ++i) {
            auto& t = piece.tracks[i];
            TrackStrip strip;
            strip.box = new QWidget();
            strip.box->setFixedWidth(100);
            auto* box = new QVBoxLayout(strip.box);

            auto* nameLabel = new QLabel(QFileInfo(QString::fromStdString(t.path)).fileName());
            nameLabel->setWordWrap(true);
            nameLabel->setFixedHeight(32);
            nameLabel->setStyleSheet("font-size: 10px;");
            box->addWidget(nameLabel);

            strip.outputSpin = new QSpinBox();
            strip.outputSpin->setRange(1, numOutputs_);
            strip.outputSpin->setValue(std::clamp(t.outputChannel, 0, numOutputs_ - 1) + 1);
            strip.outputSpin->setPrefix(tr("out "));
            const std::size_t trackIndex = i;
            connect(strip.outputSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this, trackIndex](int v) {
                if (selectedIndex_ < 0) return;
                auto& pc = playlist_.pieces[static_cast<std::size_t>(selectedIndex_)];
                if (trackIndex >= pc.tracks.size()) return;
                pc.tracks[trackIndex].outputChannel = v - 1;
                if (selectedIndex_ == currentIndex_) {
                    engine_->setTrackOutputChannel(trackIndex, v - 1);
                }
                markDirty();
            });
            box->addWidget(strip.outputSpin);

            auto* meterFaderRow = new QHBoxLayout();
            strip.meter = new LevelMeterWidget();
            strip.fader = new QSlider(Qt::Vertical);
            strip.fader->setRange(0, kSliderSteps);
            strip.fader->setValue(dbToSlider(t.gainDb, kMinDb, kMaxDb));
            meterFaderRow->addWidget(strip.meter);
            meterFaderRow->addWidget(strip.fader);
            box->addLayout(meterFaderRow, 1);

            strip.gainSpin = new QDoubleSpinBox();
            strip.gainSpin->setRange(kMinDb, kMaxDb);
            strip.gainSpin->setDecimals(1);
            strip.gainSpin->setValue(t.gainDb);
            box->addWidget(strip.gainSpin);

            connect(strip.fader, &QSlider::valueChanged, this, [this, trackIndex](int v) {
                if (selectedIndex_ < 0 || trackIndex >= strips_.size()) return;
                const float db = sliderToDb(v, kMinDb, kMaxDb);
                strips_[trackIndex].gainSpin->blockSignals(true);
                strips_[trackIndex].gainSpin->setValue(db);
                strips_[trackIndex].gainSpin->blockSignals(false);
                playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].tracks[trackIndex].gainDb = db;
                if (selectedIndex_ == currentIndex_) {
                    engine_->setTrackGainDb(trackIndex, db);
                }
                markDirty();
            });
            connect(strip.gainSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, trackIndex](double db) {
                if (selectedIndex_ < 0 || trackIndex >= strips_.size()) return;
                strips_[trackIndex].fader->blockSignals(true);
                strips_[trackIndex].fader->setValue(dbToSlider(static_cast<float>(db), kMinDb, kMaxDb));
                strips_[trackIndex].fader->blockSignals(false);
                playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].tracks[trackIndex].gainDb = static_cast<float>(db);
                if (selectedIndex_ == currentIndex_) {
                    engine_->setTrackGainDb(trackIndex, static_cast<float>(db));
                }
                markDirty();
            });

            auto* muteSoloRow = new QHBoxLayout();
            strip.muteButton = new QPushButton(tr("M"));
            strip.muteButton->setCheckable(true);
            strip.muteButton->setChecked(t.mute);
            strip.muteButton->setFixedWidth(28);
            strip.soloButton = new QPushButton(tr("S"));
            strip.soloButton->setCheckable(true);
            strip.soloButton->setChecked(t.solo);
            strip.soloButton->setFixedWidth(28);
            connect(strip.muteButton, &QPushButton::toggled, this, [this, trackIndex](bool checked) {
                if (selectedIndex_ < 0) return;
                playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].tracks[trackIndex].mute = checked;
                if (selectedIndex_ == currentIndex_) {
                    engine_->setTrackMuted(trackIndex, checked);
                }
                markDirty();
            });
            connect(strip.soloButton, &QPushButton::toggled, this, [this, trackIndex](bool checked) {
                if (selectedIndex_ < 0) return;
                playlist_.pieces[static_cast<std::size_t>(selectedIndex_)].tracks[trackIndex].solo = checked;
                if (selectedIndex_ == currentIndex_) {
                    engine_->setTrackSolo(trackIndex, checked);
                }
                markDirty();
            });
            muteSoloRow->addWidget(strip.muteButton);
            muteSoloRow->addWidget(strip.soloButton);
            box->addLayout(muteSoloRow);

            strip.peakLabel = new QLabel(tr("--"));
            strip.peakLabel->setAlignment(Qt::AlignCenter);
            strip.peakLabel->setStyleSheet("font-size: 9px; color: #999;");
            box->addWidget(strip.peakLabel);

            hLayout->addWidget(strip.box);
            strips_.push_back(strip);
            trackPeakHold_.push_back(0.0f);
        }
    }
    hLayout->addStretch(1);
    if (auto* old = mixerScroll_->takeWidget()) {
        old->deleteLater();
    }
    mixerScroll_->setWidget(canvas);

    // output meters
    auto* meterCanvas = new QWidget();
    auto* meterLayout = new QHBoxLayout(meterCanvas);
    outputMeters_.clear();
    outputPeakHold_.assign(static_cast<std::size_t>(numOutputs_), 0.0f);
    for (int i = 0; i < numOutputs_; ++i) {
        auto* col = new QVBoxLayout();
        auto* m = new LevelMeterWidget();
        auto* label = new QLabel(QString::number(i + 1));
        label->setAlignment(Qt::AlignCenter);
        col->addWidget(m, 1);
        col->addWidget(label);
        meterLayout->addLayout(col);
        outputMeters_.push_back(m);
    }
    meterLayout->addStretch(1);
    if (auto* old = outMetersScroll_->takeWidget()) {
        old->deleteLater();
    }
    outMetersScroll_->setWidget(meterCanvas);
}

// ---------------- master

void MainWindow::onMasterGainChanged(double db) {
    engine_->setMasterGainDb(static_cast<float>(db));
    markDirty();
}

void MainWindow::onLimiterToggled(bool checked) {
    engine_->setLimiterEnabled(checked);
    markDirty();
}

void MainWindow::onCeilingChanged(double db) {
    engine_->setCeilingDb(static_cast<float>(db));
    markDirty();
}

// ---------------- misc

void MainWindow::refreshTransportLabel() {
    if (awaitingStopFade_) {
        stateLabel_->setText(tr("STOPPING..."));
        playButton_->setText(tr("Play"));
        return;
    }
    switch (transport_) {
        case Transport::Playing:
            stateLabel_->setText(tr("PLAYING %1").arg(currentIndex_ + 1));
            playButton_->setText(tr("Pause"));
            break;
        case Transport::Paused:
            stateLabel_->setText(tr("PAUSED %1").arg(currentIndex_ + 1));
            playButton_->setText(tr("Resume"));
            break;
        case Transport::Gap:
            stateLabel_->setText(tr("NEXT IN %1 s").arg(gapRemainingSeconds_, 0, 'f', 1));
            playButton_->setText(tr("Play"));
            break;
        case Transport::Stopped:
            stateLabel_->setText(tr("STOPPED"));
            playButton_->setText(tr("Play"));
            break;
    }
}

void MainWindow::markDirty() {
    autosave_.markDirty(nowSeconds());
}

void MainWindow::doAutosave(const QString& path) {
    std::string error;
    mc::core::savePlaylistToFile(playlist_, path.toStdString(), error);
}

void MainWindow::onPollTick() {
    const double now = nowSeconds();

    if (awaitingStopFade_ && engine_->isStopFadeComplete()) {
        engine_->unload();
        videoWidget_->close();
        awaitingStopFade_ = false;
        transport_ = Transport::Stopped;
        currentIndex_ = -1;
        refreshPlaylistList();
        refreshTransportLabel();
    }

    if (transport_ == Transport::Gap) {
        gapRemainingSeconds_ = gapDeadlineSeconds_ - now;
        refreshTransportLabel();
        if (gapRemainingSeconds_ <= 0.0 && selectedIndex_ >= 0) {
            startPieceAt(selectedIndex_, 0.0);
        }
    }

    if ((transport_ == Transport::Playing) && engine_->isLoaded()) {
        const double pos = engine_->positionSeconds();
        const double duration = (currentIndex_ >= 0) ? mc::core::pieceDurationSeconds(playlist_.pieces[static_cast<std::size_t>(currentIndex_)]) : 0.0;
        if (!positionDragging_) {
            positionSlider_->blockSignals(true);
            positionSlider_->setValue(duration > 0.0 ? static_cast<int>((pos / duration) * kSliderSteps) : 0);
            positionSlider_->blockSignals(false);
            timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(pos)).arg(formatTime(duration)));
        }
        if (duration > 0.0 && pos >= duration - 0.05) {
            onPieceFinished();
        }
    }

    if (videoWidget_->isOpen() && engine_->isLoaded()) {
        videoWidget_->syncToClock(engine_->positionSeconds());
    }

    // meters: only the currently-loaded piece's mixer has live numbers.
    const bool live = engine_->isLoaded() && selectedIndex_ == currentIndex_;
    for (std::size_t i = 0; i < strips_.size(); ++i) {
        const float peak = live ? engine_->trackPeak(i) : 0.0f;
        strips_[i].meter->setLevel(peak);
        trackPeakHold_[i] = std::max(trackPeakHold_[i] * 0.97f, peak);
        strips_[i].peakLabel->setText(trackPeakHold_[i] > 0.0001f ? formatDb(20.0f * std::log10(trackPeakHold_[i])) : tr("--"));
    }
    for (std::size_t i = 0; i < outputMeters_.size(); ++i) {
        const float peak = live ? engine_->outputPeak(i) : 0.0f;
        outputMeters_[i]->setLevel(peak);
    }

    if (autosave_.shouldSaveNow(now)) {
        doAutosave(playlistPath_);
    }
}

} // namespace mc::app
