#include "subtitleplayer.h"

SubtitlePlayer::SubtitlePlayer(QObject *parent)
    : QObject(parent)
{
}

SubtitlePlayer::~SubtitlePlayer()
{
}

bool SubtitlePlayer::load(const QString &filePath)
{
    Q_UNUSED(filePath);
    // TODO: 解析 SRT / ASS / LRC 字幕文件
    return false;
}

QString SubtitlePlayer::getSubtitleAt(qint64 positionMs) const
{
    for (const auto &entry : m_subtitles) {
        if (positionMs >= entry.startMs && positionMs <= entry.endMs) {
            return entry.text;
        }
    }
    return QString();
}

void SubtitlePlayer::clear()
{
    m_subtitles.clear();
}
