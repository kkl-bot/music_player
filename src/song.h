#ifndef SONG_H
#define SONG_H

#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QTime>

#include <QPixmap>

/// Song - 单首歌曲的完整数据
///
/// 记录歌曲的路径、标题、艺术家、时长等元信息，
/// 并关联对应的歌词文件路径和封面图片。
class Song
{
public:
    Song() = default;

    explicit Song(const QString &filePath);

    // ── 基本元信息 ──
    QString title       = {};   // 标题
    QString artist      = {};   // 艺术家
    QString album       = {};   // 专辑
    QString filePath    = {};   // 音频文件绝对路径
    QString lyricsPath  = {};   // 关联的 .lrc 歌词文件（自动探测）
    QString coverPath   = {};   // 关联的封面图片文件（自动探测）
    qint64  durationMs  = 0;    // 时长（毫秒），0 表示未知

    // ── 便捷访问 ──
    QUrl    url() const;                        // 可用于 QMediaPlayer 的 URL
    bool    hasLyrics() const;                  // 是否有歌词文件
    bool    hasCoverFile() const;               // 是否有本地封面文件
    QPixmap loadCoverFile() const;              // 加载本地封面文件
    QString displayTitle() const;               // 显示用标题
    QString displayArtist() const;              // 显示用艺术家
    QString durationString() const;             // "03:45" 格式的时长字符串
    bool    isValid() const;                    // filePath 非空且文件存在

    // ── 序列化 ──
    QString toString() const;                   // 调试用
    static Song fromFileInfo(const QFileInfo &fi);

private:
    static QString detectLyricsFile(const QString &audioPath);
    static QString detectCoverFile(const QString &audioPath);
};

// 比较 / 排序
bool operator==(const Song &a, const Song &b);
bool operator!=(const Song &a, const Song &b);

#endif // SONG_H
