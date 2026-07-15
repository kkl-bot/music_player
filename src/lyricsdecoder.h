#ifndef LYRICSDECODER_H
#define LYRICSDECODER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QTime>

/// LyricsDecoder - LRC 歌词解析器
///
/// 支持标准 LRC 格式：
///   [mm:ss.xx]歌词文本
///   [mm:ss]歌词文本
///   [ti:标题] [ar:艺人] [al:专辑] [offset:±ms]
class LyricsDecoder : public QObject
{
    Q_OBJECT

public:
    /// 单行歌词条目
    struct LyricLine {
        qint64  startMs = 0;   // 开始时间（毫秒）
        qint64  endMs   = 0;   // 结束时间（下一句的开始）
        QString text;           // 歌词文本
    };

    explicit LyricsDecoder(QObject *parent = nullptr);
    ~LyricsDecoder() override;

    // ── 加载 ──
    /// 从文件加载 LRC 歌词
    /// @return true 如果解析成功（至少有一句歌词）
    bool load(const QString &filePath);

    /// 从字符串加载 LRC 歌词
    /// @return true 如果解析成功
    bool loadFromData(const QString &lrcData);

    /// 清空所有歌词
    void clear();

    // ── 查询 ──
    /// 获取指定播放位置对应的歌词文本
    QString getSubtitleAt(qint64 positionMs) const;

    /// 获取指定位置对应的歌词行号（-1 表示无匹配）
    int lineIndexAt(qint64 positionMs) const;

    /// 获取指定行的歌词
    LyricLine lineAt(int index) const;

    /// 歌词总行数
    int lineCount() const;

    /// 是否已加载
    bool isLoaded() const;

    // ── 元数据 ──
    QString title()  const { return m_title; }
    QString artist() const { return m_artist; }
    QString album()  const { return m_album; }

    /// LRC 头部声明的全局时间偏移（毫秒），正数=延后，负数=提前
    qint64 offset() const { return m_offsetMs; }

    /// 用户手动微调偏移（毫秒），叠加在 offset() 之上
    qint64 userOffset() const { return m_userOffsetMs; }
    void setUserOffset(qint64 ms);
    void adjustUserOffset(qint64 deltaMs);

    /// 总偏移 = offset() + userOffset()
    qint64 totalOffset() const { return m_offsetMs + m_userOffsetMs; }

signals:
    void subtitleChanged(const QString &text);
    void lyricLineChanged(int lineIndex);

private:
    /// 解析一行 LRC 文本
    void parseLine(const QString &line, int lineNumber);

    /// 解析时间标签 [mm:ss.xx]
    bool parseTimestamp(const QString &tag, qint64 &outMs) const;

    /// 解析后的所有歌词行（按时间排序）
    QList<LyricLine> m_lyrics;

    // ── 元数据 ──
    QString m_title;
    QString m_artist;
    QString m_album;
    qint64  m_offsetMs     = 0;   // 来自 [offset:±ms]
    qint64  m_userOffsetMs = 0;   // 用户手动微调
};

#endif // LYRICSDECODER_H
