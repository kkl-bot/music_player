#include "formatconverter.h"

#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QCoreApplication>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

FormatConverter::FormatConverter(QObject *parent)
    : QObject(parent)
{
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardError,
            this, &FormatConverter::onReadyReadStderr);
    // QProcess::MergedChannels 会把 stderr 合并到 stdout，
    // 我们用 readyReadStandardOutput 读取全部输出
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &FormatConverter::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &FormatConverter::onProcessFinished);
}

FormatConverter::~FormatConverter()
{
    cancel();
}

// ════════════════════════════════════════════════════════════
//  公共接口
// ════════════════════════════════════════════════════════════

void FormatConverter::convert(const QString &inputFile, Format targetFormat)
{
    if (isRunning()) {
        emit conversionError(QStringLiteral("已有转码任务正在进行中"));
        return;
    }

    m_inputFile = inputFile;
    m_targetFormat = targetFormat;

    const QFileInfo fi(inputFile);
    if (!fi.exists()) {
        emit conversionError(QStringLiteral("文件不存在: ") + inputFile);
        return;
    }

    // 构造输出路径：同目录下，文件名_格式.后缀
    const QString baseName = fi.completeBaseName();
    const QString outDir   = fi.absolutePath();
    const QString suffix   = formatSuffix(targetFormat);
    m_outputFile = outDir + QStringLiteral("/") + baseName + suffix;

    // 先探测时长
    m_totalDurationMs = probeDuration(inputFile);
    if (m_totalDurationMs <= 0) {
        qDebug() << "[FormatConverter] 无法获取音频时长，进度将不可用";
    }

    // 开始转码
    const QStringList args = buildArguments(inputFile, m_outputFile, targetFormat);
    qDebug() << "[FormatConverter] ffmpeg args:" << args;

    m_process->start(ffmpegPath(), args);
}

void FormatConverter::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool FormatConverter::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

// ════════════════════════════════════════════════════════════
//  格式辅助
// ════════════════════════════════════════════════════════════

QString FormatConverter::formatSuffix(Format fmt)
{
    switch (fmt) {
    case Format::MP3:  return QStringLiteral(".mp3");
    case Format::FLAC: return QStringLiteral(".flac");
    case Format::OGG:  return QStringLiteral(".ogg");
    case Format::WAV:  return QStringLiteral(".wav");
    case Format::AAC:  return QStringLiteral(".m4a");
    case Format::OPUS: return QStringLiteral(".opus");
    case Format::WMA:  return QStringLiteral(".wma");
    }
    return QStringLiteral(".mp3");
}

QString FormatConverter::formatDisplayName(Format fmt)
{
    switch (fmt) {
    case Format::MP3:  return QStringLiteral("MP3 (.mp3)");
    case Format::FLAC: return QStringLiteral("FLAC (.flac)");
    case Format::OGG:  return QStringLiteral("OGG (.ogg)");
    case Format::WAV:  return QStringLiteral("WAV (.wav)");
    case Format::AAC:  return QStringLiteral("AAC (.m4a)");
    case Format::OPUS: return QStringLiteral("OPUS (.opus)");
    case Format::WMA:  return QStringLiteral("WMA (.wma)");
    }
    return QStringLiteral("MP3");
}

QStringList FormatConverter::formatDisplayNames()
{
    return {
        formatDisplayName(Format::MP3),
        formatDisplayName(Format::FLAC),
        formatDisplayName(Format::OGG),
        formatDisplayName(Format::WAV),
        formatDisplayName(Format::AAC),
        formatDisplayName(Format::OPUS),
        formatDisplayName(Format::WMA),
    };
}

FormatConverter::Format FormatConverter::formatFromDisplayName(const QString &displayName)
{
    // 用后缀匹配
    if (displayName.contains(QStringLiteral(".mp3")))  return Format::MP3;
    if (displayName.contains(QStringLiteral(".flac"))) return Format::FLAC;
    if (displayName.contains(QStringLiteral(".ogg")))  return Format::OGG;
    if (displayName.contains(QStringLiteral(".wav")))  return Format::WAV;
    if (displayName.contains(QStringLiteral(".m4a")))  return Format::AAC;
    if (displayName.contains(QStringLiteral(".opus"))) return Format::OPUS;
    if (displayName.contains(QStringLiteral(".wma")))  return Format::WMA;
    return Format::MP3;
}

