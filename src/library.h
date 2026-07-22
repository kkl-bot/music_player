#ifndef LIBRARY_H
#define LIBRARY_H

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include "song.h"

/// Library - 统一持久化管理器（JSON 文件）
///
/// 所有持久化数据存为单一 JSON 文件 `library.json`，
/// 包括：状态设置、播放列表、文件夹缓存、歌单数据等。
class Library : public QObject
{
    Q_OBJECT

public:
    explicit Library(QObject *parent = nullptr);
    ~Library() override;

    // ── JSON 文件路径 ──
    QString filePath() const;

    // ── 上次文件夹 ──
    void saveLastFolder(const QString &folderPath);
    QString loadLastFolder() const;

    // ── 播放列表 ──
    void savePlaylist(const QList<Song> &songs, int currentIndex);
    QList<Song> loadPlaylist(int &outCurrentIndex) const;

    // ── 音量 ──
    void saveVolume(int percent);
    int loadVolume(int defaultPercent = 70) const;
    void saveMuted(bool muted);
    bool loadMuted(bool defaultMuted = false) const;

    // ── 播放模式 ──
    void saveRepeatMode(int mode);
    int loadRepeatMode(int defaultMode = 0) const;
    void saveShuffle(bool enabled);
    bool loadShuffle(bool defaultShuffle = false) const;

    // ── 上次播放 ──
    void saveLastPlayedSong(const QString &songPath);
    QString loadLastPlayedSong() const;
    void saveLastPosition(const QString &songPath, qint64 positionMs);
    qint64 loadLastPosition(const QString &songPath) const;

    // ── 歌词偏移 ──
    void saveLyricsOffset(const QString &songPath, qint64 offsetMs);
    qint64 loadLyricsOffset(const QString &songPath, qint64 defaultOffset = 0) const;

    // ── 主题 / 背景 ──
    void saveTheme(int theme);
    int loadTheme(int defaultTheme = 0) const;
    void saveDiyBgFolder(const QString &folderPath);
    QString loadDiyBgFolder() const;

    // ── 窗口几何 ──
    void saveWindowGeometry(const QByteArray &geometry);
    QByteArray loadWindowGeometry() const;

    // ── 歌单数据 ──
    void savePlaylists(const QJsonArray &data);
    QJsonArray loadPlaylists() const;

    // ── 文件夹扫描缓存 ──
    /// 缓存指定文件夹的扫描结果
    void cacheFolderSongs(const QString &folderPath, const QList<Song> &songs);
    /// 读取文件夹缓存
    QList<Song> loadCachedFolderSongs(const QString &folderPath) const;
    /// 检查文件夹是否有缓存
    bool hasFolderCache(const QString &folderPath) const;
    /// 清空所有文件夹缓存
    void clearFolderCache();
    /// 清空指定文件夹缓存
    void clearFolderCacheFor(const QString &folderPath);

    // ── 工具 ──
    void clearAll();

private:
    QJsonObject m_data;     // 内存中的完整数据
    QString m_filePath;     // JSON 文件路径

    void loadFromFile();
    void saveToFile();
};

#endif // LIBRARY_H
