#include "mainwindow.h"
#include "player.h"
#include "playlist.h"
#include "playtrack.h"
#include "lyricsdecoder.h"
#include "library.h"
#include "playlistmanager.h"
#include "song.h"
#include "conversiondialog.h"
#include "visualizer.h"
#include "fleeteffects.h"

#include <QInputDialog>
#include <QMessageBox>

/// 封面显示目标尺寸（px），保持各歌曲间一致
static const int kCoverSize = 280;

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QStatusBar>
#include <QCloseEvent>
#include <QTime>
#include <QFont>
#include <QFrame>
#include <QScrollBar>
#include <QAbstractItemView>
#include <QCursor>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <QImage>
#include <QPalette>

/// 返回 assets/icons/ 目录的绝对路径（从 exe 位置向上找两级到项目根目录）
static QString iconsDir()
{
    // exe 位于 build/Debug/MusicPlayer.exe ，向上两级到项目根目录
    QDir dir(QCoreApplication::applicationDirPath());
    dir.cdUp();
    dir.cdUp();
    return dir.absolutePath() + QStringLiteral("/assets/icons/");
}

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 创建核心组件
    m_player    = new Player(this);
    m_playlist  = new Playlist(this);
    m_playTrack = new PlayTrack(this);
    m_subtitle  = new LyricsDecoder(this);
    m_library   = new Library(this);
    m_playlistManager = new PlaylistManager(this);

    // PlayTrack 使用 Library 做缓存持久化
    m_playTrack->setLibrary(m_library);

    setupUI();
    connectSignals();

    // 恢复歌单数据
    restorePlaylistData();

    // 歌词回弹定时器（单次触发）
    m_lyricsRevertTimer = new QTimer(this);
    m_lyricsRevertTimer->setSingleShot(true);
    m_lyricsRevertTimer->setInterval(3000);
    connect(m_lyricsRevertTimer, &QTimer::timeout, this, &MainWindow::onLyricsRevertTimeout);

    // 歌词跳转按钮信号
    connect(m_lyricsPlayBtn, &QPushButton::clicked, this, &MainWindow::onLyricsPlayClicked);

    // 歌词时间微调
    connect(m_btnLyricsOffsetDown, &QPushButton::clicked, this, [this]() { onLyricsOffsetAdjust(-500); });
    connect(m_btnLyricsOffsetUp,   &QPushButton::clicked, this, [this]() { onLyricsOffsetAdjust(+500); });

    // 歌名滚动定时器
    m_marqueeTimer = new QTimer(this);
    m_marqueeTimer->setInterval(200);
    connect(m_marqueeTimer, &QTimer::timeout, this, [this]() {
        if (!m_songMarquee || m_songMarquee->text().length() < 15) return;
        QString text = m_songMarquee->text();
        m_marqueeOffset = (m_marqueeOffset + 1) % text.length();
        m_songMarquee->setText(text.mid(m_marqueeOffset) + text.left(m_marqueeOffset));
    });

    // 初始化 Fleet-Snowfluff 特效（主题依赖，需先创建）
    m_fleetFx = new FleetEffects(this);

    // 恢复上次状态
    m_currentTheme = static_cast<Style::Theme>(m_library->loadTheme(Style::Dark));
    qApp->setStyleSheet(Style::styleSheet(m_currentTheme));

    // 同步主题菜单勾选状态（setupMenuBar 只用了默认的 Dark）
    if (m_themeMenu) {
        const auto themeActions = m_themeMenu->actions();
        for (QAction *a : themeActions)
            a->setChecked(false);
        const int idx = static_cast<int>(m_currentTheme);
        if (idx >= 0 && idx < themeActions.size())
            themeActions[idx]->setChecked(true);
    }

    // 特效随主题联动
    if (m_currentTheme == Style::Fleet)
        m_fleetFx->start();

    m_diyBgFolder = m_library->loadDiyBgFolder();
    if (!m_diyBgFolder.isEmpty())
        applyBackgrounds(m_diyBgFolder);

    restorePlaylistFromLibrary();
    const int savedVol = m_library->loadVolume();
    m_player->setVolume(savedVol);
    m_volumeSlider->setValue(savedVol);
    m_player->setMuted(m_library->loadMuted());

    // 启动提示
    statusBar()->showMessage(
        QStringLiteral("视图 → 选择背景图片文件夹 可自定义界面背景"), 5000);
}

MainWindow::~MainWindow() = default;

// ════════════════════════════════════════════════════════════
//  关闭事件 — 保存状态
// ════════════════════════════════════════════════════════════

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_library->savePlaylist(m_playlist->allSongs(), m_playlist->currentIndex());
    m_library->saveVolume(m_player->volume());
    m_library->saveMuted(m_player->isMuted());
    m_library->saveRepeatMode(static_cast<int>(m_playlist->repeatMode()));
    m_library->saveShuffle(m_playlist->isShuffle());
    m_library->saveTheme(static_cast<int>(m_currentTheme));
    m_library->saveDiyBgFolder(m_diyBgFolder);
    m_library->saveWindowGeometry(saveGeometry());
    if (m_playlist->hasCurrent()) {
        const Song &cur = m_playlist->currentSong();
        m_library->saveLastPlayedSong(cur.filePath);
        m_library->saveLastPosition(cur.filePath, m_player->position());
        if (m_subtitle->isLoaded())
            m_library->saveLyricsOffset(cur.filePath, m_subtitle->userOffset());
    }

    // 保存歌单数据
    savePlaylistData();

    QMainWindow::closeEvent(event);
}

// ════════════════════════════════════════════════════════════
//  窗口缩放 — 重新缩放封面
// ════════════════════════════════════════════════════════════

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateCoverDisplay();
}

/// 将缓存的封面缩放到统一尺寸显示
void MainWindow::updateCoverDisplay()
{
    if (m_coverCache.isNull()) {
        // 无封面时仍保持占位区域大小一致，避免布局跳变
        QPixmap empty(kCoverSize, kCoverSize);
        empty.fill(Qt::transparent);
        m_albumArt->setPixmap(empty);
        return;
    }

    const QPixmap scaled = m_coverCache.scaled(
        kCoverSize, kCoverSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_albumArt->setPixmap(scaled);
}

// ════════════════════════════════════════════════════════════
//  UI 构建
// ════════════════════════════════════════════════════════════

void MainWindow::setupUI()
{
    setWindowTitle(QStringLiteral("Music Player"));
    setMinimumSize(900, 600);
    resize(1100, 700);

    setupMenuBar();
    setupCentralWidget();

    const QByteArray geom = m_library->loadWindowGeometry();
    if (!geom.isEmpty())
        restoreGeometry(geom);
}

void MainWindow::setupMenuBar()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    QAction *actOpenFolder = fileMenu->addAction(QStringLiteral("打开文件夹..."));
    actOpenFolder->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));
    connect(actOpenFolder, &QAction::triggered, this, [this]() {
        m_playTrack->browseAndLoad();
    });

    fileMenu->addSeparator();

    QAction *actClearCache = fileMenu->addAction(QStringLiteral("清空缓存"));
    connect(actClearCache, &QAction::triggered, this, [this]() {
        m_playTrack->clearCache();
        statusBar()->showMessage(QStringLiteral("缓存已清空"), 3000);
    });

    fileMenu->addSeparator();

    QAction *actExit = fileMenu->addAction(QStringLiteral("退出(&X)"));
    actExit->setShortcut(QKeySequence::Quit);
    connect(actExit, &QAction::triggered, this, &QWidget::close);

    QMenu *playMenu = menuBar()->addMenu(QStringLiteral("播放(&P)"));
    QAction *actPlayPause = playMenu->addAction(QStringLiteral("播放/暂停"));
    actPlayPause->setShortcut(QKeySequence(QStringLiteral("Space")));
    connect(actPlayPause, &QAction::triggered, this, &MainWindow::onPlayPause);

    QAction *actNext = playMenu->addAction(QStringLiteral("下一首"));
    actNext->setShortcut(QKeySequence(QStringLiteral("Ctrl+Right")));
    connect(actNext, &QAction::triggered, this, &MainWindow::onNext);

    QAction *actPrev = playMenu->addAction(QStringLiteral("上一首"));
    actPrev->setShortcut(QKeySequence(QStringLiteral("Ctrl+Left")));
    connect(actPrev, &QAction::triggered, this, &MainWindow::onPrevious);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    m_themeMenu = viewMenu->addMenu(QStringLiteral("主题"));
    auto addThemeAction = [&](const QString &label, Style::Theme theme) {
        QAction *act = m_themeMenu->addAction(label);
        act->setCheckable(true);
        act->setChecked(m_currentTheme == theme);
        connect(act, &QAction::triggered, this, [this, theme]() { switchTheme(theme); });
        return act;
    };
    addThemeAction(QStringLiteral("深色"),       Style::Dark);
    addThemeAction(QStringLiteral("亮色"),       Style::Light);
    addThemeAction(QStringLiteral("Fleet-Snowfluff"), Style::Fleet);

    viewMenu->addSeparator();

    QAction *actSelectBg = viewMenu->addAction(QStringLiteral("选择背景图片文件夹..."));
    actSelectBg->setToolTip(QStringLiteral(
        "将图片放入文件夹后选择，程序会按组件名自动匹配：\n"
        "leftPanel  → 左侧面板\n"
        "rightPanel → 右侧面板\n"
        "controlBar → 底部控制栏\n"
        "lyricsWidget → 歌词区域\n"
        "albumArt   → 专辑封面区\n"
        "支持 png / jpg / bmp / gif / webp 格式"));
    connect(actSelectBg, &QAction::triggered, this, &MainWindow::onSelectBgFolder);

    QAction *actClearBg = viewMenu->addAction(QStringLiteral("清除自定义背景"));
    actClearBg->setToolTip(QStringLiteral("恢复默认背景颜色"));
    connect(actClearBg, &QAction::triggered, this, &MainWindow::clearBackgrounds);
}