// ════════════════════════════════════════════════════════════
//  内部槽
// ════════════════════════════════════════════════════════════

void FormatConverter::onReadyReadStderr()
{
    // 读取所有可用的输出
    const QByteArray data = m_process->readAll();
    const QString text = QString::fromUtf8(data);

    // 解析 stderr 中的 time= 行来获取进度
    // 示例行: "size=   12345kB time=00:01:23.45 bitrate=..."
    const QStringList lines = text.split(QStringLiteral("\n"));
    for (const QString &line : lines) {
        if (!line.contains(QStringLiteral("time=")))
            continue;

        const qint64 currentMs = parseFfmpegTime(line);
        if (currentMs > 0 && m_totalDurationMs > 0) {
            int percent = static_cast<int>(currentMs * 100 / m_totalDurationMs);
            if (percent > 100) percent = 100;
            emit progressChanged(percent);
        }
    }
}

void FormatConverter::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::CrashExit || exitCode != 0) {
        const QString err = QString::fromUtf8(m_process->readAll());
        emit conversionError(
            QStringLiteral("ffmpeg 退出码: %1\n%2").arg(exitCode).arg(err));
        return;
    }

    emit progressChanged(100);
    emit conversionFinished(true, m_outputFile);
}

// ════════════════════════════════════════════════════════════
//  私有辅助
// ════════════════════════════════════════════════════════════

qint64 FormatConverter::probeDuration(const QString &filePath) const
{
    // 用 ffprobe 快速获取时长
    QProcess probe;
    probe.start(ffprobePath(), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        filePath
    });

    if (!probe.waitForFinished(10000))
        return 0;

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    bool ok = false;
    const double seconds = output.toDouble(&ok);
    if (ok && seconds > 0)
        return static_cast<qint64>(seconds * 1000.0);

    return 0;
}

qint64 FormatConverter::parseFfmpegTime(const QString &line)
{
    // 匹配 time=HH:MM:SS.mm 或 time=HH:MM:SS.mmm
    static const QRegularExpression regex(QStringLiteral(
        R"(time=(\d{2}):(\d{2}):(\d{2})\.(\d{2,3}))"));

    const auto match = regex.match(line);
    if (!match.hasMatch())
        return -1;

    const int hh = match.captured(1).toInt();
    const int mm = match.captured(2).toInt();
    const int ss = match.captured(3).toInt();
    const int ms = match.captured(4).toInt() * (match.captured(4).length() == 2 ? 10 : 1);

    return static_cast<qint64>(hh) * 3600000
         + static_cast<qint64>(mm) * 60000
         + static_cast<qint64>(ss) * 1000
         + ms;
}

QString FormatConverter::probeArtist(const QString &filePath)
{
    QProcess probe;
    probe.start(ffprobePath(), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-show_entries"), QStringLiteral("format_tags=artist"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        filePath
    });

    if (!probe.waitForFinished(10000))
        return {};

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    return output;
}

QString FormatConverter::probeAlbum(const QString &filePath)
{
    QProcess probe;
    probe.start(ffprobePath(), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-show_entries"), QStringLiteral("format_tags=album"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        filePath
    });

    if (!probe.waitForFinished(10000))
        return {};

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    return output;
}

QString FormatConverter::probeTitle(const QString &filePath)
{
    QProcess probe;
    probe.start(ffprobePath(), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-show_entries"), QStringLiteral("format_tags=title"),
        QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
        filePath
    });

    if (!probe.waitForFinished(10000))
        return {};

    const QString output = QString::fromUtf8(probe.readAllStandardOutput()).trimmed();
    return output;
}

