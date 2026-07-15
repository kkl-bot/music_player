#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>

#include "player.h"

class Playlist;
class PlayTrack;
class SubtitlePlayer;
class Library;
class Song;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUI();
    void setupMenuBar();
    void setupCentralWidget();
    void setupStyleSheet();

    void connectSignals();

    // ── 播放控制槽 ──
    void onPlayPause();
    void onNext();
    void onPrevious();
    void onSongSelected(int index);
    void onPlayerStateChanged(Player::PlaybackState state);
    void updateProgressUI(qint64 positionMs);
    void updateDurationUI(qint64 durationMs);
    void onVolumeChanged(int percent);
    void onToggleMute();
    void onToggleShuffle();
    void onToggleRepeat();
    void onSeek(int value);

    // ── 列表 / 库 ──
    void onFolderLoaded(const QList<Song> &songs);
    void onPlaylistCurrentChanged(int index);
    void restorePlaylistFromLibrary();

    // ── 歌词 ──
    void updateLyrics(qint64 positionMs);

    // ── 核心组件 ──
    Player        *m_player        = nullptr;
    Playlist      *m_playlist      = nullptr;
    PlayTrack     *m_playTrack     = nullptr;
    SubtitlePlayer *m_subtitle     = nullptr;
    Library       *m_library       = nullptr;

    // ── 左侧面板 ──
    QListWidget   *m_listWidget    = nullptr;
    QLabel        *m_listCountLabel = nullptr;

    // ── 右侧面板 ──
    QLabel        *m_songTitle     = nullptr;
    QLabel        *m_songArtist    = nullptr;
    QLabel        *m_albumArt      = nullptr;   // 预留专辑封面
    QLabel        *m_lyricsLabel   = nullptr;   // 当前歌词

    // ── 进度条 ──
    QSlider       *m_progressSlider = nullptr;
    QLabel        *m_timeCurrent   = nullptr;
    QLabel        *m_timeTotal     = nullptr;

    // ── 控制按钮 ──
    QPushButton   *m_btnPlayPause  = nullptr;
    QPushButton   *m_btnPrev       = nullptr;
    QPushButton   *m_btnNext       = nullptr;
    QPushButton   *m_btnShuffle    = nullptr;
    QPushButton   *m_btnRepeat     = nullptr;

    // ── 音量 ──
    QPushButton   *m_btnMute       = nullptr;
    QSlider       *m_volumeSlider  = nullptr;
    QLabel        *m_volumeLabel   = nullptr;

    // ── 状态追踪 ──
    bool m_userDragging = false;        // 用户正在拖拽进度条
    qint64 m_storedPosition = 0;        // 拖拽时保存的真实位置
};

#endif // MAINWINDOW_H
