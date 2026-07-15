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
}

Player::~Player() = default;

// ════════════════════════════════════════════════════════════
//  媒体源
// ════════════════════════════════════════════════════════════

void Player::setSource(const QUrl &url)
{
    if (url == m_mediaPlayer->source())
        return;

    m_mediaPlayer->setSource(url);

    // 设置新源后自动加载
    if (!url.isEmpty())
        m_mediaPlayer->play();

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
    const qreal volume = qBound(0.0, percent / 100.0, 1.0);
    m_audioOutput->setVolume(volume);
    emit volumeChanged(percent);
}

int Player::volume() const
{
    return static_cast<int>(m_audioOutput->volume() * 100);
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
