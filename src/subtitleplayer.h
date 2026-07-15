#ifndef SUBTITLEPLAYER_H
#define SUBTITLEPLAYER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QPair>
#include <QTime>

class SubtitlePlayer : public QObject
{
    Q_OBJECT

public:
    explicit SubtitlePlayer(QObject *parent = nullptr);
    ~SubtitlePlayer();

    bool load(const QString &filePath);
    QString getSubtitleAt(qint64 positionMs) const;
    void clear();

signals:
    void subtitleChanged(const QString &text);

private:
    struct SubtitleEntry {
        qint64 startMs;
        qint64 endMs;
        QString text;
    };

    QList<SubtitleEntry> m_subtitles;
};

#endif // SUBTITLEPLAYER_H