void MainWindow::setupCentralWidget()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 主分割器: 左 = 列表, 右 = 信息+歌词 ──
    QSplitter *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setHandleWidth(1);
    mainLayout->addWidget(splitter, 1);

    // ──── 左侧面板：播放列表 + 歌单管理（默认隐藏） ────
    m_leftPanel = new QWidget(this);
    m_leftPanel->setObjectName(QStringLiteral("leftPanel"));
    QVBoxLayout *leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    // 歌单选择器
    m_playlistList = new QListWidget(this);
    m_playlistList->setObjectName(QStringLiteral("playlistList"));
    m_playlistList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistList->setDragDropMode(QAbstractItemView::NoDragDrop);
    m_playlistList->setFixedHeight(160);
    leftLayout->addWidget(m_playlistList);

    // 新建 / 删除歌单按钮
    QHBoxLayout *playlistBtnLayout = new QHBoxLayout;
    playlistBtnLayout->setContentsMargins(8, 4, 8, 4);
    playlistBtnLayout->setSpacing(8);

    m_btnNewPlaylist = new QPushButton(QStringLiteral("＋ 新建歌单"), this);
    m_btnNewPlaylist->setObjectName(QStringLiteral("playlistBtn"));
    m_btnNewPlaylist->setFixedHeight(30);
    m_btnNewPlaylist->setCursor(Qt::PointingHandCursor);
    playlistBtnLayout->addWidget(m_btnNewPlaylist);

    m_btnDeletePlaylist = new QPushButton(QStringLiteral("✕ 删除"), this);
    m_btnDeletePlaylist->setObjectName(QStringLiteral("playlistBtn"));
    m_btnDeletePlaylist->setFixedHeight(30);
    m_btnDeletePlaylist->setCursor(Qt::PointingHandCursor);
    playlistBtnLayout->addWidget(m_btnDeletePlaylist);

    leftLayout->addLayout(playlistBtnLayout);

    // 搜索框
    m_searchBox = new QLineEdit(this);
    m_searchBox->setObjectName(QStringLiteral("searchBox"));
    m_searchBox->setPlaceholderText(QStringLiteral("搜索歌曲..."));
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setFixedHeight(40);
    leftLayout->addWidget(m_searchBox);

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    leftLayout->addWidget(m_listWidget, 1);

    m_listCountLabel = new QLabel(QStringLiteral("共 0 首"), this);
    m_listCountLabel->setObjectName(QStringLiteral("countLabel"));
    leftLayout->addWidget(m_listCountLabel);

    splitter->addWidget(m_leftPanel);
    m_leftPanel->setVisible(false);   // 初始隐藏

    // ──── 右侧面板：歌曲信息 + 歌词 ────
    QWidget *rightPanel = new QWidget(this);
    rightPanel->setObjectName(QStringLiteral("rightPanel"));
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 20, 20, 10);
    rightLayout->setSpacing(8);

    // 专辑封面占位 — 固定尺寸避免切歌时布局跳变
    m_albumArt = new QLabel(this);
    m_albumArt->setObjectName(QStringLiteral("albumArt"));
    m_albumArt->setFixedSize(kCoverSize, kCoverSize);
    m_albumArt->setAlignment(Qt::AlignCenter);
    m_albumArt->setText(QStringLiteral("🎵"));
    {
        QHBoxLayout *artRow = new QHBoxLayout;
        artRow->addStretch();
        artRow->addWidget(m_albumArt);
        artRow->addStretch();
        rightLayout->addLayout(artRow);
    }

    m_songTitle = new QLabel(QStringLiteral("—"), this);
    m_songTitle->setObjectName(QStringLiteral("songTitle"));
    m_songTitle->setAlignment(Qt::AlignCenter);
    m_songTitle->setWordWrap(true);
    rightLayout->addWidget(m_songTitle);

    m_songArtist = new QLabel(this);
    m_songArtist->setObjectName(QStringLiteral("songArtist"));
    m_songArtist->setAlignment(Qt::AlignCenter);
    rightLayout->addWidget(m_songArtist);

    rightLayout->addSpacing(4);

    // ── 歌词时间微调 ──
    {
        QHBoxLayout *offsetRow = new QHBoxLayout;
        offsetRow->setSpacing(4);
        offsetRow->addStretch();

        m_btnLyricsOffsetDown = new QPushButton(QStringLiteral("➖"), this);
        m_btnLyricsOffsetDown->setObjectName(QStringLiteral("lyricsAdjustBtn"));
        m_btnLyricsOffsetDown->setFixedSize(24, 24);
        m_btnLyricsOffsetDown->setToolTip(QStringLiteral("歌词提前 0.5 秒"));
        offsetRow->addWidget(m_btnLyricsOffsetDown);

        m_lyricsOffsetLabel = new QLabel(QStringLiteral("±0.0s"), this);
        m_lyricsOffsetLabel->setObjectName(QStringLiteral("lyricsOffsetLabel"));
        m_lyricsOffsetLabel->setAlignment(Qt::AlignCenter);
        m_lyricsOffsetLabel->setFixedWidth(44);
        offsetRow->addWidget(m_lyricsOffsetLabel);

        m_btnLyricsOffsetUp = new QPushButton(QStringLiteral("➕"), this);
        m_btnLyricsOffsetUp->setObjectName(QStringLiteral("lyricsAdjustBtn"));
        m_btnLyricsOffsetUp->setFixedSize(24, 24);
        m_btnLyricsOffsetUp->setToolTip(QStringLiteral("歌词延后 0.5 秒"));
        offsetRow->addWidget(m_btnLyricsOffsetUp);

        offsetRow->addStretch();
        rightLayout->addLayout(offsetRow);
    }

    rightLayout->addSpacing(6);

    // 滚动歌词
    m_lyricsWidget = new QListWidget(this);
    m_lyricsWidget->setObjectName(QStringLiteral("lyricsWidget"));
    m_lyricsWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_lyricsWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_lyricsWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_lyricsWidget->setFocusPolicy(Qt::NoFocus);
    m_lyricsWidget->setWordWrap(true);
    m_lyricsWidget->viewport()->installEventFilter(this);
    m_lyricsWidget->viewport()->setMouseTracking(true);   // 持续追踪鼠标移动
    rightLayout->addWidget(m_lyricsWidget, 1);

    // 歌词跳转按钮：叠加在歌词区域中央偏右位置
    m_lyricsPlayBtn = new QPushButton(QIcon(iconsDir() + QStringLiteral("play.ico")), QString(), m_lyricsWidget->viewport());
    m_lyricsPlayBtn->setObjectName(QStringLiteral("lyricsPlayBtn"));
    m_lyricsPlayBtn->setFixedSize(36, 36);
    m_lyricsPlayBtn->setCursor(Qt::PointingHandCursor);
    m_lyricsPlayBtn->setToolTip(QStringLiteral("跳转到此句"));
    m_lyricsPlayBtn->hide();

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({0, 1});         // 初始左面板宽度为 0

    // ════════════════════════════════════════════════════════
    //  底部控制栏（胶囊风格，参考 Fleet-Snowfluff）
    // ════════════════════════════════════════════════════════


    QWidget *controlBar = new QWidget(this);
    controlBar->setObjectName(QStringLiteral("controlBar"));
    QVBoxLayout *ctrlLayout = new QVBoxLayout(controlBar);
    ctrlLayout->setContentsMargins(16, 8, 16, 10);
    ctrlLayout->setSpacing(6);

    // ── 进度条 ──
    QHBoxLayout *progressRow = new QHBoxLayout;
    progressRow->setSpacing(10);

    m_timeCurrent = new QLabel(QStringLiteral("00:00"), this);
    m_timeCurrent->setObjectName(QStringLiteral("timeLabel"));

    m_progressSlider = new QSlider(Qt::Horizontal, this);
    m_progressSlider->setObjectName(QStringLiteral("progressSlider"));
    m_progressSlider->setRange(0, 0);

    m_timeTotal = new QLabel(QStringLiteral("00:00"), this);
    m_timeTotal->setObjectName(QStringLiteral("timeLabel"));

    progressRow->addWidget(m_timeCurrent);
    progressRow->addWidget(m_progressSlider, 1);
    progressRow->addWidget(m_timeTotal);
    ctrlLayout->addLayout(progressRow);

    // ── 控制按钮 ──
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(8);

    // 播放列表
    m_btnPlaylist = new QPushButton(QStringLiteral("☰"), this);
    m_btnPlaylist->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnPlaylist->setToolTip(QStringLiteral("播放列表"));
    m_btnPlaylist->setCheckable(true);
    m_btnPlaylist->setFixedSize(36, 36);
    btnRow->addWidget(m_btnPlaylist);

    // 可视化
    m_btnVisualizer = new QPushButton(QStringLiteral("〰"), this);
    m_btnVisualizer->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnVisualizer->setToolTip(QStringLiteral("音频可视化"));
    m_btnVisualizer->setCheckable(true);
    m_btnVisualizer->setFixedSize(36, 36);
    btnRow->addWidget(m_btnVisualizer);

    btnRow->addSpacing(8);

    // ── 旋转唱片 + 滚动歌名 ──
    m_albumDisc = new QLabel(this);
    m_albumDisc->setObjectName(QStringLiteral("albumDisc"));
    m_albumDisc->setFixedSize(36, 36);
    m_albumDisc->setAlignment(Qt::AlignCenter);
    m_albumDisc->setText(QStringLiteral("🎵"));
    btnRow->addWidget(m_albumDisc);

    m_songMarquee = new QLabel(QStringLiteral("—"), this);
    m_songMarquee->setObjectName(QStringLiteral("songMarquee"));
    m_songMarquee->setMinimumWidth(60);
    btnRow->addWidget(m_songMarquee, 1);

    // 上一曲
    m_btnPrev = new QPushButton(QIcon(iconsDir() + QStringLiteral("prev.ico")), QString(), this);
    m_btnPrev->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnPrev->setToolTip(QStringLiteral("上一首"));
    m_btnPrev->setFixedSize(32, 32);
    btnRow->addWidget(m_btnPrev);

    // 播放/暂停
    m_btnPlayPause = new QPushButton(QIcon(iconsDir() + QStringLiteral("play.ico")), QString(), this);
    m_btnPlayPause->setObjectName(QStringLiteral("playBtn"));
    m_btnPlayPause->setToolTip(QStringLiteral("播放/暂停"));
    m_btnPlayPause->setFixedSize(40, 40);
    btnRow->addWidget(m_btnPlayPause);

    // 下一曲
    m_btnNext = new QPushButton(QIcon(iconsDir() + QStringLiteral("next.ico")), QString(), this);
    m_btnNext->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnNext->setToolTip(QStringLiteral("下一首"));
    m_btnNext->setFixedSize(32, 32);
    btnRow->addWidget(m_btnNext);

    btnRow->addSpacing(8);

    // 播放顺序
    m_btnOrder = new QPushButton(QStringLiteral("□"), this);
    m_btnOrder->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnOrder->setToolTip(QStringLiteral("列表循环"));
    m_btnOrder->setFixedSize(32, 32);
    btnRow->addWidget(m_btnOrder);

    // 音量
    m_btnMute = new QPushButton(QStringLiteral("🔊"), this);
    m_btnMute->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnMute->setToolTip(QStringLiteral("静音"));
    m_btnMute->setCheckable(true);
    m_btnMute->setFixedSize(32, 32);
    btnRow->addWidget(m_btnMute);

    // 滑块+数值标签
    QWidget *volContainer = new QWidget(this);
    QHBoxLayout *volLayout = new QHBoxLayout(volContainer);
    volLayout->setContentsMargins(0, 16, 0, 0);
    volLayout->setSpacing(8);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName(QStringLiteral("volumeSlider"));
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->setFixedHeight(32);
    m_volumeSlider->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    volLayout->addWidget(m_volumeSlider, 0, Qt::AlignVCenter);

    m_volumeLabel = new QLabel(QStringLiteral("70"), this);
    m_volumeLabel->setObjectName(QStringLiteral("volumeLabel"));
    m_volumeLabel->setFixedWidth(24);
    volLayout->addWidget(m_volumeLabel);

    btnRow->addWidget(volContainer);

    ctrlLayout->addLayout(btnRow);
    mainLayout->addWidget(controlBar);

    // ── 音频可视化 ──
    m_visualizer = new AudioVisualizer(m_player->mediaPlayer(), centralWidget);
    m_visualizer->setMinimumHeight(200);
    m_visualizer->hide();
    mainLayout->insertWidget(1, m_visualizer);
}

