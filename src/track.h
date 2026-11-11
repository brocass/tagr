#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

struct Track {
    QString path;
    QString fileName;
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString comment;
    QString format;
    QString coverMime;
    QString musicbrainzReleaseId;
    QString musicbrainzAlbumId;
    QString musicbrainzAlbumArtistId;
    QString musicbrainzArtistId;
    QString musicbrainzTrackId;
    QString musicbrainzReleaseGroupId;
    int year = 0;
    int trackNumber = 0;
    int discNumber = 0;
    int durationSeconds = 0;
    int bitrate = 0;
    int sampleRate = 0;
    int channels = 0;
    qint64 size = 0;
    QByteArray cover;
    bool coverChanged = false;
    bool dirty = false;
    bool selected = false;
};

bool sameEditableTags(const Track &a, const Track &b);
QStringList supportedAudioSuffixes();
bool isSupportedAudioFile(const QString &path);
QStringList collectAudioFiles(const QStringList &paths, QString *error = nullptr);
bool readTrack(const QString &path, Track &track, QString &error);
bool writeTrackAtomic(const Track &track, QString &error);
QJsonObject trackToJson(const Track &track);
QString formatDuration(int seconds);
