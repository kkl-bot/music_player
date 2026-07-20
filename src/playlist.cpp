#include "playlist.h"
#include <QRandomGenerator>
#include <QFileInfo>

// ════════════════════════════════════════════════════════════
//  构造 / 析构
// ════════════════════════════════════════════════════════════

Playlist::Playlist(QObject *parent)
    : QObject(parent)
{
}

Playlist::~Playlist() = default;

// ════════════════════════════════════════════════════════════
//  列表操作
// ════════════════════════════════════════════════════════════

void Playlist::setSongs(const QList<Song> &songs)
{
    m_songs = songs;
    m_currentIndex = m_songs.isEmpty() ? -1 : 0;
    emit songsChanged();
    emit currentIndexChanged(m_currentIndex);
}

void Playlist::addSong(const Song &song)
{
    m_songs.append(song);
    if (m_currentIndex == -1)
        m_currentIndex = 0;
    emit songInserted(m_songs.size() - 1);
    emit songsChanged();
}

void Playlist::insertSong(int index, const Song &song)
{
    if (index < 0 || index > m_songs.size())
        return;

    m_songs.insert(index, song);
    if (m_currentIndex >= index)
        m_currentIndex++;

    emit songInserted(index);
    emit songsChanged();
}

void Playlist::removeSong(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;

    m_songs.removeAt(index);

    // 调整 currentIndex
    if (m_songs.isEmpty()) {
        m_currentIndex = -1;
    } else if (index < m_currentIndex) {
        m_currentIndex--;
    } else if (index == m_currentIndex && m_currentIndex >= m_songs.size()) {
        m_currentIndex = m_songs.size() - 1;
    }

    emit songRemoved(index);
    emit songsChanged();
    emit currentIndexChanged(m_currentIndex);
}

void Playlist::moveSong(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_songs.size())
        return;
    if (toIndex < 0 || toIndex >= m_songs.size())
        return;
    if (fromIndex == toIndex)
        return;

    m_songs.move(fromIndex, toIndex);

    // 调整 currentIndex
    if (m_currentIndex == fromIndex) {
        m_currentIndex = toIndex;
    } else {
        if (fromIndex < m_currentIndex && toIndex >= m_currentIndex)
            m_currentIndex--;
        else if (fromIndex > m_currentIndex && toIndex <= m_currentIndex)
            m_currentIndex++;
    }

    emit songsChanged();
    emit currentIndexChanged(m_currentIndex);
}

void Playlist::clear()
{
    m_songs.clear();
    m_currentIndex = -1;
    emit songsChanged();
    emit currentIndexChanged(m_currentIndex);
}

// ════════════════════════════════════════════════════════════
//  访问
// ════════════════════════════════════════════════════════════

int Playlist::count() const
{
    return m_songs.size();
}

bool Playlist::isEmpty() const
{
    return m_songs.isEmpty();
}

Song Playlist::songAt(int index) const
{
    return m_songs.value(index);
}

const QList<Song> &Playlist::allSongs() const
{
    return m_songs;
}

// ════════════════════════════════════════════════════════════
//  搜索过滤
// ════════════════════════════════════════════════════════════

QList<int> Playlist::searchIndices(const QString &keyword) const
{
    if (keyword.isEmpty())
        return {};

    QList<int> result;
    const QString lowerKw = keyword.toLower();
    for (int i = 0; i < m_songs.size(); ++i) {
        const Song &s = m_songs.at(i);
        // 匹配标题或文件名（不区分大小写）
        if (s.title.toLower().contains(lowerKw) ||
            QFileInfo(s.filePath).completeBaseName().toLower().contains(lowerKw)) {
            result.append(i);
        }
    }
    return result;
}

// ════════════════════════════════════════════════════════════
//  当前曲目
// ════════════════════════════════════════════════════════════

int Playlist::currentIndex() const
{
    return m_currentIndex;
}

void Playlist::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_songs.size())
        return;

    m_currentIndex = index;
    emit currentIndexChanged(m_currentIndex);
}

Song Playlist::currentSong() const
{
    return (m_currentIndex >= 0 && m_currentIndex < m_songs.size())
        ? m_songs.at(m_currentIndex) : Song();
}

Song &Playlist::currentSongRef()
{
    static Song s_nullSong;
    return (m_currentIndex >= 0 && m_currentIndex < m_songs.size())
        ? m_songs[m_currentIndex] : s_nullSong;
}

bool Playlist::hasCurrent() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_songs.size();
}

// ════════════════════════════════════════════════════════════
//  导航
// ════════════════════════════════════════════════════════════

bool Playlist::hasNext() const
{
    if (m_songs.isEmpty())
        return false;
    if (m_repeatMode == RepeatMode::All || m_repeatMode == RepeatMode::One)
        return true;
    return m_currentIndex < m_songs.size() - 1;
}

bool Playlist::hasPrevious() const
{
    if (m_songs.isEmpty())
        return false;
    if (m_repeatMode == RepeatMode::All)
        return true;
    return m_currentIndex > 0 || m_repeatMode == RepeatMode::One;
}

Song Playlist::next()
{
    if (m_songs.isEmpty())
        return {};

    if (m_shuffle) {
        m_currentIndex = nextShuffleIndex();
    } else {
        m_currentIndex++;
        // 列表循环
        if (m_currentIndex >= m_songs.size()) {
            if (m_repeatMode == RepeatMode::All)
                m_currentIndex = 0;
            else
                m_currentIndex = m_songs.size() - 1; // 停在最后一首
        }
    }

    emit currentIndexChanged(m_currentIndex);
    return currentSong();
}

Song Playlist::previous()
{
    if (m_songs.isEmpty())
        return {};

    if (m_shuffle) {
        m_currentIndex = nextShuffleIndex();
    } else {
        m_currentIndex--;
        if (m_currentIndex < 0) {
            if (m_repeatMode == RepeatMode::All)
                m_currentIndex = m_songs.size() - 1;
            else
                m_currentIndex = 0;
        }
    }

    emit currentIndexChanged(m_currentIndex);
    return currentSong();
}

void Playlist::jumpTo(int index)
{
    setCurrentIndex(index);
}

// ════════════════════════════════════════════════════════════
//  播放模式
// ════════════════════════════════════════════════════════════

void Playlist::setRepeatMode(RepeatMode mode)
{
    if (m_repeatMode == mode)
        return;
    m_repeatMode = mode;
    emit repeatModeChanged(mode);
}

Playlist::RepeatMode Playlist::repeatMode() const
{
    return m_repeatMode;
}

void Playlist::setShuffle(bool enabled)
{
    if (m_shuffle == enabled)
        return;
    m_shuffle = enabled;
    emit shuffleChanged(enabled);
}

bool Playlist::isShuffle() const
{
    return m_shuffle;
}

// ════════════════════════════════════════════════════════════
//  内部
// ════════════════════════════════════════════════════════════

int Playlist::nextShuffleIndex() const
{
    if (m_songs.size() <= 1)
        return 0;

    int idx;
    do {
        idx = QRandomGenerator::global()->bounded(m_songs.size());
    } while (idx == m_currentIndex && m_songs.size() > 1);

    return idx;
}
