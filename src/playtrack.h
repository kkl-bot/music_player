#ifndef PLAYTRACK_H
#define PLAYTRACK_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QList>
#include <QTime>
#include "song.h"

class Library;

/// PlayTrack - 音乐文件夹浏览与曲目管理
///
/// 功能：
///   1. 弹出文件夹选择对话框，让用户选取音乐目录
///   2. 扫描文件夹，所有歌曲打包存入缓存
///   3. 提供文件夹变化检测（新增/删除文件）
///   4. 播放时歌曲信息直接从缓存读取，不再重新探测
class PlayTrack : public QObject
{
    Q_OBJECT

public:
    explicit PlayTrack(QObject *parent = nullptr);
    ~PlayTrack() override;

    void setLibrary(Library *library);

    // ── 文件夹浏览与加载 ──
    bool browseAndLoad();
    /// 加载文件夹：清除旧缓存 → 扫描 → 缓存 → 发射 songsLoaded
    bool loadFromFolder(const QString &dirPath);

    // ── 访问曲目 ──
    const QList<Song> &songs() const;
    int songCount() const;
    Song songAt(int index) const;
    QString currentFolder() const;

    // ── 文件夹变化检测 ──
    /// 快速扫描文件夹，检查文件数量/名称变化。有变化时自动重新加载。
    /// @return true 如果文件夹内容有变化（已自动重载）
    bool refreshIfChanged();

    // ── 缓存管理 ──
    void clearCache();
    void clearCacheForFolder(const QString &dirPath);

signals:
    void folderChanged(const QString &folderPath);
    void songsLoaded(const QList<Song> &songs);
    void loadFailed(const QString &errorMessage);

private:
    static QList<Song> scanFolder(const QString &dirPath);
    static QString extractTitle(const QString &filePath);
    static bool isSupportedAudio(const QString &suffix);

    // ── 状态 ──
    QString       m_currentFolder;
    QList<Song>   m_songs;
    Library      *m_library = nullptr;
};

#endif // PLAYTRACK_H
