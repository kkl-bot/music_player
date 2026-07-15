#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>

#include "player.h"

#include <QPixmap>
#include <QResizeEvent>

class Playlist;
class PlayTrack;
class LyricsDecoder;
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
    void resizeEvent(QResizeEvent *event) override;

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
    /// 拖拽排序后从 QListWidget 顺序重建 Playlist
    void syncPlaylistFromUI();

    // ── 歌词 ──
    void updateLyrics(qint64 positionMs);
    void onLyricsScrolled(const QPoint &viewportPos = QPoint());
    void onLyricsPlayClicked();
    void onLyricsRevertTimeout();
    void moveLyricsButtonTo(const QPoint &viewportPos);
    void updateLyricsOffsetLabel();
    void onLyricsOffsetAdjust(qint64 deltaMs);
    void applyLyricsOffset();

    /// 封面自适应：将缓存的封面缩放到当前标签尺寸
    void updateCoverDisplay();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    // ── 核心组件 ──
    Player        *m_player        = nullptr;
    Playlist      *m_playlist      = nullptr;
    PlayTrack     *m_playTrack     = nullptr;
    LyricsDecoder *m_subtitle     = nullptr;
    Library       *m_library       = nullptr;

    // ── 左侧面板 ──
    QListWidget   *m_listWidget    = nullptr;
    QLabel        *m_listCountLabel = nullptr;

    // ── 右侧面板 ──
    QLabel        *m_songTitle     = nullptr;
    QLabel        *m_songArtist    = nullptr;
    QLabel        *m_albumArt      = nullptr;   // 预留专辑封面
    QListWidget   *m_lyricsWidget  = nullptr;   // 滚动歌词

    // ── 歌词时间微调 ──
    QPushButton   *m_btnLyricsOffsetDown = nullptr;
    QPushButton   *m_btnLyricsOffsetUp   = nullptr;
    QLabel        *m_lyricsOffsetLabel   = nullptr;

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

    // ── 歌词交互 ──
    QPushButton *m_lyricsPlayBtn  = nullptr;   // 歌词跳转按钮
    QTimer      *m_lyricsRevertTimer = nullptr; // 自动回弹定时器
    bool         m_lyricsFollowing = true;      // 歌词是否跟随播放
    bool         m_lyricsSuppressScroll = false;// 抑制程序化滚动触发检测
    QPoint       m_lyricsWheelPos;              // 最近一次鼠标滚轮位置（viewport 坐标）

    // ── 状态追踪 ──
    bool m_userDragging = false;        // 用户正在拖拽进度条
    bool m_manualTrackChange = false;   // 手动切歌中，防止 Stopped 额外触发 next
    QPixmap m_coverCache;               // 原始封面缓存，用于缩放
    qint64 m_storedPosition = 0;        // 拖拽时保存的真实位置
};

#endif // MAINWINDOW_H
