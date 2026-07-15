#include "player.h"

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

Player::Player(QObject *parent)
    : QObject(parent)
    , m_mediaPlayer(new QMediaPlayer(this))
    , m_audioOutput(new QAudioOutput(this))
{
    // 关联音频输出
    m_mediaPlayer->setAudioOutput(m_audioOutput);

    // ── 连接 QMediaPlayer 信号 ──
    connect(m_mediaPlayer, &QMediaPlayer::errorOccurred,
            this, &Player::onMediaError);
    connect(m_mediaPlayer, &QMediaPlayer::playbackStateChanged,
            this, &Player::onPlaybackStateChanged);
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged,
            this, &Player::onPositionChanged);
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged,
            this, &Player::onDurationChanged);
    connect(m_mediaPlayer, &QMediaPlayer::mediaStatusChanged,
            this, &Player::onMediaStatusChanged);
    connect(m_mediaPlayer, &QMediaPlayer::metaDataChanged,
            this, &Player::onMetaDataChanged);
}

Player::~Player() = default;

// ════════════════════════════════════════════════════════════
//  媒体源
// ════════════════════════════════════════════════════════════

void Player::setSource(const QUrl &url)
{
    if (url == m_mediaPlayer->source())
        return;

    m_cachedCover = QPixmap();   // 清空旧封面

    // 重置进度显示
    emit positionChanged(0);
    emit durationChanged(0);

    m_mediaPlayer->setSource(url);
    emit sourceChanged(url);
}

void Player::setSourceFromFile(const QString &filePath)
{
    setSource(QUrl::fromLocalFile(filePath));
}

QUrl Player::source() const
{
    return m_mediaPlayer->source();
}

// ════════════════════════════════════════════════════════════
//  播放控制
// ════════════════════════════════════════════════════════════

void Player::play()
{
    if (m_mediaPlayer->source().isEmpty())
        return;

    m_mediaPlayer->play();
}

void Player::pause()
{
    m_mediaPlayer->pause();
}

void Player::stop()
{
    m_mediaPlayer->stop();
}

void Player::togglePlayPause()
{
    switch (m_mediaPlayer->playbackState()) {
    case QMediaPlayer::PlayingState:
        pause();
        break;
    default:
        play();
        break;
    }
}

// ════════════════════════════════════════════════════════════
//  跳转
// ════════════════════════════════════════════════════════════

void Player::seek(qint64 positionMs)
{
    m_mediaPlayer->setPosition(positionMs);
}

qint64 Player::position() const
{
    return m_mediaPlayer->position();
}

qint64 Player::duration() const
{
    return m_mediaPlayer->duration();
}

// ════════════════════════════════════════════════════════════
//  音量 / 静音
// ════════════════════════════════════════════════════════════

void Player::setVolume(int percent)
{
    const int clamped = qBound(0, percent, 100);
    const qreal volume = static_cast<qreal>(clamped) / 100.0;
    m_audioOutput->setVolume(volume);
    emit volumeChanged(clamped);
}

int Player::volume() const
{
    return qRound(m_audioOutput->volume() * 100.0);
}

void Player::setMuted(bool muted)
{
    m_audioOutput->setMuted(muted);
    emit mutedChanged(muted);
}

bool Player::isMuted() const
{
    return m_audioOutput->isMuted();
}

bool Player::isPlaying() const
{
    return m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState;
}

// ════════════════════════════════════════════════════════════
//  状态查询
// ════════════════════════════════════════════════════════════

Player::PlaybackState Player::playbackState() const
{
    return static_cast<PlaybackState>(m_mediaPlayer->playbackState());
}

QString Player::errorString() const
{
    return m_mediaPlayer->errorString();
}

// ════════════════════════════════════════════════════════════
//  元数据查询
// ════════════════════════════════════════════════════════════

QString Player::currentArtist() const
{
    const QString artist = m_mediaPlayer->metaData().value(QMediaMetaData::Author).toString();
    if (artist.isEmpty())
        return m_mediaPlayer->metaData().value(QMediaMetaData::AlbumArtist).toString();
    return artist;
}

QString Player::currentTitle() const
{
    return m_mediaPlayer->metaData().value(QMediaMetaData::Title).toString();
}

QPixmap Player::currentCoverArt() const
{
    return m_cachedCover;
}

// ════════════════════════════════════════════════════════════
//  内部槽函数 — 桥接 QMediaPlayer 信号 → Player 信号
// ════════════════════════════════════════════════════════════

void Player::onMediaError(QMediaPlayer::Error error)
{
    Q_UNUSED(error);
    emit errorOccurred(m_mediaPlayer->errorString());
}

void Player::onPlaybackStateChanged(QMediaPlayer::PlaybackState state)
{
    emit stateChanged(static_cast<PlaybackState>(state));
}

void Player::onPositionChanged(qint64 positionMs)
{
    emit positionChanged(positionMs);
}

void Player::onDurationChanged(qint64 durationMs)
{
    emit durationChanged(durationMs);
}

void Player::onMediaStatusChanged(QMediaPlayer::MediaStatus status)
{
    switch (status) {
    case QMediaPlayer::LoadedMedia:
        emit durationChanged(m_mediaPlayer->duration());
        break;
    case QMediaPlayer::InvalidMedia:
        emit errorOccurred(QStringLiteral("无法加载媒体文件: ") + m_mediaPlayer->source().toString());
        break;
    default:
        break;
    }
}

void Player::onMetaDataChanged()
{
    // 读取内嵌封面
    const QVariant coverVariant = m_mediaPlayer->metaData().value(QMediaMetaData::CoverArtImage);
    if (coverVariant.isValid()) {
        m_cachedCover = coverVariant.value<QPixmap>();
        emit coverArtChanged(m_cachedCover);
    }

    // 读取艺术家
    const QString artist = currentArtist();
    if (!artist.isEmpty())
        emit artistChanged(artist);

    emit metaDataChanged();
}
