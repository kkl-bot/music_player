#ifndef LIBRARY_H
#define LIBRARY_H

#include <QObject>
#include <QString>
#include <QList>
#include <QSettings>
#include "song.h"

/// Library - 统一状态持久化管理器
///
/// 使用 QSettings 在程序重启后恢复上次运行状态：
///   - 上次浏览的文件夹路径
///   - 播放列表（歌曲路径列表）
///   - 当前播放索引
///   - 音量 / 静音状态
///   - 重复 / 随机模式
///   - 窗口位置与大小（预留）
///   - 上次播放位置（预留）
class Library : public QObject
{
    Q_OBJECT

public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    // ── 上次文件夹 ──
    void saveLastFolder(const QString &folderPath);
    QString loadLastFolder() const;

    // ── 播放列表 ──
    /// 将歌曲列表持久化（仅保存文件路径）
    void savePlaylist(const QList<Song> &songs, int currentIndex);

    /// 从持久化恢复歌曲列表
    /// @param outCurrentIndex 输出当前索引
    /// @return 恢复的歌曲列表（仅 filePath 有效，需重新构造）
    QList<Song> loadPlaylist(int &outCurrentIndex) const;

    // ── 音量 ──
    void saveVolume(int percent);
    int loadVolume(int defaultPercent = 70) const;

    void saveMuted(bool muted);
    bool loadMuted(bool defaultMuted = false) const;

    // ── 播放模式 ──
    void saveRepeatMode(int mode);  // 0=None, 1=One, 2=All
    int loadRepeatMode(int defaultMode = 0) const;

    void saveShuffle(bool enabled);
    bool loadShuffle(bool defaultShuffle = false) const;

    // ── 上次播放的歌曲路径 ──
    void saveLastPlayedSong(const QString &songPath);
    QString loadLastPlayedSong() const;

    // ── 上次播放位置（按歌曲路径索引） ──
    void saveLastPosition(const QString &songPath, qint64 positionMs);
    qint64 loadLastPosition(const QString &songPath) const;

    // ── 歌词时间微调偏移（按歌曲路径索引） ──
    void saveLyricsOffset(const QString &songPath, qint64 offsetMs);
    qint64 loadLyricsOffset(const QString &songPath, qint64 defaultOffset = 0) const;

    // ── 窗口几何 ──
    void saveWindowGeometry(const QByteArray &geometry);
    QByteArray loadWindowGeometry() const;

    // ── 工具 ──
    void clearAll();
    QString settingsFilePath() const;

private:
    QSettings m_settings;
};

#endif // LIBRARY_H
