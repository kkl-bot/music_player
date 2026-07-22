#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QJsonArray>
#include <QJsonObject>
#include "song.h"

/// PlaylistInfo - 单個用户歌单的数据结构
struct PlaylistInfo {
    QString name;           // 歌单名称
    QStringList songPaths;  // 歌曲绝对路径列表
};

/// PlaylistManager - 管理多个用户歌单 & 自动分类
///
/// 功能：
///   1. 用户歌单的创建、删除、重命名
///   2. 向歌单添加/移除歌曲
///   3. 按歌手/专辑自动分类
///   4. JSON 序列化（由 Library 调用）
class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    explicit PlaylistManager(QObject *parent = nullptr);
    ~PlaylistManager() override;

    // ── 用户歌单 CRUD ──
    /// 创建新歌单，成功返回 true；重名返回 false
    bool createPlaylist(const QString &name);

    /// 删除指定歌单
    bool deletePlaylist(const QString &name);

    /// 重命名歌单
    bool renamePlaylist(const QString &oldName, const QString &newName);

    /// 获取所有歌单名称
    QStringList playlistNames() const;

    /// 歌单数量
    int playlistCount() const;

    /// 指定名称的歌单是否存在
    bool playlistExists(const QString &name) const;

    // ── 歌曲管理 ──
    /// 向指定歌单添加歌曲路径（自动去重）
    bool addSong(const QString &playlistName, const QString &songPath);

    /// 从指定歌单移除指定索引的歌曲
    bool removeSong(const QString &playlistName, int index);

    /// 检查歌曲是否已在歌单中
    bool hasSong(const QString &playlistName, const QString &songPath) const;

    /// 获取歌单内所有歌曲路径
    QStringList playlistSongPaths(const QString &playlistName) const;

    /// 根据当前全部歌曲列表解析歌单中的 Song 对象
    QList<Song> resolveSongs(const QString &playlistName,
                             const QList<Song> &allSongs) const;

    // ── 自动分类 ──
    /// 从歌曲列表中提取所有不重复的歌手名（已排序）
    QStringList artistsFromSongs(const QList<Song> &songs) const;

    /// 从歌曲列表中提取所有不重复的专辑名（已排序）
    QStringList albumsFromSongs(const QList<Song> &songs) const;

    /// 返回指定歌手的歌曲在列表中的索引
    QList<int> songIndicesByArtist(const QString &artist,
                                   const QList<Song> &songs) const;

    /// 返回指定专辑的歌曲在列表中的索引
    QList<int> songIndicesByAlbum(const QString &album,
                                  const QList<Song> &songs) const;

    // ── 序列化（Library 调用） ──
    QJsonArray toJson() const;
    void fromJson(const QJsonArray &data);

signals:
    /// 歌单列表发生任何变化时发射
    void playlistsChanged();

    /// 某歌单的歌曲列表发生变化时发射
    void playlistContentChanged(const QString &playlistName);

private:
    QList<PlaylistInfo> m_playlists;
};

#endif // PLAYLISTMANAGER_H