FormatConverter::ProbeResult FormatConverter::probeAll(const QString &filePath)
{
    ProbeResult result;

    // ffprobe 不存在时直接返回空（由 Player 信号回填）
    if (!QFileInfo::exists(ffprobePath()))
        return result;

    // 使用 JSON 输出同时探测 format 和 stream 级别的标签
    QProcess probe;
    probe.start(ffprobePath(), {
        QStringLiteral("-v"), QStringLiteral("quiet"),
        QStringLiteral("-show_format"),
        QStringLiteral("-show_streams"),
        QStringLiteral("-of"), QStringLiteral("json"),
        filePath
    });

    if (!probe.waitForFinished(15000))
        return result;

    const QByteArray raw = probe.readAllStandardOutput();
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isObject())
        return result;

    const QJsonObject root = doc.object();

    // ── 辅助：从 tags 对象中读取字段 ──
    auto readTag = [](const QJsonObject &tags, const QString &key) -> QString {
        return tags.value(key).toString().trimmed();
    };

    // ── 从 format 读取容器级标签 ──
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();
    const QJsonObject formatTags = format.value(QStringLiteral("tags")).toObject();

    // ── 从第一个音频流读取流级标签（优先） ──
    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    QJsonObject streamTags;
    for (const auto &sv : streams) {
        const QJsonObject s = sv.toObject();
        if (s.value(QStringLiteral("codec_type")).toString() == QStringLiteral("audio")) {
            streamTags = s.value(QStringLiteral("tags")).toObject();
            break;
        }
    }

    // 优先使用 stream 标签，回退到 format 标签
    result.artist = readTag(streamTags, QStringLiteral("artist"));
    if (result.artist.isEmpty())
        result.artist = readTag(streamTags, QStringLiteral("TPE1"));        // ID3v2
    if (result.artist.isEmpty())
        result.artist = readTag(streamTags, QStringLiteral("\251ART"));     // MP4
    if (result.artist.isEmpty())
        result.artist = readTag(formatTags, QStringLiteral("artist"));

    result.album = readTag(streamTags, QStringLiteral("album"));
    if (result.album.isEmpty())
        result.album = readTag(streamTags, QStringLiteral("TALB"));        // ID3v2
    if (result.album.isEmpty())
        result.album = readTag(streamTags, QStringLiteral("\251alb"));     // MP4
    if (result.album.isEmpty())
        result.album = readTag(formatTags, QStringLiteral("album"));

    result.title = readTag(streamTags, QStringLiteral("title"));
    if (result.title.isEmpty())
        result.title = readTag(streamTags, QStringLiteral("TIT2"));        // ID3v2
    if (result.title.isEmpty())
        result.title = readTag(streamTags, QStringLiteral("\251nam"));     // MP4
    if (result.title.isEmpty())
        result.title = readTag(formatTags, QStringLiteral("title"));

    return result;
}

QString FormatConverter::ffmpegPath()
{
#ifdef FFMPEG_BIN_DIR
    return QDir::cleanPath(QStringLiteral(FFMPEG_BIN_DIR) + QStringLiteral("/ffmpeg.exe"));
#else
    return QStringLiteral("ffmpeg");
#endif
}

QString FormatConverter::ffprobePath()
{
#ifdef FFMPEG_BIN_DIR
    return QDir::cleanPath(QStringLiteral(FFMPEG_BIN_DIR) + QStringLiteral("/ffprobe.exe"));
#else
    return QStringLiteral("ffprobe");
#endif
}

QStringList FormatConverter::buildArguments(const QString &inputFile,
                                            const QString &outputFile,
                                            Format fmt) const
{
    QStringList args;
    args << QStringLiteral("-y");                    // 覆盖输出文件
    args << QStringLiteral("-i");  args << inputFile;

    // 编码参数
    switch (fmt) {
    case Format::MP3:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("libmp3lame");
        args << QStringLiteral("-qscale:a"); args << QStringLiteral("2");
        break;
    case Format::FLAC:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("flac");
        break;
    case Format::OGG:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("libvorbis");
        args << QStringLiteral("-qscale:a"); args << QStringLiteral("5");
        break;
    case Format::WAV:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("pcm_s16le");
        break;
    case Format::AAC:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("aac");
        args << QStringLiteral("-b:a"); args << QStringLiteral("256k");
        break;
    case Format::OPUS:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("libopus");
        args << QStringLiteral("-b:a"); args << QStringLiteral("128k");
        break;
    case Format::WMA:
        args << QStringLiteral("-codec:a"); args << QStringLiteral("wmav2");
        args << QStringLiteral("-b:a"); args << QStringLiteral("192k");
        break;
    }

    // 输出文件
    args << outputFile;

    return args;
}
