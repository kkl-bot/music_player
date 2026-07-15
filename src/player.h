#ifndef PLAYER_H
#define PLAYER_H

#include <QObject>
#include <QUrl>
#include <QMediaPlayer>
#include <QAudioOutput>

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

    // ── 状态查询 ──
    PlaybackState playbackState() const;
    QString errorString() const;

signals:
    void sourceChanged(const QUrl &url);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void stateChanged(Player::PlaybackState state);
    void volumeChanged(int percent);
    void mutedChanged(bool muted);
    void errorOccurred(const QString &errorString);

private slots:
    void onMediaError(QMediaPlayer::Error error);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);
    void onPositionChanged(qint64 positionMs);
    void onDurationChanged(qint64 durationMs);

private:
    QMediaPlayer  *m_mediaPlayer  = nullptr;
    QAudioOutput  *m_audioOutput  = nullptr;
};

#endif // PLAYER_H
