#include "trackmodel.h"

TrackModel::TrackModel(QObject *parent) : QAbstractListModel(parent) {}

int TrackModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tracks.size();
}

QVariant TrackModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};
    const Track &track = m_tracks.at(index.row());
    switch (role) {
    case PathRole: return track.path;
    case FileNameRole: return track.fileName;
    case TitleRole: return track.title;
    case ArtistRole: return track.artist;
    case AlbumRole: return track.album;
    case AlbumArtistRole: return track.albumArtist;
    case GenreRole: return track.genre;
    case CommentRole: return track.comment;
    case YearRole: return track.year;
    case TrackNumberRole: return track.trackNumber;
    case DiscNumberRole: return track.discNumber;
    case DurationRole: return track.durationSeconds;
    case DurationTextRole: return formatDuration(track.durationSeconds);
    case FormatRole: return track.format;
    case BitrateRole: return track.bitrate;
    case SampleRateRole: return track.sampleRate;
    case ChannelsRole: return track.channels;
    case SizeRole: return track.size;
    case HasCoverRole: return !track.cover.isEmpty();
    case SelectedRole: return track.selected;
    case DirtyRole: return track.dirty;
    default: return {};
    }
}

QHash<int, QByteArray> TrackModel::roleNames() const
{
    return {
        {PathRole, "path"}, {FileNameRole, "fileName"}, {TitleRole, "title"},
        {ArtistRole, "artist"}, {AlbumRole, "album"}, {AlbumArtistRole, "albumArtist"},
        {GenreRole, "genre"}, {CommentRole, "comment"}, {YearRole, "year"},
        {TrackNumberRole, "trackNumber"}, {DiscNumberRole, "discNumber"},
        {DurationRole, "duration"}, {DurationTextRole, "durationText"},
        {FormatRole, "format"}, {BitrateRole, "bitrate"}, {SampleRateRole, "sampleRate"},
        {ChannelsRole, "channels"}, {SizeRole, "size"}, {HasCoverRole, "hasCover"},
        {SelectedRole, "selected"}, {DirtyRole, "dirty"},
    };
}

void TrackModel::setTracks(const QList<Track> &tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
}

int TrackModel::rowForPath(const QString &path) const
{
    for (int row = 0; row < m_tracks.size(); ++row)
        if (m_tracks.at(row).path == path)
            return row;
    return -1;
}

void TrackModel::updateTrack(int row, const Track &track)
{
    if (row < 0 || row >= m_tracks.size())
        return;
    m_tracks[row] = track;
    emit dataChanged(index(row), index(row));
}

void TrackModel::refreshAll()
{
    if (!m_tracks.isEmpty())
        emit dataChanged(index(0), index(m_tracks.size() - 1));
}
