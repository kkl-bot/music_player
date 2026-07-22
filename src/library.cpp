#include "library.h"
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>

static const int kMaxRecentPositions = 50;

// ── JSON 键名 ──
static const QString kAppDir = QStringLiteral("MusicPlayer");
static const QString kFile   = QStringLiteral("library.json");

// ════════════════════════════════════════════════════════════
//  构造 / 析构 — 启动时加载整个 JSON
// ════════════════════════════════════════════════════════════

Library::Library(QObject *parent)
    : QObject(parent)
{
    // 存放在标准应用数据目录
    const QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_filePath = dataDir + QStringLiteral("/") + kFile;
    loadFromFile();
}

Library::~Library() = default;

QString Library::filePath() const
{
    return m_filePath;
}

// ════════════════════════════════════════════════════════════
//  内部 — 文件 I/O
// ════════════════════════════════════════════════════════════

void Library::loadFromFile()
{
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isObject())
        m_data = doc.object();
}

void Library::saveToFile()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Library: failed to write %s", qPrintable(m_filePath));
        return;
    }

    QJsonDocument doc(m_data);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

// ════════════════════════════════════════════════════════════
//  辅助 — Song ↔ QJsonObject 转换
// ════════════════════════════════════════════════════════════

static QJsonObject songToJson(const Song &s)
{
    QJsonObject obj;
    obj[QStringLiteral("path")]     = s.filePath;
    obj[QStringLiteral("title")]    = s.title;
    obj[QStringLiteral("artist")]   = s.artist;
    obj[QStringLiteral("album")]    = s.album;
    obj[QStringLiteral("duration")] = static_cast<qint64>(s.durationMs);
    return obj;
}

static Song songFromJson(const QJsonObject &obj)
{
    Song s;
    s.filePath   = obj[QStringLiteral("path")].toString();
    s.title      = obj[QStringLiteral("title")].toString();
    s.artist     = obj[QStringLiteral("artist")].toString();
    s.album      = obj[QStringLiteral("album")].toString();
    s.durationMs = static_cast<qint64>(obj[QStringLiteral("duration")].toDouble());
    // 重新探测歌词/封面路径（文件可能已被移动）
    if (!s.filePath.isEmpty()) {
        QFileInfo fi(s.filePath);
        if (fi.exists()) {
            s.lyricsPath = Song::detectLyricsFile(s.filePath);
            s.coverPath  = Song::detectCoverFile(s.filePath);
        }
    }
    return s;
}

// ════════════════════════════════════════════════════════════
//  上次文件夹
// ════════════════════════════════════════════════════════════

void Library::saveLastFolder(const QString &folderPath)
{
    m_data[QStringLiteral("lastFolder")] = folderPath;
    saveToFile();
}

QString Library::loadLastFolder() const
{
    return m_data.value(QStringLiteral("lastFolder")).toString();
}

// ════════════════════════════════════════════════════════════
//  播放列表
// ════════════════════════════════════════════════════════════

void Library::savePlaylist(const QList<Song> &songs, int currentIndex)
{
    QJsonArray arr;
    for (const auto &s : songs)
        arr.append(songToJson(s));

    QJsonObject pl;
    pl[QStringLiteral("songs")]        = arr;
    pl[QStringLiteral("currentIndex")] = currentIndex;

    m_data[QStringLiteral("playlist")] = pl;
    saveToFile();
}

QList<Song> Library::loadPlaylist(int &outCurrentIndex) const
{
    QList<Song> result;
    const QJsonObject pl = m_data.value(QStringLiteral("playlist")).toObject();
    outCurrentIndex = pl.value(QStringLiteral("currentIndex")).toInt(-1);

    const QJsonArray arr = pl.value(QStringLiteral("songs")).toArray();
    for (const auto &v : arr) {
        Song s = songFromJson(v.toObject());
        if (QFileInfo::exists(s.filePath))
            result.append(s);
    }

    if (outCurrentIndex >= result.size())
        outCurrentIndex = result.isEmpty() ? -1 : 0;

    return result;
}