// ════════════════════════════════════════════════════════════
//  信号连接
// ════════════════════════════════════════════════════════════

void MainWindow::connectSignals()
{
    // PlayTrack → 加载歌曲列表
    connect(m_playTrack, &PlayTrack::songsLoaded,
            this, &MainWindow::onFolderLoaded);
    connect(m_playTrack, &PlayTrack::loadFailed,
            this, [this](const QString &msg) {
                statusBar()->showMessage(msg, 5000);
            });

    // 列表点击 → 播放（兼容分类模式）
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &MainWindow::onSongListItemClicked);

    // 拖拽排序后同步 Playlist（仅在全部歌曲/用户歌单模式启用拖拽）
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved,
            this, &MainWindow::syncPlaylistFromUI);

    // Playlist 当前改变 → 同步高亮
    connect(m_playlist, &Playlist::currentIndexChanged,
            this, &MainWindow::onPlaylistCurrentChanged);

    // 搜索过滤
    connect(m_searchBox, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);

    // ── 右键菜单（格式转换 + 添加到歌单） ──
    m_listWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_listWidget, &QWidget::customContextMenuRequested,
            this, &MainWindow::onShowConversionMenu);

    // ── 歌单选择器 ──
    connect(m_playlistList, &QListWidget::currentRowChanged,
            this, &MainWindow::onPlaylistSelectorSelected);

    // 新建 / 删除歌单
    connect(m_btnNewPlaylist, &QPushButton::clicked,
            this, &MainWindow::onNewPlaylist);
    connect(m_btnDeletePlaylist, &QPushButton::clicked,
            this, &MainWindow::onDeletePlaylist);

    // PlaylistManager 变化 → 刷新歌单选择器
    connect(m_playlistManager, &PlaylistManager::playlistsChanged,
            this, &MainWindow::rebuildPlaylistSelector);

    // 控制按钮
    connect(m_btnPlaylist,   &QPushButton::clicked, this, &MainWindow::onTogglePlaylist);
    connect(m_btnVisualizer, &QPushButton::clicked, this, [this]() {
        bool visible = !m_visualizer->isVisible();
        m_visualizer->setVisible(visible);
        m_btnVisualizer->setChecked(visible);
        statusBar()->showMessage(
            visible ? QStringLiteral("音频可视化已开启") : QStringLiteral("音频可视化已关闭"), 2000);
    });
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnPrev,      &QPushButton::clicked, this, &MainWindow::onPrevious);
    connect(m_btnNext,      &QPushButton::clicked, this, &MainWindow::onNext);
    connect(m_btnOrder,     &QPushButton::clicked, this, &MainWindow::onToggleOrder);
    connect(m_btnMute,      &QPushButton::clicked, this, &MainWindow::onToggleMute);

    // 进度条拖拽
    connect(m_progressSlider, &QSlider::sliderPressed, this, [this]() {
        m_userDragging = true;
    });
    connect(m_progressSlider, &QSlider::sliderReleased, this, [this]() {
        m_userDragging = false;
        m_player->seek(m_progressSlider->value());
    });
    connect(m_progressSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_userDragging) {
            const int curSecs = value / 1000;
            const int totalSecs = m_progressSlider->maximum() / 1000;
            m_timeCurrent->setText(QStringLiteral("%1:%2 / %3:%4")
                .arg(curSecs / 60, 2, 10, QLatin1Char('0'))
                .arg(curSecs % 60, 2, 10, QLatin1Char('0'))
                .arg(totalSecs / 60, 2, 10, QLatin1Char('0'))
                .arg(totalSecs % 60, 2, 10, QLatin1Char('0')));
        }
    });

    // ── 歌词滚动检测（valueChanged 在滚动完成后触发，位置更准确）
    connect(m_lyricsWidget->verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() {
        if (m_lyricsSuppressScroll)
            return;
        // 优先使用最近 wheel 位置，否则取视口中央
        QPoint pos = m_lyricsWheelPos;
        if (pos.isNull())
            pos = QPoint(m_lyricsWidget->viewport()->width() / 2,
                         m_lyricsWidget->viewport()->height() / 2);
        onLyricsScrolled(pos);
    });

    // 音量
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);

    // Player 信号
    connect(m_player, &Player::stateChanged, this, &MainWindow::onPlayerStateChanged);
    connect(m_player, &Player::positionChanged, this, &MainWindow::updateProgressUI);
    connect(m_player, &Player::durationChanged, this, &MainWindow::updateDurationUI);
    connect(m_player, &Player::errorOccurred, this, [this](const QString &msg) {
        statusBar()->showMessage(QStringLiteral("错误: ") + msg, 5000);
    });

    // ── 元数据信号 ──
    // QMediaPlayer 异步回填：每次切歌时断开旧连接重建新连接并捕获曲目路径，
    // 防止队列中延迟到来的旧信号污染新曲目
    // （永久连接在 playCurrentSong 中被动态替换）

    connect(m_player, &Player::coverArtChanged, this, [this](const QPixmap &cover) {
        if (!cover.isNull()) {
            m_coverCache = cover;
            updateCoverDisplay();
            // 同步更新控制栏小唱片
            QPixmap disc = m_coverCache.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_albumDisc->setPixmap(disc);
        }
    });
}

// ════════════════════════════════════════════════════════════
//  播放控制槽
// ════════════════════════════════════════════════════════════

void MainWindow::onPlayPause()
{
    if (!m_playlist->hasCurrent()) {
        if (m_playlist->count() > 0) {
            m_playlist->setCurrentIndex(0);
        } else {
            m_playTrack->browseAndLoad();
            return;
        }
    }
    m_player->togglePlayPause();
    if (m_player->playbackState() == Player::PlaybackState::Paused
        && m_lyricsWidget->count() > 0) {
        // 暂停时在列表底部加一条提示
    }
}

void MainWindow::onNext()
{
    // 切歌前检查文件夹变化
    if (m_playTrack->refreshIfChanged()) {
        statusBar()->showMessage(QStringLiteral("文件夹内容已更新"), 2000);
    }

    if (!m_playlist->hasNext()) {
        statusBar()->showMessage(QStringLiteral("已是最后一首"), 2000);
        return;
    }
    m_manualTrackChange = true;
    m_playlist->next();
    playCurrentSong();
}

void MainWindow::onPrevious()
{
    // 切歌前检查文件夹变化
    if (m_playTrack->refreshIfChanged()) {
        statusBar()->showMessage(QStringLiteral("文件夹内容已更新"), 2000);
    }

    if (!m_playlist->hasPrevious()) {
        statusBar()->showMessage(QStringLiteral("已是第一首"), 2000);
        return;
    }
    m_manualTrackChange = true;
    m_playlist->previous();
    playCurrentSong();
}

