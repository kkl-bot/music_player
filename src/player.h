#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QUrl>
#include <QPixmap>
#include <QMediaPlayer>
#include <QAudioOutput>
#include <QMediaMetaData>

/// Player - 对 Qt Multimedia (QMediaPlayer + QAudioOutput) 的简化封装
///
/// 提供统一的播放/暂停/停止/跳转/音量控制接口，
/// UI 层只需连接此类的信号与槽即可完成所有媒体控制。
class Player : public QObject
{
    Q_OBJECT

public:
    /// 播放状态枚举（与 QMediaPlayer::PlaybackState 保持一致）
    enum class PlaybackState {
        Stopped = 0,
        Playing = 1,
        Paused  = 2,
    };
    Q_ENUM(PlaybackState)

    explicit Player(QObject *parent = nullptr);
    ~Player() override;

    // ── 媒体源 ──
    void setSource(const QUrl &url);
    void setSourceFromFile(const QString &filePath);
    QUrl source() const;

    // ── 播放控制 ──
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePlayPause();

    // ── 跳转 ──
    Q_INVOKABLE void seek(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;

    // ── 音量 / 静音 ──
    void setVolume(int percent);          // 0 ~ 100
    int volume() const;                   // 0 ~ 100
    void setMuted(bool muted);
    bool isMuted() const;

    // ── 底层访问（用于音频可视化等） ──
    QMediaPlayer *mediaPlayer() const { return m_mediaPlayer; }

    // ── 状态查询 ──
    PlaybackState playbackState() const;
    bool isPlaying() const;
    QString errorString() const;

    // ── 元数据查询 ──
    QString currentArtist() const;
    QString currentTitle() const;
    QPixmap currentCoverArt() const;

signals:
    void sourceChanged(const QUrl &url);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(Player::PlaybackState state);
    void volumeChanged(int percent);
    void mutedChanged(bool muted);
    void errorOccurred(const QString &errorString);

    /// 媒体加载完成（可安全 seek）
    void mediaLoaded();

    /// 元数据变更（艺术家、封面等就绪时发射）
    void metaDataChanged();
    void artistChanged(const QString &artist);
    void coverArtChanged(const QPixmap &cover);

private slots:
    void onMediaError(QMediaPlayer::Error error);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onPositionChanged(qint64 positionMs);
    void onDurationChanged(qint64 durationMs);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onMetaDataChanged();

private:
    QMediaPlayer  *m_mediaPlayer  = nullptr;
    QAudioOutput  *m_audioOutput  = nullptr;
    QPixmap        m_cachedCover;
};

#endif // PLAYER_H
