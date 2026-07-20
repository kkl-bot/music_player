#include "library.h"
#include <QFileInfo>

// ── QSettings 键名 ──
static const QString kKeyLastFolder    = QStringLiteral("lastFolder");
static const QString kKeyPlaylist      = QStringLiteral("playlist/songs");
static const QString kKeyPlaylistTitles    = QStringLiteral("playlist/titles");
static const QString kKeyPlaylistArtists   = QStringLiteral("playlist/artists");
static const QString kKeyPlaylistDurations = QStringLiteral("playlist/durations");
static const QString kKeyCurrentIndex  = QStringLiteral("playlist/currentIndex");
static const QString kKeyVolume        = QStringLiteral("audio/volume");
static const QString kKeyMuted         = QStringLiteral("audio/muted");
static const QString kKeyRepeatMode    = QStringLiteral("playback/repeatMode");
static const QString kKeyShuffle       = QStringLiteral("playback/shuffle");
static const QString kKeyLastPlayedSong = QStringLiteral("playback/lastPlayedSong");
static const QString kKeyWindowGeom    = QStringLiteral("window/geometry");

static const int kMaxRecentPositions = 50;  // 最多记住 50 首歌的位置

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

Library::Library(QObject *parent)
    : QObject(parent)
    , m_settings(QStringLiteral("MusicPlayer"), QStringLiteral("MusicPlayer"))
{
}

Library::~Library() = default;

// ════════════════════════════════════════════════════════════
//  上次文件夹
// ════════════════════════════════════════════════════════════

void Library::saveLastFolder(const QString &folderPath)
{
    m_settings.setValue(kKeyLastFolder, folderPath);
    m_settings.sync();
}

QString Library::loadLastFolder() const
{
    return m_settings.value(kKeyLastFolder).toString();
}

// ════════════════════════════════════════════════════════════
//  播放列表
// ════════════════════════════════════════════════════════════

void Library::savePlaylist(const QList<Song> &songs, int currentIndex)
{
    QStringList paths, titles, artists;
    QList<qint64> durations;
    paths.reserve(songs.size());
    titles.reserve(songs.size());
    artists.reserve(songs.size());
    durations.reserve(songs.size());
    for (const auto &song : songs) {
        paths.append(song.filePath);
        titles.append(song.title);
        artists.append(song.artist);
        durations.append(song.durationMs);
    }

    m_settings.setValue(kKeyPlaylist, paths);
    m_settings.setValue(kKeyPlaylistTitles, titles);
    m_settings.setValue(kKeyPlaylistArtists, artists);
    m_settings.setValue(kKeyPlaylistDurations, QVariant::fromValue(durations));
    m_settings.setValue(kKeyCurrentIndex, currentIndex);
    m_settings.sync();
}

QList<Song> Library::loadPlaylist(int &outCurrentIndex) const
{
    outCurrentIndex = m_settings.value(kKeyCurrentIndex, -1).toInt();

    const QStringList paths     = m_settings.value(kKeyPlaylist).toStringList();
    const QStringList titles    = m_settings.value(kKeyPlaylistTitles).toStringList();
    const QStringList artists   = m_settings.value(kKeyPlaylistArtists).toStringList();
    const QList<QVariant> durVar = m_settings.value(kKeyPlaylistDurations).toList();

    QList<Song> songs;
    songs.reserve(paths.size());

    for (int i = 0; i < paths.size(); ++i) {
        const QString &path = paths.at(i);
        if (!QFileInfo::exists(path))
            continue;

        Song song(path);
        // 恢复保存的标题（不是文件名 completeBaseName）
        if (i < titles.size() && !titles.at(i).isEmpty())
            song.title = titles.at(i);
        // 恢复保存的艺术家（避免重启后丢失）
        if (i < artists.size() && !artists.at(i).isEmpty())
            song.artist = artists.at(i);
        // 恢复保存的时长
        if (i < durVar.size() && durVar.at(i).isValid())
            song.durationMs = durVar.at(i).toLongLong();
        songs.append(song);
    }

    // 修正越界的索引
    if (outCurrentIndex >= songs.size())
        outCurrentIndex = songs.isEmpty() ? -1 : 0;

    return songs;
}

// ════════════════════════════════════════════════════════════
//  音量
// ════════════════════════════════════════════════════════════

void Library::saveVolume(int percent)
{
    m_settings.setValue(kKeyVolume, percent);
    m_settings.sync();
}

int Library::loadVolume(int defaultPercent) const
{
    return m_settings.value(kKeyVolume, defaultPercent).toInt();
}

