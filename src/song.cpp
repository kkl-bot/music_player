#include "song.h"
#include "formatconverter.h"
#include <QFileInfo>
#include <QDir>
#include <QPixmap>

// ════════════════════════════════════════════════════════════
//  构造 — 自动探测元数据
// ════════════════════════════════════════════════════════════

Song::Song(const QString &filePath)
    : filePath(filePath)
{
    const QFileInfo fi(filePath);
    title   = fi.completeBaseName();
    artist  = {};
    album   = {};
    lyricsPath = detectLyricsFile(filePath);
    coverPath  = detectCoverFile(filePath);

    // 用 ffprobe 读取内嵌元数据覆盖默认值
    const auto meta = FormatConverter::probeAll(filePath);
    if (!meta.title.isEmpty())
        title = meta.title;
    if (!meta.artist.isEmpty())
        artist = meta.artist;
    if (!meta.album.isEmpty())
        album = meta.album;
}

// ════════════════════════════════════════════════════════════
//  便捷访问
// ════════════════════════════════════════════════════════════

QUrl Song::url() const
{
    return QUrl::fromLocalFile(filePath);
}

bool Song::hasLyrics() const
{
    return !lyricsPath.isEmpty() && QFileInfo::exists(lyricsPath);
}

bool Song::hasCoverFile() const
{
    return !coverPath.isEmpty() && QFileInfo::exists(coverPath);
}

QPixmap Song::loadCoverFile() const
{
    if (!hasCoverFile())
        return {};
    QPixmap pix(coverPath);
    // 限制最大尺寸防止内存占用过大
    if (pix.width() > 512 || pix.height() > 512)
        pix = pix.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return pix;
}

QString Song::displayTitle() const
{
    if (!title.isEmpty())
        return title;
    if (!filePath.isEmpty())
        return QFileInfo(filePath).completeBaseName();
    return QStringLiteral("未知曲目");
}

QString Song::displayArtist() const
{
    return artist.isEmpty() ? QStringLiteral("未知艺术家") : artist;
}

QString Song::durationString() const
{
    if (durationMs <= 0)
        return QStringLiteral("--:--");

    const int secs = static_cast<int>(durationMs / 1000);
    return QStringLiteral("%1:%2")
        .arg(secs / 60, 2, 10, QLatin1Char('0'))
        .arg(secs % 60, 2, 10, QLatin1Char('0'));
}

bool Song::isValid() const
{
    return !filePath.isEmpty() && QFileInfo::exists(filePath);
}

// ════════════════════════════════════════════════════════════
//  序列化
// ════════════════════════════════════════════════════════════

QString Song::toString() const
{
    return QStringLiteral("Song(%1 | %2 | %3)")
        .arg(displayTitle(), displayArtist(), durationString());
}

Song Song::fromFileInfo(const QFileInfo &fi)
{
    return Song(fi.absoluteFilePath());
}

// ════════════════════════════════════════════════════════════
//  内部 — 自动探测歌词文件
// ════════════════════════════════════════════════════════════

QString Song::detectLyricsFile(const QString &audioPath)
{
    const QFileInfo fi(audioPath);
    const QDir dir = fi.absoluteDir();
    const QString baseName = fi.completeBaseName();

    static const QStringList kLyricExtensions { "lrc", "txt", "srt" };
    for (const auto &ext : kLyricExtensions) {
        const QString candidate = dir.absoluteFilePath(baseName + '.' + ext);
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return {};
}

// ════════════════════════════════════════════════════════════
//  内部 — 自动探测封面图片
// ════════════════════════════════════════════════════════════

QString Song::detectCoverFile(const QString &audioPath)
{
    const QFileInfo fi(audioPath);
    const QDir dir = fi.absoluteDir();
    const QString baseName = fi.completeBaseName();

    // 常见封面文件名（优先同名，再试通用名）
    static const QStringList kCoverCandidates = {
        // 同名图片
        baseName + ".jpg",  baseName + ".png",
        baseName + ".jpeg", baseName + ".webp",
        baseName + ".bmp",
        // 通用封面名
        "cover.jpg",   "cover.png",
        "Cover.jpg",   "Cover.png",
        "folder.jpg",  "folder.png",
        "Folder.jpg",  "Folder.png",
        "album.jpg",   "album.png",
        "Album.jpg",   "Album.png",
        "front.jpg",   "front.png",
    };

    for (const auto &name : kCoverCandidates) {
        const QString candidate = dir.absoluteFilePath(name);
        if (QFileInfo::exists(candidate))
            return candidate;
    }

    return {};
}

// ════════════════════════════════════════════════════════════
//  运算符重载
// ════════════════════════════════════════════════════════════

bool operator==(const Song &a, const Song &b)
{
    return a.filePath == b.filePath;
}

bool operator!=(const Song &a, const Song &b)
{
    return !(a == b);
}