// ════════════════════════════════════════════════════════════
//  音量
// ════════════════════════════════════════════════════════════

void Library::saveVolume(int percent)
{
    QJsonObject audio = m_data.value(QStringLiteral("audio")).toObject();
    audio[QStringLiteral("volume")] = percent;
    m_data[QStringLiteral("audio")] = audio;
    saveToFile();
}

int Library::loadVolume(int defaultPercent) const
{
    return m_data.value(QStringLiteral("audio")).toObject()
           .value(QStringLiteral("volume")).toInt(defaultPercent);
}

void Library::saveMuted(bool muted)
{
    QJsonObject audio = m_data.value(QStringLiteral("audio")).toObject();
    audio[QStringLiteral("muted")] = muted;
    m_data[QStringLiteral("audio")] = audio;
    saveToFile();
}

bool Library::loadMuted(bool defaultMuted) const
{
    return m_data.value(QStringLiteral("audio")).toObject()
           .value(QStringLiteral("muted")).toBool(defaultMuted);
}

// ════════════════════════════════════════════════════════════
//  播放模式
// ════════════════════════════════════════════════════════════

void Library::saveRepeatMode(int mode)
{
    QJsonObject pb = m_data.value(QStringLiteral("playback")).toObject();
    pb[QStringLiteral("repeatMode")] = mode;
    m_data[QStringLiteral("playback")] = pb;
    saveToFile();
}

int Library::loadRepeatMode(int defaultMode) const
{
    return m_data.value(QStringLiteral("playback")).toObject()
           .value(QStringLiteral("repeatMode")).toInt(defaultMode);
}

void Library::saveShuffle(bool enabled)
{
    QJsonObject pb = m_data.value(QStringLiteral("playback")).toObject();
    pb[QStringLiteral("shuffle")] = enabled;
    m_data[QStringLiteral("playback")] = pb;
    saveToFile();
}

bool Library::loadShuffle(bool defaultShuffle) const
{
    return m_data.value(QStringLiteral("playback")).toObject()
           .value(QStringLiteral("shuffle")).toBool(defaultShuffle);
}

// ════════════════════════════════════════════════════════════
//  上次播放的歌曲
// ════════════════════════════════════════════════════════════

void Library::saveLastPlayedSong(const QString &songPath)
{
    QJsonObject pb = m_data.value(QStringLiteral("playback")).toObject();
    pb[QStringLiteral("lastPlayedSong")] = songPath;
    m_data[QStringLiteral("playback")] = pb;
    saveToFile();
}

QString Library::loadLastPlayedSong() const
{
    return m_data.value(QStringLiteral("playback")).toObject()
           .value(QStringLiteral("lastPlayedSong")).toString();
}

// ════════════════════════════════════════════════════════════
//  上次播放位置
// ════════════════════════════════════════════════════════════

void Library::saveLastPosition(const QString &songPath, qint64 positionMs)
{
    QJsonObject pos = m_data.value(QStringLiteral("positions")).toObject();

    // 限制条目数量
    if (pos.size() >= kMaxRecentPositions)
        pos = QJsonObject();

    pos[songPath] = static_cast<qint64>(positionMs);
    m_data[QStringLiteral("positions")] = pos;
    saveToFile();
}

qint64 Library::loadLastPosition(const QString &songPath) const
{
    return static_cast<qint64>(
        m_data.value(QStringLiteral("positions")).toObject()
        .value(songPath).toDouble(0));
}

// ════════════════════════════════════════════════════════════
//  歌词时间微调偏移
// ════════════════════════════════════════════════════════════

void Library::saveLyricsOffset(const QString &songPath, qint64 offsetMs)
{
    QJsonObject off = m_data.value(QStringLiteral("lyricsOffsets")).toObject();
    off[songPath] = static_cast<qint64>(offsetMs);
    m_data[QStringLiteral("lyricsOffsets")] = off;
    saveToFile();
}