void MainWindow::playCurrentSong()
{
    const Song &song = m_playlist->currentSong();
    if (!song.isValid()) return;

    // 记录当前曲目路径，用于校验异步信号是否匹配
    m_currentPlayingPath = song.filePath;

    m_songTitle->setText(song.displayTitle());
    updateArtistAlbumDisplay();

    // 加载封面（优先本地文件，播放后会被 Player 内嵌封面替换）
    if (song.hasCoverFile())
        m_coverCache = song.loadCoverFile();
    else
        m_coverCache = QPixmap();
    updateCoverDisplay();

    // 更新控制栏唱片和歌名
    {
        if (!m_coverCache.isNull()) {
            QPixmap disc = m_coverCache.scaled(36, 36, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            m_albumDisc->setPixmap(disc);
        } else {
            m_albumDisc->setText(QStringLiteral("🎵"));
        }
        QString display = song.displayTitle();
        QString artist = song.displayArtist();
        QString full = display + QStringLiteral(" — ") + artist + QStringLiteral(" ");
        m_songMarquee->setText(full);
        m_marqueeOffset = 0;
        if (full.length() > 15)
            m_marqueeTimer->start();
        else
            m_marqueeTimer->stop();
    }

    // 重置歌词跟随状态
    m_lyricsFollowing = true;
    m_lyricsPlayBtn->hide();
    m_lyricsRevertTimer->stop();

    // 恢复该歌曲的歌词时间微调
    const qint64 savedOffset = m_library->loadLyricsOffset(song.filePath);
    m_subtitle->setUserOffset(savedOffset);
    updateLyricsOffsetLabel();

    // 通知可视化组件新的音频源
    m_visualizer->setSourceFile(song.filePath);

    m_subtitle->clear();
    m_lyricsWidget->clear();

    if (song.hasLyrics() && m_subtitle->load(song.lyricsPath)) {
        for (int i = 0; i < m_subtitle->lineCount(); ++i) {
            const auto line = m_subtitle->lineAt(i);
            auto *item = new QListWidgetItem(line.text);
            item->setTextAlignment(Qt::AlignCenter);
            m_lyricsWidget->addItem(item);
        }
        if (m_lyricsWidget->count() > 0)
            m_lyricsWidget->item(0)->setForeground(QColor("#e94560"));
    } else {
        auto *item = new QListWidgetItem(QStringLiteral("♪ 无歌词"));
        item->setTextAlignment(Qt::AlignCenter);
        m_lyricsWidget->addItem(item);
    }

    m_player->setSource(song.url());
    m_player->play();

    // 建立基于会话 ID 的元数据回填连接
    connectMetadataSession(song);
}

void MainWindow::connectMetadataSession(const Song &song)
{
    // 切歌递增会话 ID，使旧连接捕获的 ID 失效
    const int sessionId = ++m_metadataSessionId;
    const QString playingPath = song.filePath;

    // ── 媒体加载后直接读取 QMediaPlayer 元数据（比 metaDataChanged 信号更可靠） ──
    // 单次连接，断开后将不再触发
    auto *metaConn = new QMetaObject::Connection;
    *metaConn = connect(m_player, &Player::mediaLoaded, this,
        [this, metaConn, sessionId, playingPath]() {
            disconnect(*metaConn);
            delete metaConn;

            if (!m_playlist->hasCurrent() || sessionId != m_metadataSessionId)
                return;
            if (m_playlist->currentSong().filePath != playingPath)
                return;

            Song &s = m_playlist->currentSongRef();
            bool changed = false;

            const QString plArtist = m_player->currentArtist();
            if (!plArtist.isEmpty() &&
                (s.artist.isEmpty() || s.artist == QStringLiteral("未知艺术家"))) {
                s.artist = plArtist;
                changed = true;
            }

            const QString plAlbum = m_player->currentAlbum();
            if (!plAlbum.isEmpty() &&
                (s.album.isEmpty() || s.album == QStringLiteral("未知专辑"))) {
                s.album = plAlbum;
                changed = true;
            }

            if (changed)
                updateArtistAlbumDisplay();
        });

    // ── artistChanged / albumChanged 动态单次连接 ──
    // 每次 playCurrentSong 都会断开旧的并创建新的，确保捕获的 path 始终正确
    auto *artistConn = new QMetaObject::Connection;
    *artistConn = connect(m_player, &Player::artistChanged, this,
        [this, artistConn, sessionId, playingPath](const QString &artist) {
            disconnect(*artistConn);
            delete artistConn;

            if (!m_playlist->hasCurrent() || artist.isEmpty())
                return;
            if (sessionId != m_metadataSessionId)
                return;
            if (m_playlist->currentSong().filePath != playingPath)
                return;

            Song &song = m_playlist->currentSongRef();
            if (song.artist.isEmpty() || song.artist == QStringLiteral("未知艺术家")) {
                song.artist = artist;
                updateArtistAlbumDisplay();
            }
        });

    auto *albumConn = new QMetaObject::Connection;
    *albumConn = connect(m_player, &Player::albumChanged, this,
        [this, albumConn, sessionId, playingPath](const QString &album) {
            disconnect(*albumConn);
            delete albumConn;

            if (!m_playlist->hasCurrent() || album.isEmpty())
                return;
            if (sessionId != m_metadataSessionId)
                return;
            if (m_playlist->currentSong().filePath != playingPath)
                return;

            Song &song = m_playlist->currentSongRef();
            if (song.album.isEmpty() || song.album == QStringLiteral("未知专辑")) {
                song.album = album;
                updateArtistAlbumDisplay();
            }
        });
}

void MainWindow::onSongSelected(int index)
{
    // 点击播放前检查文件夹变化
    if (m_playTrack->refreshIfChanged()) {
        statusBar()->showMessage(QStringLiteral("文件夹内容已更新"), 2000);
    }

    // 从 QListWidget item 获取文件路径，再查找 Playlist 中的实际索引
    if (index < 0 || index >= m_listWidget->count())
        return;

    auto *item = m_listWidget->item(index);
    if (!item)
        return;

    const QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty())
        return;

    // 在 Playlist 中查找实际索引
    const QList<Song> &allSongs = m_playlist->allSongs();
    int actualIndex = -1;
    for (int i = 0; i < allSongs.size(); ++i) {
        if (allSongs[i].filePath == filePath) {
            actualIndex = i;
            break;
        }
    }

    if (actualIndex < 0)
        return;

    m_manualTrackChange = true;
    m_playlist->setCurrentIndex(actualIndex);
    playCurrentSong();
}

// ════════════════════════════════════════════════════════════
//  Player 状态更新
// ════════════════════════════════════════════════════════════

void MainWindow::onPlayerStateChanged(Player::PlaybackState state)
{
    switch (state) {
    case Player::PlaybackState::Playing:
        m_btnPlayPause->setIcon(QIcon(iconsDir() + QStringLiteral("pause.ico")));
        m_btnPlayPause->setToolTip(QStringLiteral("暂停"));
        break;
    case Player::PlaybackState::Paused:
        m_btnPlayPause->setIcon(QIcon(iconsDir() + QStringLiteral("play.ico")));
        m_btnPlayPause->setToolTip(QStringLiteral("播放"));
        break;
    case Player::PlaybackState::Stopped:
        m_btnPlayPause->setIcon(QIcon(iconsDir() + QStringLiteral("play.ico")));
        m_btnPlayPause->setToolTip(QStringLiteral("播放"));
        // 手动切歌（下一首/上一首/双击列表）时，Stopped 由 setSource 触发，
        // 此时不应自动跳到下一首，否则会跳两首
        if (m_manualTrackChange) {
            m_manualTrackChange = false;
            break;
        }
        // 自然结束 → 自动下一首
        if (m_playlist->hasNext()) {
            m_playlist->next();
            playCurrentSong();
        }
        break;
    }
}

