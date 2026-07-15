#include "mainwindow.h"
#include "player.h"
#include "playlist.h"
#include "playtrack.h"
#include "subtitleplayer.h"
#include "library.h"
#include "song.h"

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
    m_subtitle  = new SubtitlePlayer(this);
    m_library   = new Library(this);

    setupUI();
    connectSignals();

    // 恢复上次状态
    restorePlaylistFromLibrary();
    const int savedVol = m_library->loadVolume();
    m_player->setVolume(savedVol);
    m_volumeSlider->setValue(savedVol);
    m_player->setMuted(m_library->loadMuted());
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
    m_library->saveWindowGeometry(saveGeometry());
    if (m_playlist->hasCurrent())
        m_library->saveLastPosition(m_playlist->currentSong().filePath, m_player->position());

    QMainWindow::closeEvent(event);
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
    setupStyleSheet();

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
        PlayTrack::clearCache();
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

    // ──── 左侧面板：播放列表 ────
    QWidget *leftPanel = new QWidget(this);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    m_listWidget = new QListWidget(this);
    m_listWidget->setAlternatingRowColors(true);
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_listWidget->setDefaultDropAction(Qt::MoveAction);
    leftLayout->addWidget(m_listWidget, 1);

    m_listCountLabel = new QLabel(QStringLiteral("共 0 首"), this);
    m_listCountLabel->setObjectName(QStringLiteral("countLabel"));
    leftLayout->addWidget(m_listCountLabel);

    splitter->addWidget(leftPanel);

    // ──── 右侧面板：歌曲信息 + 歌词 ────
    QWidget *rightPanel = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(20, 20, 20, 10);
    rightLayout->setSpacing(8);

    // 专辑封面占位
    m_albumArt = new QLabel(this);
    m_albumArt->setObjectName(QStringLiteral("albumArt"));
    m_albumArt->setFixedSize(180, 180);
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

    rightLayout->addSpacing(10);

    m_lyricsLabel = new QLabel(QStringLiteral("♪ 等待播放..."), this);
    m_lyricsLabel->setObjectName(QStringLiteral("lyricsLabel"));
    m_lyricsLabel->setAlignment(Qt::AlignCenter);
    m_lyricsLabel->setWordWrap(true);
    m_lyricsLabel->setMinimumHeight(60);
    rightLayout->addWidget(m_lyricsLabel, 1);

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    // ════════════════════════════════════════════════════════
    //  底部控制栏
    // ════════════════════════════════════════════════════════
    QWidget *controlBar = new QWidget(this);
    controlBar->setObjectName(QStringLiteral("controlBar"));
    QVBoxLayout *ctrlLayout = new QVBoxLayout(controlBar);
    ctrlLayout->setContentsMargins(12, 6, 12, 10);
    ctrlLayout->setSpacing(4);

    // ── 进度条 ──
    QHBoxLayout *progressRow = new QHBoxLayout;
    progressRow->setSpacing(8);

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
    btnRow->setSpacing(6);

    m_btnShuffle = new QPushButton(QStringLiteral("随机"), this);
    m_btnShuffle->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnShuffle->setToolTip(QStringLiteral("随机播放"));
    m_btnShuffle->setCheckable(true);
    m_btnShuffle->setFixedSize(48, 36);
    btnRow->addWidget(m_btnShuffle);

    btnRow->addStretch();

    m_btnPrev = new QPushButton(QStringLiteral("⏮"), this);
    m_btnPrev->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnPrev->setToolTip(QStringLiteral("上一首"));
    m_btnPrev->setFixedSize(36, 36);
    btnRow->addWidget(m_btnPrev);

    m_btnPlayPause = new QPushButton(QStringLiteral("▶"), this);
    m_btnPlayPause->setObjectName(QStringLiteral("playBtn"));
    m_btnPlayPause->setToolTip(QStringLiteral("播放/暂停"));
    m_btnPlayPause->setFixedSize(44, 44);
    btnRow->addWidget(m_btnPlayPause);

    m_btnNext = new QPushButton(QStringLiteral("⏭"), this);
    m_btnNext->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnNext->setToolTip(QStringLiteral("下一首"));
    m_btnNext->setFixedSize(36, 36);
    btnRow->addWidget(m_btnNext);

    btnRow->addStretch();

    m_btnRepeat = new QPushButton(QStringLiteral("循环"), this);
    m_btnRepeat->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnRepeat->setToolTip(QStringLiteral("循环模式"));
    m_btnRepeat->setFixedSize(48, 36);
    btnRow->addWidget(m_btnRepeat);

    // 音量
    m_btnMute = new QPushButton(QStringLiteral("🔊"), this);
    m_btnMute->setObjectName(QStringLiteral("ctrlBtn"));
    m_btnMute->setToolTip(QStringLiteral("静音"));
    m_btnMute->setCheckable(true);
    m_btnMute->setFixedSize(36, 36);
    btnRow->addWidget(m_btnMute);

    m_volumeSlider = new QSlider(Qt::Horizontal, this);
    m_volumeSlider->setObjectName(QStringLiteral("volumeSlider"));
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(100);
    btnRow->addWidget(m_volumeSlider);

    m_volumeLabel = new QLabel(QStringLiteral("70"), this);
    m_volumeLabel->setObjectName(QStringLiteral("volumeLabel"));
    m_volumeLabel->setFixedWidth(28);
    btnRow->addWidget(m_volumeLabel);

    ctrlLayout->addLayout(btnRow);
    mainLayout->addWidget(controlBar);
}

