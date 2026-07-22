#include "playlistmanager.h"
#include <QSet>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

PlaylistManager::PlaylistManager(QObject *parent)
    : QObject(parent)
{
}

PlaylistManager::~PlaylistManager() = default;

// ════════════════════════════════════════════════════════════
//  用户歌单 CRUD
// ════════════════════════════════════════════════════════════

bool PlaylistManager::createPlaylist(const QString &name)
{
    if (name.trimmed().isEmpty())
        return false;

    // 检查重名
    for (const auto &pl : m_playlists) {
        if (pl.name == name.trimmed())
            return false;
    }

    PlaylistInfo info;
    info.name = name.trimmed();
    m_playlists.append(info);
    emit playlistsChanged();
    return true;
}

bool PlaylistManager::deletePlaylist(const QString &name)
{
    for (int i = 0; i < m_playlists.size(); ++i) {
        if (m_playlists[i].name == name) {
            m_playlists.removeAt(i);
            emit playlistsChanged();
            return true;
        }
    }
    return false;
}

bool PlaylistManager::renamePlaylist(const QString &oldName, const QString &newName)
{
    if (newName.trimmed().isEmpty())
        return false;

    // 检查新名称是否已被占用
    for (const auto &pl : m_playlists) {
        if (pl.name == newName.trimmed())
            return false;
    }

    for (auto &pl : m_playlists) {
        if (pl.name == oldName) {
            pl.name = newName.trimmed();
            emit playlistsChanged();
            return true;
        }
    }
    return false;
}

QStringList PlaylistManager::playlistNames() const
{
    QStringList names;
    names.reserve(m_playlists.size());
    for (const auto &pl : m_playlists)
        names.append(pl.name);
    return names;
}

int PlaylistManager::playlistCount() const
{
    return m_playlists.size();
}

bool PlaylistManager::playlistExists(const QString &name) const
{
    for (const auto &pl : m_playlists) {
        if (pl.name == name)
            return true;
    }
    return false;
}

// ════════════════════════════════════════════════════════════
//  歌曲管理
// ════════════════════════════════════════════════════════════

bool PlaylistManager::addSong(const QString &playlistName, const QString &songPath)
{
    for (auto &pl : m_playlists) {
        if (pl.name == playlistName) {
            // 去重
            if (pl.songPaths.contains(songPath))
                return false;
            pl.songPaths.append(songPath);
            emit playlistContentChanged(playlistName);
            emit playlistsChanged();
            return true;
        }
    }
    return false;
}

bool PlaylistManager::removeSong(const QString &playlistName, int index)
{
    for (auto &pl : m_playlists) {
        if (pl.name == playlistName) {
            if (index < 0 || index >= pl.songPaths.size())
                return false;
            pl.songPaths.removeAt(index);
            emit playlistContentChanged(playlistName);
            emit playlistsChanged();
            return true;
        }
    }
    return false;
}

bool PlaylistManager::hasSong(const QString &playlistName, const QString &songPath) const
{
    for (const auto &pl : m_playlists) {
        if (pl.name == playlistName)
            return pl.songPaths.contains(songPath);
    }
    return false;
}

QStringList PlaylistManager::playlistSongPaths(const QString &playlistName) const
{
    for (const auto &pl : m_playlists) {
        if (pl.name == playlistName)
            return pl.songPaths;
    }
    return {};
}

QList<Song> PlaylistManager::resolveSongs(const QString &playlistName,
                                           const QList<Song> &allSongs) const
{
    const QStringList paths = playlistSongPaths(playlistName);
    if (paths.isEmpty())
        return {};

    // 构建路径→Song 映射
    QMap<QString, Song> pathMap;
    for (const auto &s : allSongs)
        pathMap.insert(s.filePath, s);

    QList<Song> result;
    for (const auto &path : paths) {
        if (pathMap.contains(path))
            result.append(pathMap.value(path));
    }
    return result;
}

// ════════════════════════════════════════════════════════════
//  自动分类
// ════════════════════════════════════════════════════════════

QStringList PlaylistManager::artistsFromSongs(const QList<Song> &songs) const
{
    QSet<QString> artists;
    for (const auto &s : songs) {
        if (!s.artist.isEmpty())
            artists.insert(s.artist);
    }
    QStringList sorted = artists.values();
    std::sort(sorted.begin(), sorted.end(),
              [](const QString &a, const QString &b) {
                  return a.localeAwareCompare(b) < 0;
              });
    return sorted;
}

QStringList PlaylistManager::albumsFromSongs(const QList<Song> &songs) const
{
    QSet<QString> albums;
    for (const auto &s : songs) {
        if (!s.album.isEmpty())
            albums.insert(s.album);
    }
    QStringList sorted = albums.values();
    std::sort(sorted.begin(), sorted.end(),
              [](const QString &a, const QString &b) {
                  return a.localeAwareCompare(b) < 0;
              });
    return sorted;
}

QList<int> PlaylistManager::songIndicesByArtist(const QString &artist,
                                                 const QList<Song> &songs) const
{
    QList<int> indices;
    for (int i = 0; i < songs.size(); ++i) {
        if (songs[i].artist == artist)
            indices.append(i);
    }
    return indices;
}

QList<int> PlaylistManager::songIndicesByAlbum(const QString &album,
                                                const QList<Song> &songs) const
{
    QList<int> indices;
    for (int i = 0; i < songs.size(); ++i) {
        if (songs[i].album == album)
            indices.append(i);
    }
    return indices;
}

// ════════════════════════════════════════════════════════════
//  序列化
// ════════════════════════════════════════════════════════════

QJsonArray PlaylistManager::toJson() const
{
    QJsonArray arr;
    for (const auto &pl : m_playlists) {
        QJsonObject obj;
        obj[QStringLiteral("name")] = pl.name;
        QJsonArray paths;
        for (const auto &p : pl.songPaths)
            paths.append(p);
        obj[QStringLiteral("songs")] = paths;
        arr.append(obj);
    }
    return arr;
}

void PlaylistManager::fromJson(const QJsonArray &data)
{
    m_playlists.clear();
    for (const auto &val : data) {
        QJsonObject obj = val.toObject();
        PlaylistInfo info;
        info.name = obj[QStringLiteral("name")].toString();
        const QJsonArray paths = obj[QStringLiteral("songs")].toArray();
        info.songPaths.reserve(paths.size());
        for (const auto &p : paths)
            info.songPaths.append(p.toString());
        if (!info.name.isEmpty())
            m_playlists.append(info);
    }
    emit playlistsChanged();
}
