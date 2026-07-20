#ifndef FORMATCONVERTER_H
#define FORMATCONVERTER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QDir>

/// FormatConverter - 利用 QProcess + ffmpeg 实现音频格式转码
///
/// 功能：
///   1. 异步调用 ffmpeg 进行转码
///   2. 解析 ffmpeg 输出获取转换进度
///   3. 支持多种常见输出格式
class FormatConverter : public QObject
{
    Q_OBJECT

public:
    /// 支持的输出格式
    enum class Format {
        MP3,    // .mp3  MPEG Audio Layer III
        FLAC,   // .flac Free Lossless Audio Codec
        OGG,    // .ogg  Ogg Vorbis
        WAV,    // .wav  PCM Wave
        AAC,    // .m4a  Advanced Audio Coding
        OPUS,   // .opus Opus Audio
        WMA,    // .wma  Windows Media Audio
    };
    Q_ENUM(Format)

    explicit FormatConverter(QObject *parent = nullptr);
    ~FormatConverter() override;

    /// 开始转码（异步）。如果已在转换中则忽略。
    void convert(const QString &inputFile, Format targetFormat);

    /// 取消当前转码
    void cancel();

    /// 是否正在转码
    bool isRunning() const;

    // ── 格式辅助 ──

    /// 返回格式对应的文件后缀（含点），如 ".mp3"
    static QString formatSuffix(Format fmt);

    /// 返回格式的显示名称，如 "MP3 (.mp3)"
    static QString formatDisplayName(Format fmt);

    /// 返回所有格式的显示名称列表（用于下拉框）
    static QStringList formatDisplayNames();

    /// 从显示名称解析格式
    static Format formatFromDisplayName(const QString &displayName);

    // ── ffmpeg/ffprobe 路径 ──

    /// 返回 ffmpeg.exe 完整路径
    static QString ffmpegPath();
    /// 返回 ffprobe.exe 完整路径
    static QString ffprobePath();

    /// 用 ffprobe 读取音频文件的艺术家标签
    static QString probeArtist(const QString &filePath);

signals:
    /// 转换进度 (0~100)
    void progressChanged(int percent);

    /// 转换完成
    void conversionFinished(bool success, const QString &outputPath);

    /// 转换出错
    void conversionError(const QString &errorMessage);

private slots:
    void onReadyReadStderr();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    /// 用 ffprobe 获取音频时长（毫秒）
    qint64 probeDuration(const QString &filePath) const;

    /// 解析 ffmpeg stderr 中的 time=HH:MM:SS.mm 为毫秒
    static qint64 parseFfmpegTime(const QString &line);

    /// 构造 ffmpeg 参数列表
    QStringList buildArguments(const QString &inputFile,
                               const QString &outputFile,
                               Format fmt) const;

    QProcess *m_process     = nullptr;
    QString   m_inputFile;
    QString   m_outputFile;
    qint64    m_totalDurationMs = 0;
    Format    m_targetFormat = Format::MP3;
};

#endif // FORMATCONVERTER_H
