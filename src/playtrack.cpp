#include "playtrack.h"

#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QDirIterator>
#include <QRegularExpression>
#include <algorithm>

// ════════════════════════════════════════════════════════════
//  支持的音频格式
// ════════════════════════════════════════════════════════════

static const QStringList kAudioSuffixes {
    "mp3",  "wav",  "flac", "ogg",
    "aac",  "m4a",  "wma",  "opus",
    "ape",  "aiff", "au",
};

// ════════════════════════════════════════════════════════════
//  静态成员定义
// ════════════════════════════════════════════════════════════

QMap<QString, QList<Song>> PlayTrack::s_cache;

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

PlayTrack::PlayTrack(QObject *parent)
    : QObject(parent)
{
}

PlayTrack::~PlayTrack() = default;

// ════════════════════════════════════════════════════════════
//  文件夹浏览与加载
// ════════════════════════════════════════════════════════════

bool PlayTrack::browseAndLoad()
{
    // 弹出系统文件夹选择对话框
    const QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("选择音乐文件夹"),
        m_currentFolder.isEmpty() ? QString() : m_currentFolder,
        QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);

    if (dir.isEmpty())
        return false;   // 用户取消了选择

    return loadFromFolder(dir);
}

bool PlayTrack::loadFromFolder(const QString &dirPath)
{
    const QString canonical = QDir(dirPath).canonicalPath();
    if (canonical.isEmpty()) {
        emit loadFailed(QStringLiteral("无效的文件夹路径: ") + dirPath);
        return false;
    }

    // ── 命中缓存，直接返回 ──
    if (s_cache.contains(canonical)) {
        m_currentFolder = canonical;
        m_songs         = s_cache.value(canonical);

        emit folderChanged(canonical);
        emit songsLoaded(m_songs);
        return true;
    }

    // ── 扫描文件夹 ──
    const QList<Song> songs = scanFolder(canonical);

    if (songs.isEmpty()) {
        emit loadFailed(QStringLiteral("文件夹中未找到支持的音频文件: ") + canonical);
        return false;
    }

    // ── 写入缓存 ──
    s_cache.insert(canonical, songs);

    m_currentFolder = canonical;
    m_songs         = songs;

    emit folderChanged(canonical);
    emit songsLoaded(m_songs);
    return true;
}

// ════════════════════════════════════════════════════════════
//  访问曲目
// ════════════════════════════════════════════════════════════

const QList<Song> &PlayTrack::songs() const
{
    return m_songs;
}

int PlayTrack::songCount() const
{
    return m_songs.size();
}

Song PlayTrack::songAt(int index) const
{
    return m_songs.value(index);
}

QString PlayTrack::currentFolder() const
{
    return m_currentFolder;
}

// ════════════════════════════════════════════════════════════
//  缓存管理
// ════════════════════════════════════════════════════════════

void PlayTrack::clearCache()
{
    s_cache.clear();
}

void PlayTrack::clearCacheForFolder(const QString &dirPath)
{
    const QString canonical = QDir(dirPath).canonicalPath();
    s_cache.remove(canonical);
}

// ════════════════════════════════════════════════════════════
//  内部 — 扫描文件夹
// ════════════════════════════════════════════════════════════

QList<Song> PlayTrack::scanFolder(const QString &dirPath)
{
    QList<Song> result;

    QDirIterator it(dirPath, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo fi     = it.fileInfo();
        const QString   suffix = fi.suffix().toLower();

        if (!isSupportedAudio(suffix))
            continue;

        Song song(fi.absoluteFilePath());
        song.title = extractTitle(song.filePath);
        result.append(song);
    }

    // 按标题排序，保证每次加载顺序一致
    std::sort(result.begin(), result.end(),
              [](const Song &a, const Song &b) {
                  return a.title.toLower() < b.title.toLower();
              });

    return result;
}

// ════════════════════════════════════════════════════════════
//  内部 — 工具函数
// ════════════════════════════════════════════════════════════

QString PlayTrack::extractTitle(const QString &filePath)
{
    const QFileInfo fi(filePath);
    QString name = fi.completeBaseName(); // "01 - Hello.mp3" → "01 - Hello"

    // 去掉前导数字序号，如 "01 - Hello" → "Hello"
    static const QRegularExpression kLeadingNum(QStringLiteral(R"(^\d+[\s\-\_\.]*)"));
    name = name.remove(kLeadingNum).trimmed();

    return name.isEmpty() ? fi.completeBaseName() : name;
}

bool PlayTrack::isSupportedAudio(const QString &suffix)
{
    return kAudioSuffixes.contains(suffix);
}