void MainWindow::updateProgressUI(qint64 positionMs)
{
    if (m_userDragging) return;

    m_progressSlider->setValue(static_cast<int>(positionMs));
    const int secs = static_cast<int>(positionMs / 1000);
    m_timeCurrent->setText(QStringLiteral("%1:%2")
        .arg(secs / 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0')));

    updateLyrics(positionMs);
}

void MainWindow::updateDurationUI(qint64 durationMs)
{
    m_progressSlider->setRange(0, static_cast<int>(durationMs));
    const int secs = static_cast<int>(durationMs / 1000);
    m_timeTotal->setText(QStringLiteral("%1:%2")
        .arg(secs / 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0')));
}

void MainWindow::onVolumeChanged(int percent)
{
    if (percent == 0) {
        m_btnMute->setText(QStringLiteral("🔇"));
        m_btnMute->setChecked(true);
    } else if (percent < 40) {
        m_btnMute->setText(QStringLiteral("🔉"));
        m_btnMute->setChecked(false);
    } else {
        m_btnMute->setText(QStringLiteral("🔊"));
        m_btnMute->setChecked(false);
    }
    m_player->setVolume(percent);
    m_volumeLabel->setText(QString::number(percent));
    m_player->isMuted() ? m_player->setMuted(false) : void();
}

void MainWindow::onToggleMute()
{
    const bool muted = !m_player->isMuted();
    m_player->setMuted(muted);
    m_btnMute->setChecked(muted);
    m_btnMute->setText(muted ? QStringLiteral("🔇") : QStringLiteral("🔊"));
}

void MainWindow::onToggleOrder()
{
    // 0=列表循环  1=单曲循环  2=随机播放
    m_playOrder = (m_playOrder + 1) % 3;

    switch (m_playOrder) {
    case 0:
        m_btnOrder->setText(QStringLiteral("□"));
        m_btnOrder->setToolTip(QStringLiteral("列表循环"));
        m_playlist->setShuffle(false);
        m_playlist->setRepeatMode(Playlist::RepeatMode::All);
        break;
    case 1:
        m_btnOrder->setText(QStringLiteral("↺"));
        m_btnOrder->setToolTip(QStringLiteral("单曲循环"));
        m_playlist->setShuffle(false);
        m_playlist->setRepeatMode(Playlist::RepeatMode::One);
        break;
    case 2:
        m_btnOrder->setText(QStringLiteral("↯"));
        m_btnOrder->setToolTip(QStringLiteral("随机播放"));
        m_playlist->setShuffle(true);
        m_playlist->setRepeatMode(Playlist::RepeatMode::All);
        break;
    }

    const QString msgs[] = { QStringLiteral("列表循环"), QStringLiteral("单曲循环"), QStringLiteral("随机播放") };
    statusBar()->showMessage(msgs[m_playOrder], 2000);
}

void MainWindow::onSeek(int value)
{
    m_player->seek(value);
}

// ════════════════════════════════════════════════════════════
//  列表 / 库
// ════════════════════════════════════════════════════════════

void MainWindow::onFolderLoaded(const QList<Song> &songs)
{
    // 切换文件夹：完全重置播放列表、播放状态、缓存视图
    m_playlist->clear();
    m_listWidget->clear();
    m_lyricsWidget->clear();
    m_subtitle->clear();
    m_coverCache = QPixmap();
    m_albumArt->setText(QStringLiteral("🎵"));
    m_songTitle->setText(QStringLiteral("—"));
    m_songArtist->setText(QString());
    m_albumDisc->setText(QStringLiteral("🎵"));
    m_songMarquee->setText(QStringLiteral("—"));
    m_marqueeTimer->stop();
    m_player->stop();

    // 重新设置歌曲列表
    m_playlist->setSongs(songs);
    for (const auto &song : songs) {
        auto *item = new QListWidgetItem(song.displayTitle());
        item->setData(Qt::UserRole, song.filePath);
        m_listWidget->addItem(item);
    }

    m_listCountLabel->setText(QStringLiteral("共 %1 首").arg(songs.size()));
    statusBar()->showMessage(QStringLiteral("已加载 %1 首歌曲").arg(songs.size()), 3000);
    m_library->saveLastFolder(m_playTrack->currentFolder());

    // 重置视图到"所有歌曲"并刷新歌单选择器
    m_listMode = ListMode::AllSongs;
    m_currentFilterName.clear();
    rebuildPlaylistSelector();

    if (songs.size() > 0)
        m_listWidget->setCurrentRow(0);
}

void MainWindow::onPlaylistCurrentChanged(int index)
{
    // 在 m_listWidget 中查找当前播放歌曲的高亮行（通过文件路径匹配）
    if (m_playlist->hasCurrent() && index >= 0 && index < m_playlist->count()) {
        const QString currentPath = m_playlist->songAt(index).filePath;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            auto *item = m_listWidget->item(i);
            if (item && item->data(Qt::UserRole).toString() == currentPath) {
                m_listWidget->blockSignals(true);
                m_listWidget->setCurrentRow(i);
                m_listWidget->blockSignals(false);
                break;
            }
        }
    }

    if (m_playlist->hasCurrent()) {
        const Song &song = m_playlist->currentSong();
        m_songTitle->setText(song.displayTitle());
        updateArtistAlbumDisplay();   // 包含专辑信息，不覆盖
    }
}

void MainWindow::restorePlaylistFromLibrary()
{
    int repeatMode = m_library->loadRepeatMode();
    m_playlist->setRepeatMode(static_cast<Playlist::RepeatMode>(repeatMode));
    bool shuffle = m_library->loadShuffle();

    // 同步播放顺序按钮
    if (shuffle) {
        m_playOrder = 2;
        m_btnOrder->setText(QStringLiteral("↯"));
        m_btnOrder->setToolTip(QStringLiteral("随机播放"));
    } else if (repeatMode == 1) {
        m_playOrder = 1;
        m_btnOrder->setText(QStringLiteral("↺"));
        m_btnOrder->setToolTip(QStringLiteral("单曲循环"));
    } else {
        m_playOrder = 0;
        m_btnOrder->setText(QStringLiteral("□"));
        m_btnOrder->setToolTip(QStringLiteral("列表循环"));
    }
    int savedIndex = -1;
    QList<Song> savedSongs = m_library->loadPlaylist(savedIndex);
    if (!savedSongs.isEmpty()) {
        m_playlist->setSongs(savedSongs);
        m_playlist->setCurrentIndex(savedIndex);
        m_listWidget->clear();
        for (const auto &song : savedSongs) {
            auto *item = new QListWidgetItem(song.displayTitle());
            item->setData(Qt::UserRole, song.filePath);  // ← 存入路径
            m_listWidget->addItem(item);
        }
        m_listCountLabel->setText(QStringLiteral("共 %1 首").arg(savedSongs.size()));
        statusBar()->showMessage(
            QStringLiteral("已恢复上次会话 %1 首歌曲").arg(savedSongs.size()), 3000);

        // ── 恢复上次播放的歌曲及播放位置 ──
        const QString lastPath = m_library->loadLastPlayedSong();
        if (!lastPath.isEmpty() && savedIndex >= 0 && savedIndex < m_listWidget->count()) {
            qint64 restorePos = 0;
            // 检查 savedIndex 的歌曲路径是否匹配
            const QString curPath = m_listWidget->item(savedIndex)->data(Qt::UserRole).toString();
            if (curPath == lastPath) {
                restorePos = m_library->loadLastPosition(lastPath);
            } else {
                // savedIndex 不匹配，在所有 items 中查找
                for (int i = 0; i < m_listWidget->count(); ++i) {
                    if (m_listWidget->item(i)->data(Qt::UserRole).toString() == lastPath) {
                        savedIndex = i;
                        restorePos = m_library->loadLastPosition(lastPath);
                        break;
                    }
                }
            }
            // 触发播放
            m_listWidget->setCurrentRow(savedIndex);
            // 等待媒体加载完成后跳转（比 durationChanged 更可靠）
            if (restorePos > 0) {
                auto *conn = new QMetaObject::Connection;
                *conn = connect(m_player, &Player::mediaLoaded, this,
                    [this, restorePos, conn]() {
                        m_player->seek(restorePos);
                        disconnect(*conn);
                        delete conn;
                    });
            }
        }
    }
}

/// 拖拽排序后，将 QListWidget 中的显示顺序同步回 Playlist
void MainWindow::syncPlaylistFromUI()
{
    // 仅在"所有歌曲"模式下允许拖拽排序
    if (m_listMode != ListMode::AllSongs)
        return;

    // 从 QListWidget item 中读取歌曲路径
    QStringList orderedPaths;
    for (int i = 0; i < m_listWidget->count(); ++i) {
        const QString path = m_listWidget->item(i)->data(Qt::UserRole).toString();
        if (!path.isEmpty())
            orderedPaths.append(path);
    }

    // 按新顺序重建 Song 列表（用路径快速查找保留原 metadata）
    QMap<QString, Song> pathMap;
    for (const auto &s : m_playlist->allSongs())
        pathMap.insert(s.filePath, s);

    QList<Song> reordered;
    for (const auto &path : orderedPaths) {
        if (pathMap.contains(path))
            reordered.append(pathMap.value(path));
    }

    // 找出当前播放歌曲在新列表中的索引
    const QString currentPath = m_playlist->hasCurrent()
        ? m_playlist->currentSong().filePath : QString();

    int newIndex = 0;
    for (int i = 0; i < reordered.size(); ++i) {
        if (reordered[i].filePath == currentPath) {
            newIndex = i;
            break;
        }
    }

    // block 信号，一次性更新列表和索引
    m_playlist->blockSignals(true);
    m_playlist->setSongs(reordered);
    m_playlist->setCurrentIndex(newIndex);
    m_playlist->blockSignals(false);

    emit m_playlist->songsChanged();
    emit m_playlist->currentIndexChanged(newIndex);

    statusBar()->showMessage(QStringLiteral("播放顺序已更新"), 2000);
}

// ════════════════════════════════════════════════════════════
//  搜索过滤
// ════════════════════════════════════════════════════════════

void MainWindow::onSearchTextChanged(const QString &text)
{
    const QList<int> matchIndices = m_playlist->searchIndices(text);

    // 遍历所有 item，匹配的显示，不匹配的隐藏
    const int total = m_listWidget->count();
    int visibleCount = 0;
    for (int i = 0; i < total; ++i) {
        const bool visible = text.isEmpty() || matchIndices.contains(i);
        m_listWidget->item(i)->setHidden(!visible);
        if (visible) visibleCount++;
    }

    // 搜索激活时禁用拖拽排序，避免混乱
    m_listWidget->setDragDropMode(text.isEmpty()
        ? QAbstractItemView::InternalMove
        : QAbstractItemView::NoDragDrop);

    // 若当前播放歌曲被隐藏（搜索不匹配），清除高亮
    if (m_playlist->hasCurrent()) {
        const int curIdx = m_playlist->currentIndex();
        if (!text.isEmpty() && !matchIndices.contains(curIdx)) {
            if (m_listWidget->currentRow() >= 0)
                m_listWidget->setCurrentRow(-1);
        }
    }

    m_listCountLabel->setText(text.isEmpty()
        ? QStringLiteral("共 %1 首").arg(total)
        : QStringLiteral("找到 %1 / %2 首").arg(visibleCount).arg(total));
}

// ════════════════════════════════════════════════════════════
//  歌词
// ════════════════════════════════════════════════════════════

void MainWindow::updateLyrics(qint64 positionMs)
{
    if (!m_subtitle->isLoaded() || m_lyricsWidget->count() == 0)
        return;

    const int idx = m_subtitle->lineIndexAt(positionMs);
    if (idx < 0 || idx >= m_lyricsWidget->count())
        return;

    // 如果当前行已在歌词运行中高亮且已滚动到相应位置，忽略
    // 用当前高亮行的下标判断是否需要更新
    static int s_lastHighlighted = -1;

    if (idx == s_lastHighlighted)
        return;

    // 还原上一行颜色
    if (s_lastHighlighted >= 0 && s_lastHighlighted < m_lyricsWidget->count()) {
        auto *prevItem = m_lyricsWidget->item(s_lastHighlighted);
        prevItem->setForeground(QColor("#888"));
        QFont f = prevItem->font();
        f.setBold(false);
        f.setPointSize(12);
        prevItem->setFont(f);
    }

    // 高亮当前行
    auto *currItem = m_lyricsWidget->item(idx);
    currItem->setForeground(QColor("#e94560"));
    QFont f = currItem->font();
    f.setBold(true);
    f.setPointSize(15);
    currItem->setFont(f);

    // 只有在跟随模式下才自动滚动
    if (m_lyricsFollowing) {
        m_lyricsSuppressScroll = true;
        m_lyricsWidget->scrollToItem(currItem, QAbstractItemView::PositionAtCenter);
        m_lyricsSuppressScroll = false;
    }

    s_lastHighlighted = idx;
}

// ════════════════════════════════════════════════════════════
//  歌词滚动交互
// ════════════════════════════════════════════════════════════

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_lyricsWidget->viewport()) {
        switch (event->type()) {
        case QEvent::Wheel: {
            auto *we = static_cast<QWheelEvent *>(event);
            m_lyricsWheelPos = we->position().toPoint();
            break;
        }
        case QEvent::MouseMove: {
            // 非跟随模式下，持续检测鼠标悬浮行并移动按钮
            if (!m_lyricsFollowing && m_lyricsPlayBtn->isVisible()) {
                auto *me = static_cast<QMouseEvent *>(event);
                moveLyricsButtonTo(me->position().toPoint());
                m_lyricsRevertTimer->start();  // 每次移动都重置回弹倒计时
            }
            break;
        }
        default:
            break;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::moveLyricsButtonTo(const QPoint &viewportPos)
{
    if (!m_subtitle->isLoaded() || m_lyricsWidget->count() == 0)
        return;

    // 找到鼠标悬浮处的歌词行
    QListWidgetItem *hoverItem = m_lyricsWidget->itemAt(viewportPos);
    if (!hoverItem) {
        const int firstVisible = m_lyricsWidget->indexAt(QPoint(0, 0)).row();
        if (firstVisible >= 0 && firstVisible < m_lyricsWidget->count())
            hoverItem = m_lyricsWidget->item(firstVisible);
    }
    if (!hoverItem)
        return;

    // 定位按钮到该行右侧
    const QRect vpRect = m_lyricsWidget->viewport()->rect();
    const QRect itemRect = m_lyricsWidget->visualItemRect(hoverItem);
    const int btnX = vpRect.right() - m_lyricsPlayBtn->width() - 8;
    const int btnY = itemRect.center().y() - m_lyricsPlayBtn->height() / 2;
    m_lyricsPlayBtn->move(btnX, btnY);
    m_lyricsPlayBtn->show();
    m_lyricsPlayBtn->raise();
}

void MainWindow::onLyricsScrolled(const QPoint &viewportPos)
{
    if (!m_subtitle->isLoaded() || m_lyricsWidget->count() == 0)
        return;

    m_lyricsFollowing = false;

    // 确定鼠标悬浮位置：优先使用传入的 viewportPos，否则从全局鼠标位置获取
    QPoint pos = viewportPos;
    if (pos.isNull())
        pos = m_lyricsWidget->viewport()->mapFromGlobal(QCursor::pos());

    moveLyricsButtonTo(pos);

    // 重置回弹定时器
    m_lyricsRevertTimer->start();
}

void MainWindow::onLyricsPlayClicked()
{
    if (!m_subtitle->isLoaded() || m_lyricsWidget->count() == 0)
        return;

    // 取按钮当前所在行（按钮中心点对应的歌词行）
    const QPoint btnCenter(m_lyricsPlayBtn->x() + m_lyricsPlayBtn->width() / 2,
                           m_lyricsPlayBtn->y() + m_lyricsPlayBtn->height() / 2);
    QListWidgetItem *targetItem = m_lyricsWidget->itemAt(btnCenter);
    if (!targetItem) {
        // 按钮通常位于某行右侧，若 itemAt 没命中，用按钮左边缘再试
        const QPoint btnLeft(m_lyricsPlayBtn->x() - 4, btnCenter.y());
        targetItem = m_lyricsWidget->itemAt(btnLeft);
    }
    if (!targetItem)
        return;

    // 用按钮上方的 item（按钮可能位于两行之间时取上面的）
    const int row = m_lyricsWidget->row(targetItem);
    const qint64 targetMs = m_subtitle->lineAt(row).startMs;

    // 跳转播放
    m_player->seek(targetMs);
    if (m_player->playbackState() != Player::PlaybackState::Playing)
        m_player->play();

    // 恢复跟随状态
    m_lyricsFollowing = true;
    m_lyricsPlayBtn->hide();
    m_lyricsRevertTimer->stop();

    // 立即刷新高亮
    updateLyrics(m_player->position());
}

void MainWindow::updateLyricsOffsetLabel()
{
    const qint64 offsetMs = m_subtitle->userOffset();
    const double sec = offsetMs / 1000.0;
    m_lyricsOffsetLabel->setText((sec >= 0 ? QStringLiteral("+") : QString())
                                 + QString::number(sec, 'f', 1) + QStringLiteral("s"));
}

void MainWindow::onLyricsOffsetAdjust(qint64 deltaMs)
{
    if (!m_subtitle->isLoaded())
        return;

    m_subtitle->adjustUserOffset(deltaMs);
    updateLyricsOffsetLabel();

    // 立即根据新偏移刷新当前高亮
    if (m_player->playbackState() != Player::PlaybackState::Stopped)
        updateLyrics(m_player->position());

    // 实时保存偏移
    if (m_playlist->hasCurrent())
        m_library->saveLyricsOffset(m_playlist->currentSong().filePath, m_subtitle->userOffset());

    statusBar()->showMessage(
        QStringLiteral("歌词偏移: %1").arg(m_lyricsOffsetLabel->text()), 2000);
}

void MainWindow::onLyricsRevertTimeout()
{
    m_lyricsPlayBtn->hide();
    m_lyricsFollowing = true;

    // 滚回当前高亮行
    if (m_subtitle->isLoaded() && m_lyricsWidget->count() > 0) {
        // 用 s_lastHighlighted 的方式需要获取，但它是 static 局部变量
        // 改为从 m_subtitle 获取当前播放位置对应行
        if (m_player->playbackState() != Player::PlaybackState::Stopped) {
            const int curIdx = m_subtitle->lineIndexAt(m_player->position());
            if (curIdx >= 0 && curIdx < m_lyricsWidget->count()) {
                m_lyricsSuppressScroll = true;
                m_lyricsWidget->scrollToItem(m_lyricsWidget->item(curIdx),
                                             QAbstractItemView::PositionAtCenter);
                m_lyricsSuppressScroll = false;
            }
        }
    }
}

// ════════════════════════════════════════════════════════════
//  元数据显示
// ════════════════════════════════════════════════════════════

void MainWindow::updateArtistAlbumDisplay()
{
    if (!m_playlist->hasCurrent())
        return;

    const Song &song = m_playlist->currentSong();
    const QString artist = song.displayArtist();
    const QString album  = song.album;

    if (!album.isEmpty())
        m_songArtist->setText(artist + QStringLiteral("  ·  ") + album);
    else
        m_songArtist->setText(artist);
}

// ════════════════════════════════════════════════════════════
//  格式转换（右键菜单）
// ════════════════════════════════════════════════════════════

void MainWindow::onShowConversionMenu(const QPoint &pos)
{
    QListWidgetItem *item = m_listWidget->itemAt(pos);
    if (!item)
        return;

    const QString filePath = item->data(Qt::UserRole).toString();
    if (filePath.isEmpty() || !QFileInfo::exists(filePath))
        return;

    const QString title = item->text();

    QMenu menu(this);
    menu.setTitle(QStringLiteral("操作"));

    QAction *actConvert = menu.addAction(QStringLiteral("格式转换"));
    actConvert->setIcon(qApp->style()->standardIcon(QStyle::SP_FileDialogDetailedView));

    menu.addSeparator();

    // 添加到歌单子菜单
    QMenu *addToMenu = menu.addMenu(QStringLiteral("添加到歌单"));
    const QStringList playlistNames = m_playlistManager->playlistNames();
    if (playlistNames.isEmpty()) {
        addToMenu->addAction(QStringLiteral("（暂无歌单）"))->setEnabled(false);
    } else {
        for (const auto &plName : playlistNames) {
            QAction *act = addToMenu->addAction(plName);
            if (m_playlistManager->hasSong(plName, filePath)) {
                act->setEnabled(false);
                act->setText(plName + QStringLiteral(" ✓"));
            }
        }
    }

    QAction *selected = menu.exec(m_listWidget->viewport()->mapToGlobal(pos));
    if (selected == actConvert) {
        onConvertSong(filePath, title);
    } else if (selected && selected->parent() == addToMenu && selected->isEnabled()) {
        QString targetName = selected->text();
        if (targetName.endsWith(QStringLiteral(" ✓")))
            targetName.chop(2);
        if (m_playlistManager->addSong(targetName, filePath)) {
            statusBar()->showMessage(
                QStringLiteral("已添加到歌单「%1」").arg(targetName), 3000);
        }
    }
}

void MainWindow::onConvertSong(const QString &filePath, const QString &title)
{
    // 对话框采用模态方式，内部异步转码不阻塞播放器
    ConversionDialog dlg(filePath, title, this);
    dlg.setModal(true);
    dlg.exec();
}

// ════════════════════════════════════════════════════════════
//  播放列表切换
// ════════════════════════════════════════════════════════════

void MainWindow::onTogglePlaylist()
{
    const bool visible = !m_leftPanel->isVisible();
    m_leftPanel->setVisible(visible);
    m_btnPlaylist->setChecked(visible);   // checked → 红色 (#ff5252) 表示已展开

    if (visible && m_listWidget->count() > 0 && m_playlist->hasCurrent()) {
        // 展开时滚动到当前播放项
        m_listWidget->scrollToItem(m_listWidget->item(m_playlist->currentIndex()),
                                   QAbstractItemView::PositionAtCenter);
    }

    statusBar()->showMessage(
        visible ? QStringLiteral("播放列表已显示") : QStringLiteral("播放列表已隐藏"),
        2000);
}

// ════════════════════════════════════════════════════════════
//  主题切换
// ════════════════════════════════════════════════════════════

void MainWindow::switchTheme(Style::Theme theme)
{
    m_currentTheme = theme;

    // 应用样式
    if (!m_diyBgFolder.isEmpty())
        applyBackgrounds(m_diyBgFolder);
    else
        qApp->setStyleSheet(Style::styleSheet(m_currentTheme));

    // Fleet-Snowfluff 特效联动
    if (m_fleetFx) {
        if (theme == Style::Fleet)
            m_fleetFx->start();
        else
            m_fleetFx->stop();
    }

    // 更新菜单勾选状态 — 先清除所有勾选，再勾选当前主题
    if (m_themeMenu) {
        const auto themeActions = m_themeMenu->actions();
        for (QAction *a : themeActions)
            a->setChecked(false);
        const int idx = static_cast<int>(theme);
        if (idx >= 0 && idx < themeActions.size())
            themeActions[idx]->setChecked(true);
    }

    const QString names[] = { QStringLiteral("深色主题"), QStringLiteral("亮色主题"), QStringLiteral("Fleet-Snowfluff") };
    statusBar()->showMessage(names[int(theme)] + QStringLiteral(" 已应用"), 3000);
}

void MainWindow::onToggleTheme()
{
    int next = (int(m_currentTheme) + 1) % 3;
    switchTheme(Style::Theme(next));
}

// ════════════════════════════════════════════════════════════
//  DIY 背景
// ════════════════════════════════════════════════════════════

void MainWindow::onSelectBgFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this,
        QStringLiteral("选择背景图片文件夹"),
        m_diyBgFolder.isEmpty() ? QDir::homePath() : m_diyBgFolder);

    if (dir.isEmpty())
        return;

    m_diyBgFolder = dir;
    applyBackgrounds(dir);
    statusBar()->showMessage(QStringLiteral("自定义背景已应用: ") + dir, 3000);
}

void MainWindow::clearBackgrounds()
{
    m_diyBgFolder.clear();
    // 重新应用样式表以恢复默认背景
    qApp->setStyleSheet(Style::styleSheet(m_currentTheme));
    statusBar()->showMessage(QStringLiteral("已清除自定义背景"), 2000);
}

void MainWindow::applyBackgrounds(const QString &folderPath)
{
    QDir dir(folderPath);
    if (!dir.exists())
        return;

    // 先重置样式表（保留主题基础），再逐组件添加背景
    QString baseSheet = Style::styleSheet(m_currentTheme);

    // 支持的文件名 → 组件对象名映射
    // 用户放置如 leftPanel.png, controlBar.jpg, rightPanel.png 等
    struct BgMapping {
        QString objectName;
        QString fileName;
    };
    const BgMapping mappings[] = {
        { QStringLiteral("leftPanel"),    QStringLiteral("leftPanel") },
        { QStringLiteral("rightPanel"),   QStringLiteral("rightPanel") },
        { QStringLiteral("controlBar"),   QStringLiteral("controlBar") },
        { QStringLiteral("lyricsWidget"), QStringLiteral("lyricsWidget") },
        { QStringLiteral("albumArt"),     QStringLiteral("albumArt") },
    };

    QStringList bgRules;
    for (const auto &m : mappings) {
        // 尝试常见图片格式
        static const char *exts[] = { "png", "jpg", "jpeg", "bmp", "gif", "webp" };
        QString imagePath;
        for (const char *ext : exts) {
            QString candidate = dir.filePath(m.fileName + QStringLiteral(".") + QString::fromLatin1(ext));
            if (QFileInfo::exists(candidate)) {
                imagePath = candidate;
                break;
            }
        }
        if (imagePath.isEmpty())
            continue;

        // 将路径中的反斜杠转为正斜杠，QSS 要求
        imagePath.replace(QStringLiteral("\\"), QStringLiteral("/"));

        QString rule;
        if (m.objectName == QStringLiteral("albumArt")) {
            // albumArt 是 QLabel，用 border-image 保持背景
            rule = QStringLiteral("#%1 { border-image: url(%2); background: transparent; }")
                       .arg(m.objectName, imagePath);
        } else {
            rule = QStringLiteral("#%1 { background-image: url(%2); background-repeat: no-repeat; background-position: center; }")
                       .arg(m.objectName, imagePath);
        }
        bgRules.append(rule);
    }

    if (bgRules.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("文件夹中未找到匹配的图片文件（如 leftPanel.png, controlBar.jpg 等）"), 3000);
        return;
    }

    // 合并样式：主题基础 + 自定义背景规则
    QString fullSheet = baseSheet + QStringLiteral("\n") + bgRules.join(QStringLiteral("\n"));
    qApp->setStyleSheet(fullSheet);
}

// ════════════════════════════════════════════════════════════
//  歌单管理
// ════════════════════════════════════════════════════════════

void MainWindow::rebuildPlaylistSelector()
{
    m_playlistList->blockSignals(true);
    m_playlistList->clear();

    // 1. "所有歌曲"（固定首项）
    auto *allItem = new QListWidgetItem(QStringLiteral("🎵 所有歌曲"));
    allItem->setData(Qt::UserRole, QStringLiteral("__all__"));
    allItem->setData(Qt::UserRole + 1, QStringLiteral("all"));
    allItem->setFlags(allItem->flags() & ~Qt::ItemIsDropEnabled);
    m_playlistList->addItem(allItem);

    // 2. 用户歌单
    const QStringList names = m_playlistManager->playlistNames();
    if (!names.isEmpty()) {
        // 分隔标题（不可选）
        auto *sep1 = new QListWidgetItem(QStringLiteral("── 我的歌单 ──"));
        sep1->setFlags(sep1->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
        sep1->setData(Qt::UserRole, QStringLiteral("__separator__"));
        m_playlistList->addItem(sep1);

        for (const auto &name : names) {
            auto *plItem = new QListWidgetItem(QStringLiteral("📁 ") + name);
            plItem->setData(Qt::UserRole, name);
            plItem->setData(Qt::UserRole + 1, QStringLiteral("playlist"));
            m_playlistList->addItem(plItem);
        }
    }

    // 3. 分类浏览
    auto *sep2 = new QListWidgetItem(QStringLiteral("── 分类浏览 ──"));
    sep2->setFlags(sep2->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
    sep2->setData(Qt::UserRole, QStringLiteral("__separator__"));
    m_playlistList->addItem(sep2);

    auto *artistItem = new QListWidgetItem(QStringLiteral("👤 按歌手"));
    artistItem->setData(Qt::UserRole, QStringLiteral("__artist__"));
    artistItem->setData(Qt::UserRole + 1, QStringLiteral("category"));
    m_playlistList->addItem(artistItem);

    auto *albumItem = new QListWidgetItem(QStringLiteral("💿 按专辑"));
    albumItem->setData(Qt::UserRole, QStringLiteral("__album__"));
    albumItem->setData(Qt::UserRole + 1, QStringLiteral("category"));
    m_playlistList->addItem(albumItem);

    // 恢复选中项
    restorePlaylistSelectorSelection();

    m_playlistList->blockSignals(false);
}

void MainWindow::restorePlaylistSelectorSelection()
{
    // 根据当前状态恢复选中项
    for (int i = 0; i < m_playlistList->count(); ++i) {
        auto *item = m_playlistList->item(i);
        if (!item || !(item->flags() & Qt::ItemIsSelectable))
            continue;

        const QString type = item->data(Qt::UserRole + 1).toString();
        const QString val  = item->data(Qt::UserRole).toString();

        if (m_listMode == ListMode::AllSongs && type == QStringLiteral("all")) {
            m_playlistList->setCurrentRow(i);
            return;
        }
        if (m_listMode == ListMode::UserPlaylist && type == QStringLiteral("playlist") && val == m_currentFilterName) {
            m_playlistList->setCurrentRow(i);
            return;
        }
        if (m_listMode == ListMode::ByArtist && type == QStringLiteral("category") && val == QStringLiteral("__artist__")) {
            m_playlistList->setCurrentRow(i);
            return;
        }
        if (m_listMode == ListMode::ByAlbum && type == QStringLiteral("category") && val == QStringLiteral("__album__")) {
            m_playlistList->setCurrentRow(i);
            return;
        }
    }
}

void MainWindow::onPlaylistSelectorSelected(int index)
{
    if (index < 0 || index >= m_playlistList->count())
        return;

    auto *item = m_playlistList->item(index);
    if (!item || !(item->flags() & Qt::ItemIsSelectable))
        return;

    const QString type = item->data(Qt::UserRole + 1).toString();
    const QString val  = item->data(Qt::UserRole).toString();

    if (type == QStringLiteral("all")) {
        // 所有歌曲
        m_listMode = ListMode::AllSongs;
        m_currentFilterName.clear();
        rebuildSongList();
    } else if (type == QStringLiteral("playlist")) {
        // 用户歌单
        m_listMode = ListMode::UserPlaylist;
        m_currentFilterName = val;
        rebuildSongList();
    } else if (type == QStringLiteral("category")) {
        if (val == QStringLiteral("__artist__")) {
            // 按歌手：显示歌手列表
            m_listMode = ListMode::ByArtist;
            m_currentFilterName.clear();
            showCategoryList(true);
        } else if (val == QStringLiteral("__album__")) {
            // 按专辑：显示专辑列表
            m_listMode = ListMode::ByAlbum;
            m_currentFilterName.clear();
            showCategoryList(false);
        }
    }
}

void MainWindow::showCategoryList(bool isArtist)
{
    m_listWidget->blockSignals(true);
    m_listWidget->clear();
    m_listWidget->setDragDropMode(QAbstractItemView::NoDragDrop);

    const QList<Song> &allSongs = m_playlist->allSongs();
    QStringList categories;
    if (isArtist)
        categories = m_playlistManager->artistsFromSongs(allSongs);
    else
        categories = m_playlistManager->albumsFromSongs(allSongs);

    // 添加返回项
    auto *backItem = new QListWidgetItem(QStringLiteral("← 返回全部歌曲"));
    backItem->setData(Qt::UserRole, QStringLiteral("__back__"));
    backItem->setData(Qt::UserRole + 1, QStringLiteral("back"));
    m_listWidget->addItem(backItem);

    // 添加分类项
    for (const auto &cat : categories) {
        QString prefix = isArtist ? QStringLiteral("👤 ") : QStringLiteral("💿 ");
        auto *catItem = new QListWidgetItem(prefix + cat);
        catItem->setData(Qt::UserRole, cat);
        catItem->setData(Qt::UserRole + 1, isArtist ? QStringLiteral("artist") : QStringLiteral("album"));
        m_listWidget->addItem(catItem);
    }

    m_listCountLabel->setText(QStringLiteral("共 %1 个分类").arg(categories.size()));
    m_listWidget->blockSignals(false);
}

void MainWindow::onSongListItemClicked(int index)
{
    if (index < 0)
        return;

    auto *item = m_listWidget->item(index);
    if (!item)
        return;

    // 分类模式处理
    if (m_listMode == ListMode::ByArtist || m_listMode == ListMode::ByAlbum) {
        const QString subType = item->data(Qt::UserRole + 1).toString();
        const QString val = item->data(Qt::UserRole).toString();

        if (subType == QStringLiteral("back")) {
            // 返回全部歌曲
            m_listMode = ListMode::AllSongs;
            m_currentFilterName.clear();
            rebuildPlaylistSelector();
            rebuildSongList();
            return;
        }

        if (subType == QStringLiteral("artist")) {
            m_listMode = ListMode::ArtistSongs;
            m_currentFilterName = val;
            rebuildSongList();
            return;
        }

        if (subType == QStringLiteral("album")) {
            m_listMode = ListMode::AlbumSongs;
            m_currentFilterName = val;
            rebuildSongList();
            return;
        }
        return;
    }

    // 艺术家/专辑歌曲列表模式：与普通模式相同，调用 onSongSelected
    if (m_listMode == ListMode::ArtistSongs || m_listMode == ListMode::AlbumSongs) {
        onSongSelected(index);
        return;
    }

    // 普通模式（AllSongs / UserPlaylist）
    onSongSelected(index);
}

void MainWindow::rebuildSongList()
{
    m_listWidget->blockSignals(true);
    m_listWidget->clear();

    // 仅在"所有歌曲"模式下启用拖拽排序
    if (m_listMode == ListMode::AllSongs)
        m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    else
        m_listWidget->setDragDropMode(QAbstractItemView::NoDragDrop);

    const QList<Song> &allSongs = m_playlist->allSongs();

    QList<Song> displaySongs;

    if (m_listMode == ListMode::AllSongs) {
        displaySongs = allSongs;
    } else if (m_listMode == ListMode::UserPlaylist) {
        displaySongs = m_playlistManager->resolveSongs(m_currentFilterName, allSongs);
    } else if (m_listMode == ListMode::ArtistSongs) {
        const QList<int> indices = m_playlistManager->songIndicesByArtist(m_currentFilterName, allSongs);
        for (int idx : indices) {
            if (idx >= 0 && idx < allSongs.size())
                displaySongs.append(allSongs[idx]);
        }
    } else if (m_listMode == ListMode::AlbumSongs) {
        const QList<int> indices = m_playlistManager->songIndicesByAlbum(m_currentFilterName, allSongs);
        for (int idx : indices) {
            if (idx >= 0 && idx < allSongs.size())
                displaySongs.append(allSongs[idx]);
        }
    }

    // 填充列表
    for (const auto &song : displaySongs) {
        auto *item = new QListWidgetItem(song.displayTitle());
        item->setData(Qt::UserRole, song.filePath);
        m_listWidget->addItem(item);
    }

    // 同步 Playlist（仅 AllSongs 模式使用完整列表，其他模式用筛选后的）
    // 保存当前播放歌曲引用
    const QString currentPath = m_playlist->hasCurrent()
        ? m_playlist->currentSong().filePath : QString();

    if (m_listMode == ListMode::AllSongs) {
        // 非 AllSongs 模式下不修改 Playlist，仅修改显示
        // AllSongs 模式下直接使用 m_playlist 已有的内容
        int newIndex = -1;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            if (m_listWidget->item(i)->data(Qt::UserRole).toString() == currentPath) {
                newIndex = i;
                break;
            }
        }
        if (newIndex >= 0) {
            m_listWidget->setCurrentRow(newIndex);
        } else if (m_listWidget->count() > 0 && m_playlist->hasCurrent()) {
            m_listWidget->setCurrentRow(0);
        }
    } else {
        // 筛选模式：尝试恢复当前播放歌曲的高亮
        int newIndex = -1;
        for (int i = 0; i < m_listWidget->count(); ++i) {
            if (m_listWidget->item(i)->data(Qt::UserRole).toString() == currentPath) {
                newIndex = i;
                break;
            }
        }
        if (newIndex >= 0) {
            m_listWidget->setCurrentRow(newIndex);
        }
    }

    // 更新计数
    const int total = m_playlist->count();
    const int shown = m_listWidget->count();
    if (shown == total)
        m_listCountLabel->setText(QStringLiteral("共 %1 首").arg(total));
    else
        m_listCountLabel->setText(QStringLiteral("显示 %1 / %2 首").arg(shown).arg(total));

    m_listWidget->blockSignals(false);
}

void MainWindow::onNewPlaylist()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this,
        QStringLiteral("新建歌单"),
        QStringLiteral("请输入歌单名称："),
        QLineEdit::Normal,
        QString(),
        &ok);

    if (!ok || name.trimmed().isEmpty())
        return;

    if (m_playlistManager->createPlaylist(name.trimmed())) {
        statusBar()->showMessage(QStringLiteral("歌单「%1」已创建").arg(name.trimmed()), 3000);
        // 自动选中新建的歌单
        rebuildPlaylistSelector();
        for (int i = 0; i < m_playlistList->count(); ++i) {
            auto *item = m_playlistList->item(i);
            if (item && item->data(Qt::UserRole).toString() == name.trimmed()) {
                m_playlistList->setCurrentRow(i);
                break;
            }
        }
    } else {
        statusBar()->showMessage(QStringLiteral("歌单「%1」已存在，请使用其他名称").arg(name.trimmed()), 3000);
    }
}

