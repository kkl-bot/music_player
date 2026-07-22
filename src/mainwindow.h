#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMenu>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QLineEdit>
#include <QMouseEvent>
#include <QWheelEvent>

#include "player.h"
#include "style.h"

#include <QPixmap>
#include <QResizeEvent>

class Playlist;
class PlayTrack;
class LyricsDecoder;
class Library;
class PlaylistManager;
class Song;
class ConversionDialog;
class AudioVisualizer;
class FleetEffects;

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


    void connectSignals();

    // ── 统一播放当前歌曲（onSongSelected / onNext / onPrevious 共用） ──
    void playCurrentSong();

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

    void onSeek(int value);

    // ── 列表 / 库 ──
    void onFolderLoaded(const QList<Song> &songs);
    void onPlaylistCurrentChanged(int index);
    void restorePlaylistFromLibrary();
    /// 拖拽排序后从 QListWidget 顺序重建 Playlist
    void syncPlaylistFromUI();
    /// 搜索过滤
    void onSearchTextChanged(const QString &text);

    // ── 歌单管理 ──
    /// 歌单选择器选中变化
    void onPlaylistSelectorSelected(int index);
    /// 新建歌单
    void onNewPlaylist();
    /// 删除当前选中的歌单
    void onDeletePlaylist();
    /// 歌曲列表点击（考虑分类模式）
    void onSongListItemClicked(int index);
    /// 右键菜单：添加到歌单
    void onAddToPlaylistMenu(const QString &songPath);
    /// 重建歌单选择器列表 UI
    void rebuildPlaylistSelector();
    /// 在重建后恢复歌单选择器选中项
    void restorePlaylistSelectorSelection();
    /// 根据当前视图重建歌曲列表
    void rebuildSongList();
    /// 显示分类列表（歌手/专辑）
    void showCategoryList(bool isArtist);
    /// 保存歌单数据到 Library
    void savePlaylistData();
    /// 从 Library 恢复歌单数据
    void restorePlaylistData();

    // ── 主题切换 ──
    void switchTheme(Style::Theme theme);
    void onToggleTheme();

    // ── DIY 背景 ──
    void onSelectBgFolder();
    void applyBackgrounds(const QString &folderPath);
    void clearBackgrounds();

    // ── 格式转换 ──
    void onShowConversionMenu(const QPoint &pos);
    void onConvertSong(const QString &filePath, const QString &title);

    // ── 播放列表切换 ──
    void onTogglePlaylist();

    // ── 播放顺序切换 ──
    void onToggleOrder();

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

    /// 更新艺术家/专辑显示
    void updateArtistAlbumDisplay();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

    // ── 核心组件 ──
    Player        *m_player        = nullptr;
    Playlist      *m_playlist      = nullptr;
    PlayTrack     *m_playTrack     = nullptr;
    LyricsDecoder *m_subtitle     = nullptr;
    Library       *m_library       = nullptr;
    PlaylistManager *m_playlistManager = nullptr;

    // ── 左侧面板（可切换） ──
    QWidget       *m_leftPanel      = nullptr;
    QListWidget   *m_playlistList   = nullptr;   // 歌单选择器（新增）
    QPushButton   *m_btnNewPlaylist = nullptr;   // 新建歌单按钮
    QPushButton   *m_btnDeletePlaylist = nullptr;// 删除歌单按钮
    QLineEdit     *m_searchBox      = nullptr;   // 搜索框
    QListWidget   *m_listWidget     = nullptr;
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
    QPushButton   *m_btnPlaylist   = nullptr;
    QPushButton   *m_btnVisualizer = nullptr;
    QPushButton   *m_btnPlayPause  = nullptr;
    QPushButton   *m_btnPrev       = nullptr;
    QPushButton   *m_btnNext       = nullptr;
    QPushButton   *m_btnOrder      = nullptr;

    // ── 唱片 + 歌名滚动 ──
    QLabel        *m_albumDisc     = nullptr;
    QLabel        *m_songMarquee   = nullptr;
    QTimer        *m_marqueeTimer  = nullptr;
    int            m_playOrder     = 0;   // 0=列表 1=单曲 2=随机
    int            m_marqueeOffset = 0;

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

    // ── 可视化 ──
    AudioVisualizer *m_visualizer = nullptr;

    // ── 主题菜单（在 setupMenuBar 中创建，供 switchTheme 直接访问） ──
    QMenu        *m_themeMenu   = nullptr;

    // ── Fleet-Snowfluff 特效 ──
    FleetEffects *m_fleetFx = nullptr;

    // ── 歌单视图状态 ──
    /// 列表显示模式
    enum class ListMode {
        AllSongs,       // 所有歌曲
        UserPlaylist,   // 用户歌单
        ByArtist,       // 按歌手（显示歌手名列表）
        ByAlbum,        // 按专辑（显示专辑名列表）
        ArtistSongs,    // 某歌手的歌曲
        AlbumSongs,     // 某专辑的歌曲
    };
    ListMode m_listMode = ListMode::AllSongs;
    /// 当前选中的歌单名 / 歌手名 / 专辑名（空 = 全部）
    QString m_currentFilterName;

    // ── 状态追踪 ──
    Style::Theme m_currentTheme = Style::Dark;
    QString m_diyBgFolder;
    bool m_userDragging = false;        // 用户正在拖拽进度条
    bool m_manualTrackChange = false;   // 手动切歌中，防止 Stopped 额外触发 next
    QPixmap m_coverCache;               // 原始封面缓存，用于缩放
    qint64 m_storedPosition = 0;        // 拖拽时保存的真实位置
};

#endif // MAINWINDOW_H
