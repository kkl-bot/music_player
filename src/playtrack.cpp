#include "playtrack.h"
#include "library.h"

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
//  构造 / 析构
// ════════════════════════════════════════════════════════════

PlayTrack::PlayTrack(QObject *parent)
    : QObject(parent)
{
}

PlayTrack::~PlayTrack() = default;

void PlayTrack::setLibrary(Library *library)
{
    m_library = library;
}

// ════════════════════════════════════════════════════════════
//  文件夹浏览与加载
// ════════════════════════════════════════════════════════════

bool PlayTrack::browseAndLoad()
{
    const QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        QStringLiteral("选择音乐文件夹"),
        m_currentFolder.isEmpty() ? QString() : m_currentFolder,
        QFileDialog::ShowDirsOnly | QFileDialog::ReadOnly);

    if (dir.isEmpty())
        return false;

    return loadFromFolder(dir);
}

bool PlayTrack::loadFromFolder(const QString &dirPath)
{
    const QString canonical = QDir(dirPath).canonicalPath();
    if (canonical.isEmpty()) {
        emit loadFailed(QStringLiteral("无效的文件夹路径: ") + dirPath);
        return false;
    }

    // ── 优先使用 Library 缓存 ──
    if (m_library && m_library->hasFolderCache(canonical)) {
        m_songs = m_library->loadCachedFolderSongs(canonical);
        if (!m_songs.isEmpty()) {
            m_currentFolder = canonical;
            emit folderChanged(canonical);
            emit songsLoaded(m_songs);
            return true;
        }
    }

    // ── 缓存未命中，扫描文件夹 ──
    const QList<Song> songs = scanFolder(canonical);

    if (songs.isEmpty()) {
        emit loadFailed(QStringLiteral("文件夹中未找到支持的音频文件: ") + canonical);
        return false;
    }

    // ── 写入 Library 缓存 ──
    if (m_library)
        m_library->cacheFolderSongs(canonical, songs);

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
//  缓存管理（委托给 Library）
// ════════════════════════════════════════════════════════════

void PlayTrack::clearCache()
{
    if (m_library)
        m_library->clearFolderCache();
}

void PlayTrack::clearCacheForFolder(const QString &dirPath)
{
    if (m_library) {
        const QString canonical = QDir(dirPath).canonicalPath();
        m_library->clearFolderCacheFor(canonical);
    }
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

        Song song(fi.absoluteFilePath());   // 构造函数已自动探测 artist/album/title
        // 如果 probeAll 未读到标题，用文件名规则降级
        if (song.title.isEmpty() || song.title == fi.completeBaseName())
            song.title = extractTitle(fi.absoluteFilePath());
        result.append(song);
    }

    // 按原始文件名排序（保留文件自身编号顺序）
    std::sort(result.begin(), result.end(),
              [](const Song &a, const Song &b) {
                  return QFileInfo(a.filePath).completeBaseName().toLower()
                       < QFileInfo(b.filePath).completeBaseName().toLower();
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
