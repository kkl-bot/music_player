#ifndef PLAYTRACK_H
#define PLAYTRACK_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QList>
#include <QMap>
#include <QTime>
#include "song.h"

/// PlayTrack - 音乐文件夹浏览与曲目管理
///
/// 功能：
///   1. 弹出文件夹选择对话框，让用户选取音乐目录
///   2. 递归扫描目录下所有支持的音频文件
///   3. 将扫描结果存入内存，供 UI 层访问
///   4. 缓存机制：同一文件夹不会重复扫描
class PlayTrack : public QObject
{
    Q_OBJECT

public:
    explicit PlayTrack(QObject *parent = nullptr);
    ~PlayTrack() override;

    // ── 文件夹浏览与加载 ──
    /// 弹出系统文件夹选择对话框，选择后自动扫描并加载
    /// @return true 如果用户选择了文件夹且加载成功
    bool browseAndLoad();

    /// 直接指定文件夹路径加载
    /// @return true 如果加载成功
    bool loadFromFolder(const QString &dirPath);

    // ── 访问曲目 ──
    const QList<Song> &songs() const;
    int songCount() const;
    Song songAt(int index) const;
    QString currentFolder() const;

    // ── 缓存管理 ──
    /// 清空所有缓存，下次浏览时将重新扫描
    static void clearCache();
    /// 清空指定文件夹的缓存
    static void clearCacheForFolder(const QString &dirPath);

signals:
    /// 当前文件夹发生变化
    void folderChanged(const QString &folderPath);

    /// 歌曲列表加载完成
    void songsLoaded(const QList<Song> &songs);

    /// 加载失败
    void loadFailed(const QString &errorMessage);

private:
    /// 扫描文件夹，返回所有支持的音频文件列表
    static QList<Song> scanFolder(const QString &dirPath);

    /// 从文件路径中提取曲目标题（去掉扩展名、去掉前导数字等）
    static QString extractTitle(const QString &filePath);

    /// 判断文件扩展名是否被支持
    static bool isSupportedAudio(const QString &suffix);

    // ── 状态 ──
    QString       m_currentFolder;
    QList<Song>   m_songs;

    // ── 静态缓存 ──
    // key = 文件夹绝对路径, value = 扫描结果
    static QMap<QString, QList<Song>> s_cache;
};

#endif // PLAYTRACK_H
