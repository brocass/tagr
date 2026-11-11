#pragma once

#include <QAbstractListModel>

#include "track.h"

class TrackModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        FileNameRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        AlbumArtistRole,
        GenreRole,
        CommentRole,
        YearRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationRole,
        DurationTextRole,
        FormatRole,
        BitrateRole,
        SampleRateRole,
        ChannelsRole,
        SizeRole,
        HasCoverRole,
        SelectedRole,
        DirtyRole,
    };

    explicit TrackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setTracks(const QList<Track> &tracks);
    const QList<Track> &tracks() const { return m_tracks; }
    QList<Track> &tracks() { return m_tracks; }
    int rowForPath(const QString &path) const;
    void updateTrack(int row, const Track &track);
    void refreshAll();

private:
    QList<Track> m_tracks;
};
