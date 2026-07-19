#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <QObject>
#include <QList>
#include "song.h"

/// Playlist - 播放列表管理
///
/// 功能：
///   1. 维护有序歌曲列表
///   2. 当前曲目追踪（currentIndex）
///   3. 上/下一首切换
///   4. 拖拽排序接口
///   5. 重复/随机模式
class Playlist : public QObject
{
    Q_OBJECT

public:
    /// 重复模式
    enum class RepeatMode {
        None,       // 列表播放完即停
        One,        // 单曲循环
        All,        // 列表循环
    };
    Q_ENUM(RepeatMode)

    explicit Playlist(QObject *parent = nullptr);
    ~Playlist() override;

    // ── 列表操作 ──
    void setSongs(const QList<Song> &songs);
    void addSong(const Song &song);
    void insertSong(int index, const Song &song);
    void removeSong(int index);
    void moveSong(int fromIndex, int toIndex);
    void clear();

    // ── 访问 ──
    int  count() const;
    bool isEmpty() const;
    Song songAt(int index) const;
    const QList<Song> &allSongs() const;

    // ── 当前曲目 ──
    int  currentIndex() const;
    void setCurrentIndex(int index);

    Song currentSong() const;
    bool hasCurrent() const;

    // ── 导航 ──
    bool hasNext() const;
    bool hasPrevious() const;

    /// 跳到下一首，返回切换后的歌曲（考虑循环/随机模式）
    Song next();
    /// 跳到上一首
    Song previous();

    /// 跳到指定索引
    void jumpTo(int index);

    // ── 搜索过滤 ──
    /// 返回所有标题或文件名包含 keyword（不区分大小写）的歌曲索引
    QList<int> searchIndices(const QString &keyword) const;

    // ── 播放模式 ──
    void setRepeatMode(RepeatMode mode);
    RepeatMode repeatMode() const;

    void setShuffle(bool enabled);
    bool isShuffle() const;

signals:
    void songsChanged();
    void currentIndexChanged(int index);
    void songInserted(int index);
    void songRemoved(int index);
    void repeatModeChanged(Playlist::RepeatMode mode);
    void shuffleChanged(bool enabled);

private:
    QList<Song> m_songs;
    int  m_currentIndex = -1;
    bool m_shuffle = false;
    RepeatMode m_repeatMode = RepeatMode::None;

    /// 随机模式下计算下一首的索引
    int nextShuffleIndex() const;
};

#endif // PLAYLIST_H