void MainWindow::onDeletePlaylist()
{
    // 获取当前选中的歌单
    int row = m_playlistList->currentRow();
    if (row < 0) {
        statusBar()->showMessage(QStringLiteral("请先在歌单列表中选择要删除的歌单"), 3000);
        return;
    }

    auto *item = m_playlistList->item(row);
    if (!item)
        return;

    const QString type = item->data(Qt::UserRole + 1).toString();
    if (type != QStringLiteral("playlist")) {
        statusBar()->showMessage(QStringLiteral("请选择一个用户歌单进行删除"), 3000);
        return;
    }

    const QString name = item->data(Qt::UserRole).toString();

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        QStringLiteral("删除歌单"),
        QStringLiteral("确定要删除歌单「%1」吗？\n此操作不可撤销。").arg(name),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_playlistManager->deletePlaylist(name);
        statusBar()->showMessage(QStringLiteral("歌单「%1」已删除").arg(name), 3000);
        // 切换到"所有歌曲"视图
        m_listMode = ListMode::AllSongs;
        m_currentFilterName.clear();
        rebuildSongList();
    }
}

void MainWindow::onAddToPlaylistMenu(const QString &songPath)
{
    const QStringList names = m_playlistManager->playlistNames();
    if (names.isEmpty()) {
        QMessageBox::information(this,
            QStringLiteral("添加到歌单"),
            QStringLiteral("暂无歌单，请先创建歌单。"));
        return;
    }

    // 显示选择菜单
    QMenu menu(this);
    menu.setTitle(QStringLiteral("添加到歌单"));

    QAction *selected = nullptr;
    for (const auto &name : names) {
        QAction *act = menu.addAction(name);
        if (m_playlistManager->hasSong(name, songPath)) {
            act->setEnabled(false);
            act->setText(name + QStringLiteral(" ✓"));
        }
        if (!selected)
            selected = act;
    }

    QAction *result = menu.exec(QCursor::pos());
    if (result && result->isEnabled()) {
        const QString targetName = result->text();
        // 去掉可能的 ✓ 标记
        QString cleanName = targetName;
        if (cleanName.endsWith(QStringLiteral(" ✓")))
            cleanName.chop(2);

        if (m_playlistManager->addSong(cleanName, songPath)) {
            statusBar()->showMessage(
                QStringLiteral("已添加到歌单「%1」").arg(cleanName), 3000);
        }
    }
}

void MainWindow::savePlaylistData()
{
    m_library->savePlaylists(m_playlistManager->toJson());
}

void MainWindow::restorePlaylistData()
{
    const QJsonArray data = m_library->loadPlaylists();
    m_playlistManager->fromJson(data);
    rebuildPlaylistSelector();
}