qint64 Library::loadLyricsOffset(const QString &songPath, qint64 defaultOffset) const
{
    return static_cast<qint64>(
        m_data.value(QStringLiteral("lyricsOffsets")).toObject()
        .value(songPath).toDouble(static_cast<double>(defaultOffset)));
}

// ════════════════════════════════════════════════════════════
//  主题
// ════════════════════════════════════════════════════════════

void Library::saveTheme(int theme)
{
    QJsonObject app = m_data.value(QStringLiteral("app")).toObject();
    app[QStringLiteral("theme")] = theme;
    m_data[QStringLiteral("app")] = app;
    saveToFile();
}

int Library::loadTheme(int defaultTheme) const
{
    return m_data.value(QStringLiteral("app")).toObject()
           .value(QStringLiteral("theme")).toInt(defaultTheme);
}

// ════════════════════════════════════════════════════════════
//  DIY 背景
// ════════════════════════════════════════════════════════════

void Library::saveDiyBgFolder(const QString &folderPath)
{
    QJsonObject app = m_data.value(QStringLiteral("app")).toObject();
    app[QStringLiteral("diyBgFolder")] = folderPath;
    m_data[QStringLiteral("app")] = app;
    saveToFile();
}

QString Library::loadDiyBgFolder() const
{
    return m_data.value(QStringLiteral("app")).toObject()
           .value(QStringLiteral("diyBgFolder")).toString();
}

// ════════════════════════════════════════════════════════════
//  窗口几何
// ════════════════════════════════════════════════════════════

void Library::saveWindowGeometry(const QByteArray &geometry)
{
    // Base64 编码存储二进制几何数据
    m_data[QStringLiteral("windowGeometry")] =
        QString::fromLatin1(geometry.toBase64());
    saveToFile();
}

QByteArray Library::loadWindowGeometry() const
{
    return QByteArray::fromBase64(
        m_data.value(QStringLiteral("windowGeometry")).toString().toLatin1());
}

// ════════════════════════════════════════════════════════════
//  歌单数据（PlaylistManager）
// ════════════════════════════════════════════════════════════

void Library::savePlaylists(const QJsonArray &data)
{
    m_data[QStringLiteral("playlists")] = data;
    saveToFile();
}

QJsonArray Library::loadPlaylists() const
{
    return m_data.value(QStringLiteral("playlists")).toArray();
}

// ════════════════════════════════════════════════════════════
//  文件夹扫描缓存
// ════════════════════════════════════════════════════════════

void Library::cacheFolderSongs(const QString &folderPath, const QList<Song> &songs)
{
    QJsonObject folderCache = m_data.value(QStringLiteral("folderCache")).toObject();
    QJsonArray arr;
    for (const auto &s : songs)
        arr.append(songToJson(s));
    folderCache[folderPath] = arr;
    m_data[QStringLiteral("folderCache")] = folderCache;
    saveToFile();
}

QList<Song> Library::loadCachedFolderSongs(const QString &folderPath) const
{
    QList<Song> result;
    const QJsonObject folderCache = m_data.value(QStringLiteral("folderCache")).toObject();
    const QJsonArray arr = folderCache.value(folderPath).toArray();
    for (const auto &v : arr) {
        Song s = songFromJson(v.toObject());
        if (QFileInfo::exists(s.filePath))
            result.append(s);
    }
    return result;
}

bool Library::hasFolderCache(const QString &folderPath) const
{
    return m_data.value(QStringLiteral("folderCache")).toObject()
           .contains(folderPath);
}

void Library::clearFolderCache()
{
    m_data.remove(QStringLiteral("folderCache"));
    saveToFile();
}

void Library::clearFolderCacheFor(const QString &folderPath)
{
    QJsonObject folderCache = m_data.value(QStringLiteral("folderCache")).toObject();
    folderCache.remove(folderPath);
    m_data[QStringLiteral("folderCache")] = folderCache;
    saveToFile();
}

// ════════════════════════════════════════════════════════════
//  工具
// ════════════════════════════════════════════════════════════

void Library::clearAll()
{
    m_data = QJsonObject();
    saveToFile();
}