void Library::saveMuted(bool muted)
{
    m_settings.setValue(kKeyMuted, muted);
    m_settings.sync();
}

bool Library::loadMuted(bool defaultMuted) const
{
    return m_settings.value(kKeyMuted, defaultMuted).toBool();
}

// ════════════════════════════════════════════════════════════
//  播放模式
// ════════════════════════════════════════════════════════════

void Library::saveRepeatMode(int mode)
{
    m_settings.setValue(kKeyRepeatMode, mode);
    m_settings.sync();
}

int Library::loadRepeatMode(int defaultMode) const
{
    return m_settings.value(kKeyRepeatMode, defaultMode).toInt();
}

void Library::saveShuffle(bool enabled)
{
    m_settings.setValue(kKeyShuffle, enabled);
    m_settings.sync();
}

bool Library::loadShuffle(bool defaultShuffle) const
{
    return m_settings.value(kKeyShuffle, defaultShuffle).toBool();
}

// ════════════════════════════════════════════════════════════
//  上次播放的歌曲
// ════════════════════════════════════════════════════════════

void Library::saveLastPlayedSong(const QString &songPath)
{
    m_settings.setValue(kKeyLastPlayedSong, songPath);
    m_settings.sync();
}

QString Library::loadLastPlayedSong() const
{
    return m_settings.value(kKeyLastPlayedSong).toString();
}

// ════════════════════════════════════════════════════════════
//  上次播放位置
// ════════════════════════════════════════════════════════════

void Library::saveLastPosition(const QString &songPath, qint64 positionMs)
{
    // 移除旧记录避免无限增长
    QStringList allKeys = m_settings.allKeys().filter("position/");
    if (allKeys.size() >= kMaxRecentPositions) {
        // 简单地全部清理，保留当前这一条
        for (const auto &key : allKeys)
            m_settings.remove(key);
    }

    const QString key = QStringLiteral("position/") + songPath;
    m_settings.setValue(key, static_cast<qlonglong>(positionMs));
    m_settings.sync();
}

qint64 Library::loadLastPosition(const QString &songPath) const
{
    const QString key = QStringLiteral("position/") + songPath;
    return static_cast<qint64>(m_settings.value(key, 0).toLongLong());
}

// ════════════════════════════════════════════════════════════
//  歌词时间微调偏移
// ════════════════════════════════════════════════════════════

void Library::saveLyricsOffset(const QString &songPath, qint64 offsetMs)
{
    const QString key = QStringLiteral("lyricsoffset/") + songPath;
    m_settings.setValue(key, static_cast<qlonglong>(offsetMs));
    m_settings.sync();
}

qint64 Library::loadLyricsOffset(const QString &songPath, qint64 defaultOffset) const
{
    const QString key = QStringLiteral("lyricsoffset/") + songPath;
    return static_cast<qint64>(m_settings.value(key, static_cast<qlonglong>(defaultOffset)).toLongLong());
}

// ════════════════════════════════════════════════════════════
//  窗口几何
// ════════════════════════════════════════════════════════════
//  主题
// ════════════════════════════════════════════════════════════

static const QString kKeyTheme = QStringLiteral("app/theme");

void Library::saveTheme(int theme)
{
    m_settings.setValue(kKeyTheme, theme);
    m_settings.sync();
}

int Library::loadTheme(int defaultTheme) const
{
    return m_settings.value(kKeyTheme, defaultTheme).toInt();
}

// ════════════════════════════════════════════════════════════
//  DIY 背景
// ════════════════════════════════════════════════════════════

static const QString kKeyDiyBg = QStringLiteral("app/diyBgFolder");

void Library::saveDiyBgFolder(const QString &folderPath)
{
    m_settings.setValue(kKeyDiyBg, folderPath);
    m_settings.sync();
}

QString Library::loadDiyBgFolder() const
{
    return m_settings.value(kKeyDiyBg).toString();
}

// ════════════════════════════════════════════════════════════

void Library::saveWindowGeometry(const QByteArray &geometry)
{
    m_settings.setValue(kKeyWindowGeom, geometry);
    m_settings.sync();
}

QByteArray Library::loadWindowGeometry() const
{
    return m_settings.value(kKeyWindowGeom).toByteArray();
}

// ════════════════════════════════════════════════════════════
//  工具
// ════════════════════════════════════════════════════════════

void Library::clearAll()
{
    m_settings.clear();
    m_settings.sync();
}

QString Library::settingsFilePath() const
{
    return m_settings.fileName();
}
