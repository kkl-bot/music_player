#include "song.h"
#include <QFileInfo>
#include <QDir>

// ════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════

Song::Song(const QString &filePath)
    : filePath(filePath)
{
    const QFileInfo fi(filePath);
    title   = fi.completeBaseName();
    artist  = {};
    album   = {};
    lyricsPath = detectLyricsFile(filePath);
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

QString Song::displayTitle() const
{
    if (!title.isEmpty())
        return title;
    if (!filePath.isEmpty())
        return QFileInfo(filePath).completeBaseName();
    return QStringLiteral("未知曲目");
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
        .arg(displayTitle(), artist, durationString());
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

    // 优先尝试同名的 .lrc
    static const QStringList kLyricExtensions { "lrc", "txt", "srt" };
    for (const auto &ext : kLyricExtensions) {
        const QString candidate = dir.absoluteFilePath(baseName + '.' + ext);
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
