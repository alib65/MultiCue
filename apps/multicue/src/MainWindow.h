#pragma once

#include <QWidget>
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "mc/core/AutosaveScheduler.h"
#include "mc/core/Playlist.h"
#include "mc/engine/DeviceInfo.h"
#include "mc/engine/RealtimeAudioEngine.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class QSlider;
class QDoubleSpinBox;
class QSpinBox;
class QLineEdit;
class QCheckBox;
class QComboBox;
class QScrollArea;
class QTimer;

namespace mc::app {

class LevelMeterWidget;

// One mixer channel strip's widgets (built/rebuilt per selected piece, like
// the reference's makeStrip).
struct TrackStrip {
    QWidget* box = nullptr;
    QSpinBox* outputSpin = nullptr;
    QSlider* fader = nullptr;
    QDoubleSpinBox* gainSpin = nullptr;
    QPushButton* muteButton = nullptr;
    QPushButton* soloButton = nullptr;
    LevelMeterWidget* meter = nullptr;
    QLabel* peakLabel = nullptr;
};

// Playlist mode (M2): parity with the reference's direct-mode feature set,
// wired to mc::engine::RealtimeAudioEngine. Cue-list mode (QLab-style) is
// M5.
class MainWindow : public QWidget {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRefreshDevices();
    void onApplyOutputs();
    void onAddFiles();
    void onAddFolder();
    void onAddManyFolders();
    void onMoveUp();
    void onMoveDown();
    void onDuplicate();
    void onRemovePiece();
    void onNewPlaylist();
    void onOpenPlaylist();
    void onSaveAs();
    void onPlaylistRowChanged(int row);
    void onPlaylistItemActivated(QListWidgetItem* item);
    void onPrev();
    void onPlayPause();
    void onStop();
    void onNext();
    void onPositionSliderPressed();
    void onPositionSliderReleased();
    void onTitleEdited();
    void onTrimSliderMoved(int value);
    void onTrimSpinChanged(double db);
    void onAddTracksToSelected();
    void onAllFadersZero();
    void onMasterGainChanged(double db);
    void onLimiterToggled(bool checked);
    void onCeilingChanged(double db);
    void onAutoAdvanceToggled(bool checked);
    void onGapChanged(double seconds);
    void onPollTick();

private:
    double nowSeconds() const;
    void addPieceFromFilePaths(const std::vector<std::string>& paths, const QString& titleOverride = {});
    void addPieceFromFolder(const QString& folder);
    void startPieceAt(int index, double seconds);
    void onPieceFinished();
    void stopImmediately(); // blocking; for internal callers (New/Open/Remove), not the Stop button
    void beginGap();
    void rebuildMixer();
    void refreshPlaylistList();
    void refreshPieceHeader();
    void refreshTransportLabel();
    void markDirty();
    void doAutosave(const QString& path);
    void loadPlaylistFile(const QString& path);
    QString autosavePath() const;

    mc::core::Playlist playlist_;
    QString playlistPath_;
    int selectedIndex_ = -1;
    int currentIndex_ = -1;

    enum class Transport { Stopped, Playing, Paused, Gap };
    Transport transport_ = Transport::Stopped;
    double gapRemainingSeconds_ = 0.0;
    double gapDeadlineSeconds_ = 0.0; // absolute nowSeconds() value when the gap ends

    std::vector<mc::engine::DeviceInfo> devices_;
    std::uint32_t selectedDeviceId_ = 0;
    int numOutputs_ = 2;
    std::unique_ptr<mc::engine::RealtimeAudioEngine> engine_;
    mc::core::AutosaveScheduler autosave_;
    std::chrono::steady_clock::time_point startTime_;

    bool positionDragging_ = false;
    bool awaitingStopFade_ = false; // true between the Stop button's requestStop() and the poll timer noticing the fade finished

    std::vector<TrackStrip> strips_;
    std::vector<float> trackPeakHold_;
    std::vector<float> outputPeakHold_;

    QSpinBox* outputsBox_ = nullptr;
    QComboBox* deviceCombo_ = nullptr;
    QPushButton* applyButton_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    QListWidget* playlistList_ = nullptr;
    QLabel* playlistInfoLabel_ = nullptr;

    QPushButton* prevButton_ = nullptr;
    QPushButton* playButton_ = nullptr;
    QPushButton* stopButton_ = nullptr;
    QPushButton* nextButton_ = nullptr;
    QLabel* stateLabel_ = nullptr;
    QCheckBox* autoAdvanceBox_ = nullptr;
    QDoubleSpinBox* gapBox_ = nullptr;
    QSlider* positionSlider_ = nullptr;
    QLabel* timeLabel_ = nullptr;

    QLineEdit* titleField_ = nullptr;
    QLabel* pieceInfoLabel_ = nullptr;
    QSlider* trimSlider_ = nullptr;
    QDoubleSpinBox* trimSpin_ = nullptr;

    QScrollArea* mixerScroll_ = nullptr;

    QSlider* masterFader_ = nullptr;
    QDoubleSpinBox* masterGainSpin_ = nullptr;
    QCheckBox* limiterBox_ = nullptr;
    QDoubleSpinBox* ceilingSpin_ = nullptr;
    QScrollArea* outMetersScroll_ = nullptr;
    std::vector<LevelMeterWidget*> outputMeters_;

    QTimer* pollTimer_ = nullptr;
};

} // namespace mc::app
