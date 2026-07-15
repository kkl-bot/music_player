#include "lyricsdecoder.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

LyricsDecoder::LyricsDecoder(QObject *parent)
    : QObject(parent)
{
}

LyricsDecoder::~LyricsDecoder() = default;

// ════════════════════════════════════════════════════════════
//  加载
// ════════════════════════════════════════════════════════════

bool LyricsDecoder::load(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    // Qt6: QTextStream 默认使用 UTF-8
    // Qt5: 需要显式设置 codec
    QTextStream stream(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    stream.setCodec("UTF-8");
#endif

    const QString content = stream.readAll();
    file.close();

    return loadFromData(content);
}

bool LyricsDecoder::loadFromData(const QString &lrcData)
{
    clear();

    const QStringList lines = lrcData.split(QRegularExpression("[\r\n]+"),
                                            Qt::SkipEmptyParts);

    for (int i = 0; i < lines.size(); ++i)
        parseLine(lines.at(i), i);

    // 按时间排序
    std::sort(m_lyrics.begin(), m_lyrics.end(),
              [](const LyricLine &a, const LyricLine &b) {
                  return a.startMs < b.startMs;
              });

    // 补全每条歌词的 endMs（下一条的开始时间）
    for (int i = 0; i < m_lyrics.size() - 1; ++i)
        m_lyrics[i].endMs = m_lyrics[i + 1].startMs;

    // 最后一条的 endMs 设为 startMs + 5 秒
    if (!m_lyrics.isEmpty())
        m_lyrics.last().endMs = m_lyrics.last().startMs + 5000;

    return !m_lyrics.isEmpty();
}

void LyricsDecoder::clear()
{
    m_lyrics.clear();
    m_title.clear();
    m_artist.clear();
    m_album.clear();
    m_offsetMs     = 0;
    m_userOffsetMs = 0;
}

// ════════════════════════════════════════════════════════════
//  查询
// ════════════════════════════════════════════════════════════

QString LyricsDecoder::getSubtitleAt(qint64 positionMs) const
{
    const int idx = lineIndexAt(positionMs);
    return (idx >= 0) ? m_lyrics.at(idx).text : QString();
}

void LyricsDecoder::setUserOffset(qint64 ms)
{
    if (m_userOffsetMs != ms) {
        m_userOffsetMs = ms;
    }
}

void LyricsDecoder::adjustUserOffset(qint64 deltaMs)
{
    m_userOffsetMs += deltaMs;
}

int LyricsDecoder::lineIndexAt(qint64 positionMs) const
{
    if (m_lyrics.isEmpty())
        return -1;

    const qint64 adjusted = positionMs - (m_offsetMs + m_userOffsetMs);

    // 二分查找
    int lo = 0, hi = m_lyrics.size() - 1;
    while (lo <= hi) {
        const int mid = lo + (hi - lo) / 2;
        const auto &line = m_lyrics.at(mid);

        if (adjusted >= line.startMs && adjusted < line.endMs)
            return mid;
        if (adjusted < line.startMs)
            hi = mid - 1;
        else
            lo = mid + 1;
    }
    return -1;
}

LyricsDecoder::LyricLine LyricsDecoder::lineAt(int index) const
{
    return m_lyrics.value(index);
}

int LyricsDecoder::lineCount() const
{
    return m_lyrics.size();
}

bool LyricsDecoder::isLoaded() const
{
    return !m_lyrics.isEmpty();
}

// ════════════════════════════════════════════════════════════
//  内部 — 解析一行 LRC
// ════════════════════════════════════════════════════════════

void LyricsDecoder::parseLine(const QString &line, int /*lineNumber*/)
{
    if (line.trimmed().isEmpty())
        return;

    // ── 元数据标签: [ti:xxx] [ar:xxx] [al:xxx] [offset:±ms] ──
    static const QRegularExpression kMeta(R"(\[(\w+):\s*(.*?)\])");
    auto metaIt = kMeta.globalMatch(line);
    while (metaIt.hasNext()) {
        const auto match = metaIt.next();
        const QString key = match.captured(1).toLower();
        const QString val = match.captured(2).trimmed();

        if (key == "ti")  m_title  = val;
        if (key == "ar")  m_artist = val;
        if (key == "al")  m_album  = val;
        if (key == "offset") {
            bool ok = false;
            qint64 off = val.toLongLong(&ok);
            if (ok) m_offsetMs = off;
        }
    }

    // ── 时间标签 + 歌词文本: [mm:ss.xx]歌词 ──
    static const QRegularExpression kTimeTag(R"((\[\d{1,3}:\d{2}(?:\.\d{1,3})?\]))");
    QList<qint64> timestamps;
    int lastTagEnd = -1;

    auto timeIt = kTimeTag.globalMatch(line);
    while (timeIt.hasNext()) {
        const auto match = timeIt.next();
        qint64 ms = 0;
        if (parseTimestamp(match.captured(1), ms)) {
            timestamps.append(ms);
            lastTagEnd = match.capturedEnd();
        }
    }

    if (timestamps.isEmpty())
        return;  // 没有时间标签，跳过（纯元数据行已被上面处理）

    // 提取所有时间标签后面的歌词文本
    const QString text = (lastTagEnd > 0)
        ? line.mid(lastTagEnd).trimmed()
        : QString();

    for (qint64 ts : timestamps) {
        LyricLine entry;
        entry.startMs = ts;
        entry.text    = text.isEmpty() ? QStringLiteral("♪") : text;
        m_lyrics.append(entry);
    }
}

bool LyricsDecoder::parseTimestamp(const QString &tag, qint64 &outMs) const
{
    // 支持格式: [mm:ss] 或 [mm:ss.xx] 或 [mm:ss.xxx]
    static const QRegularExpression kPattern(R"((\d{1,3}):(\d{2})(?:\.(\d{1,3}))?)");
    const auto match = kPattern.match(tag);
    if (!match.hasMatch())
        return false;

    const int minutes = match.captured(1).toInt();
    const int seconds = match.captured(2).toInt();
    int milliseconds = 0;

    if (!match.captured(3).isEmpty()) {
        QString frac = match.captured(3);
        // 补齐或截断到 3 位
        while (frac.length() < 3) frac += '0';
        if (frac.length() > 3) frac = frac.left(3);
        milliseconds = frac.toInt();
    }

    outMs = minutes * 60000LL + seconds * 1000LL + milliseconds;
    return true;
}