void MainWindow::setupStyleSheet()
{
    setStyleSheet(QStringLiteral(R"(
        QMainWindow { background-color: #1a1a2e; }
        QWidget { color: #e0e0e0; font-family: "Segoe UI","Microsoft YaHei",sans-serif; }

        QMenuBar {
            background-color: #16213e; color: #ccc;
            border-bottom: 1px solid #0f3460; padding: 2px 0;
        }
        QMenuBar::item:selected { background-color: #0f3460; color: #fff; }
        QMenu { background-color: #16213e; border: 1px solid #0f3460; }
        QMenu::item:selected { background-color: #0f3460; }

        QListWidget {
            background-color: #16213e; border: none;
            border-right: 1px solid #0f3460; font-size: 13px; outline: none;
        }
        QListWidget::item {
            padding: 6px 12px; border-bottom: 1px solid #1a2744;
        }
        QListWidget::item:selected { background-color: #0f3460; color: #fff; }
        QListWidget::item:hover:!selected { background-color: #1a2744; }
        QListWidget::item:alternate { background-color: #192841; }

        #countLabel {
            background-color: #16213e; padding: 4px 12px;
            font-size: 12px; color: #888; border-top: 1px solid #0f3460;
        }

        #songTitle { font-size: 20px; font-weight: bold; color: #fff; }
        #songArtist { font-size: 14px; color: #aaa; }

        #lyricsLabel {
            font-size: 16px; color: #e0e0e0; padding: 10px;
            background-color: rgba(15,52,96,0.3); border-radius: 8px;
        }
        #albumArt { font-size: 64px; background-color: #16213e; border-radius: 12px; }

        #controlBar { background-color: #0f3460; border-top: 1px solid #1a5276; }

        #progressSlider::groove:horizontal { height: 5px; background: #1a2744; border-radius: 2px; }
        #progressSlider::handle:horizontal {
            width: 14px; height: 14px; margin: -5px 0;
            background: #e94560; border-radius: 7px;
        }
        #progressSlider::sub-page:horizontal { background: #e94560; border-radius: 2px; }
        #progressSlider { background: transparent; }

        #volumeSlider::groove:horizontal { height: 4px; background: #1a2744; border-radius: 2px; }
        #volumeSlider::handle:horizontal {
            width: 12px; height: 12px; margin: -4px 0;
            background: #ccc; border-radius: 6px;
        }
        #volumeSlider::sub-page:horizontal { background: #e94560; border-radius: 2px; }
        #volumeSlider { background: transparent; }

        #timeLabel { font-size: 12px; color: #aaa; min-width: 36px; }

        #ctrlBtn {
            background: transparent; border: none;
            font-size: 18px; color: #ccc; border-radius: 18px;
        }
        #ctrlBtn:hover { background: rgba(233,69,96,0.2); color: #fff; }
        #ctrlBtn:checked { color: #e94560; }

        #playBtn {
            background: #e94560; border: none; font-size: 20px;
            color: #fff; border-radius: 22px;
        }
        #playBtn:hover { background: #ff6b81; }

        #volumeLabel { font-size: 12px; color: #aaa; }

        QStatusBar {
            background-color: #16213e; color: #888;
            border-top: 1px solid #0f3460; font-size: 12px;
        }
        QSplitter::handle { background: #0f3460; }

        QScrollBar:vertical {
            width: 8px; background: #16213e;
        }
        QScrollBar::handle:vertical {
            background: #0f3460; border-radius: 4px; min-height: 30px;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
    )"));
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

    // 列表点击 → 播放
    connect(m_listWidget, &QListWidget::currentRowChanged,
            this, &MainWindow::onSongSelected);

    // 拖拽排序后同步 Playlist
    connect(m_listWidget->model(), &QAbstractItemModel::rowsMoved,
            this, &MainWindow::syncPlaylistFromUI);

    // Playlist 当前改变 → 同步高亮
    connect(m_playlist, &Playlist::currentIndexChanged,
            this, &MainWindow::onPlaylistCurrentChanged);

    // 控制按钮
    connect(m_btnPlayPause, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    connect(m_btnPrev,      &QPushButton::clicked, this, &MainWindow::onPrevious);
    connect(m_btnNext,      &QPushButton::clicked, this, &MainWindow::onNext);
    connect(m_btnShuffle,   &QPushButton::clicked, this, &MainWindow::onToggleShuffle);
    connect(m_btnRepeat,    &QPushButton::clicked, this, &MainWindow::onToggleRepeat);
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
            const int secs = value / 1000;
            m_timeCurrent->setText(QStringLiteral("%1:%2")
                .arg(secs / 60, 2, 10, QLatin1Char('0'))
                .arg(secs % 60, 2, 10, QLatin1Char('0')));
        }
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
    connect(m_player, &Player::artistChanged, this, [this](const QString &artist) {
        m_songArtist->setText(artist.isEmpty()
            ? QStringLiteral("未知艺术家") : artist);
    });
    connect(m_player, &Player::coverArtChanged, this, [this](const QPixmap &cover) {
        if (!cover.isNull())
            m_albumArt->setPixmap(cover.scaled(
                180, 180, Qt::KeepAspectRatio, Qt::SmoothTransformation));
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
    if (m_player->playbackState() == Player::PlaybackState::Paused)
        m_lyricsLabel->setText(QStringLiteral("⏸ 已暂停"));
}

void MainWindow::onNext()
{
    if (!m_playlist->hasNext()) {
        statusBar()->showMessage(QStringLiteral("已是最后一首"), 2000);
        return;
    }
    m_manualTrackChange = true;
    const Song song = m_playlist->next();
    if (song.isValid()) {
        m_player->setSource(song.url());
        m_player->play();
    }
}

void MainWindow::onPrevious()
{
    if (!m_playlist->hasPrevious()) {
        statusBar()->showMessage(QStringLiteral("已是第一首"), 2000);
        return;
    }
    m_manualTrackChange = true;
    const Song song = m_playlist->previous();
    if (song.isValid()) {
        m_player->setSource(song.url());
        m_player->play();
    }
}

void MainWindow::onSongSelected(int index)
{
    if (index < 0 || index >= m_playlist->count())
        return;

    m_manualTrackChange = true;
    m_playlist->setCurrentIndex(index);
    const Song &song = m_playlist->currentSong();
    if (!song.isValid()) return;

    m_songTitle->setText(song.displayTitle());
    m_songArtist->setText(song.displayArtist());

    // 优先加载本地封面文件（文件夹中的 cover.jpg 等）
    if (song.hasCoverFile()) {
        QPixmap cover = song.loadCoverFile();
        if (!cover.isNull())
            m_albumArt->setPixmap(cover);
        else
            m_albumArt->setText(QStringLiteral("🎵"));
    } else {
        m_albumArt->setText(QStringLiteral("🎵"));
    }

    m_subtitle->clear();
    if (song.hasLyrics()) {
        m_subtitle->load(song.lyricsPath);
        m_lyricsLabel->setText(QStringLiteral("♪ 加载歌词..."));
    } else {
        m_lyricsLabel->setText(QStringLiteral("♪ 无歌词"));
    }

    m_player->setSource(song.url());
    m_player->play();
}

// ════════════════════════════════════════════════════════════
//  Player 状态更新
// ════════════════════════════════════════════════════════════

void MainWindow::onPlayerStateChanged(Player::PlaybackState state)
{
    switch (state) {
    case Player::PlaybackState::Playing:
        m_btnPlayPause->setText(QStringLiteral("⏸"));
        m_btnPlayPause->setToolTip(QStringLiteral("暂停"));
        break;
    case Player::PlaybackState::Paused:
        m_btnPlayPause->setText(QStringLiteral("▶"));
        m_btnPlayPause->setToolTip(QStringLiteral("播放"));
        break;
    case Player::PlaybackState::Stopped:
        m_btnPlayPause->setText(QStringLiteral("▶"));
        m_btnPlayPause->setToolTip(QStringLiteral("播放"));
        // 手动切歌（下一首/上一首/双击列表）时，Stopped 由 setSource 触发，
        // 此时不应自动跳到下一首，否则会跳两首
        if (m_manualTrackChange) {
            m_manualTrackChange = false;
            break;
        }
        // 自然结束 → 自动下一首
        if (m_playlist->hasNext()) {
            const Song nextSong = m_playlist->next();
            if (nextSong.isValid()) {
                m_player->setSource(nextSong.url());
                m_player->play();
            }
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
    m_player->setVolume(percent);
    m_volumeLabel->setText(QString::number(percent));

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
}

void MainWindow::onToggleMute()
{
    const bool muted = !m_player->isMuted();
    m_player->setMuted(muted);
    m_btnMute->setChecked(muted);
    m_btnMute->setText(muted ? QStringLiteral("🔇") : QStringLiteral("🔊"));
}

void MainWindow::onToggleShuffle()
{
    const bool enabled = !m_playlist->isShuffle();
    m_playlist->setShuffle(enabled);
    m_btnShuffle->setChecked(enabled);
    statusBar()->showMessage(enabled ? QStringLiteral("随机播放: 开")
                                     : QStringLiteral("随机播放: 关"), 2000);
}

void MainWindow::onToggleRepeat()
{
    using RM = Playlist::RepeatMode;
    RM mode = m_playlist->repeatMode();
    switch (mode) {
    case RM::None: mode = RM::All;  break;
    case RM::All:  mode = RM::One;  break;
    case RM::One:  mode = RM::None; break;
    }
    m_playlist->setRepeatMode(mode);
    m_btnRepeat->setText(mode == RM::One ? QStringLiteral("单曲") : QStringLiteral("循环"));
    statusBar()->showMessage(
        mode == RM::None ? QStringLiteral("循环: 关闭") :
        mode == RM::All  ? QStringLiteral("循环: 列表循环") :
                           QStringLiteral("循环: 单曲循环"), 2000);
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
    m_playlist->setSongs(songs);
    m_listWidget->clear();
    for (const auto &song : songs) {
        auto *item = new QListWidgetItem(song.displayTitle());
        item->setData(Qt::UserRole, song.filePath);      // ← 存入路径
        m_listWidget->addItem(item);
    }

    m_listCountLabel->setText(QStringLiteral("共 %1 首").arg(songs.size()));
    statusBar()->showMessage(QStringLiteral("已加载 %1 首歌曲").arg(songs.size()), 3000);
    m_library->saveLastFolder(m_playTrack->currentFolder());

    if (songs.size() > 0)
        m_listWidget->setCurrentRow(0);
}

void MainWindow::onPlaylistCurrentChanged(int index)
{
    if (index >= 0 && index < m_listWidget->count()) {
        m_listWidget->blockSignals(true);
        m_listWidget->setCurrentRow(index);
        m_listWidget->blockSignals(false);
    }

    if (m_playlist->hasCurrent()) {
        const Song &song = m_playlist->currentSong();
        m_songTitle->setText(song.displayTitle());
        m_songArtist->setText(song.displayArtist());
    }
}

void MainWindow::restorePlaylistFromLibrary()
{
    int repeatMode = m_library->loadRepeatMode();
    m_playlist->setRepeatMode(static_cast<Playlist::RepeatMode>(repeatMode));
    m_playlist->setShuffle(m_library->loadShuffle());
    m_btnShuffle->setChecked(m_playlist->isShuffle());
    if (repeatMode == 1) m_btnRepeat->setText(QStringLiteral("单曲"));

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
    }
}

/// 拖拽排序后，将 QListWidget 中的显示顺序同步回 Playlist
void MainWindow::syncPlaylistFromUI()
{
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
//  歌词
// ════════════════════════════════════════════════════════════

void MainWindow::updateLyrics(qint64 positionMs)
{
    if (!m_subtitle->isLoaded()) return;

    const int idx = m_subtitle->lineIndexAt(positionMs);
    if (idx >= 0) {
        const auto line = m_subtitle->lineAt(idx);
        if (line.text != m_lyricsLabel->text())
            m_lyricsLabel->setText(line.text);
    }
}