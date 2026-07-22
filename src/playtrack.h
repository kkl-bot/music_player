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
///   2. 递归扫描目录下所有支持的音频文件
///   3. 通过 Library 缓存扫描结果，避免重复扫描
class PlayTrack : public QObject
{
    Q_OBJECT

public:
    explicit PlayTrack(QObject *parent = nullptr);
    ~PlayTrack() override;

    /// 设置 Library 指针（用于缓存读写）
    void setLibrary(Library *library);

    // ── 文件夹浏览与加载 ──
    /// 弹出系统文件夹选择对话框，选择后自动扫描并加载
    bool browseAndLoad();

    /// 直接指定文件夹路径加载（优先使用 Library 缓存）
    bool loadFromFolder(const QString &dirPath);

    // ── 访问曲目 ──
    const QList<Song> &songs() const;
    int songCount() const;
    Song songAt(int index) const;
    QString currentFolder() const;

    // ── 缓存管理（委托给 Library） ──
    void clearCache();
    void clearCacheForFolder(const QString &dirPath);

signals:
    void folderChanged(const QString &folderPath);
    void songsLoaded(const QList<Song> &songs);
    void loadFailed(const QString &errorMessage);

private:
    /// 扫描文件夹，返回所有支持的音频文件列表
    static QList<Song> scanFolder(const QString &dirPath);

    static QString extractTitle(const QString &filePath);
    static bool isSupportedAudio(const QString &suffix);

    // ── 状态 ──
    QString       m_currentFolder;
    QList<Song>   m_songs;
    Library      *m_library = nullptr;
};

#endif // PLAYTRACK_H
