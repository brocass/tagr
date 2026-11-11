#include "backend.h"

#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QLocale>
#include <QMimeDatabase>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#include "filepicker.h"
#include "portalfilepicker.h"

namespace {
class TrackFilterModel : public QSortFilterProxyModel {
public:
    explicit TrackFilterModel(QObject *parent = nullptr) : QSortFilterProxyModel(parent) {}

    QString needle() const { return m_needle; }
    void setNeedle(const QString &needle)
    {
        beginFilterChange();
        m_needle = needle;
        endFilterChange();
    }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
    {
        if (m_needle.isEmpty())
            return true;
        const auto *model = sourceModel();
        static const QList<int> roles = {
            TrackModel::PathRole, TrackModel::FileNameRole, TrackModel::TitleRole,
            TrackModel::ArtistRole, TrackModel::AlbumRole, TrackModel::AlbumArtistRole,
            TrackModel::GenreRole,
        };
        for (const int role : roles) {
            if (model->data(model->index(sourceRow, 0, sourceParent), role)
                    .toString().contains(m_needle, Qt::CaseInsensitive))
                return true;
        }
        return false;
    }

private:
    QString m_needle;
};

QString valueForTrack(const Track &track, const QString &field)
{
    if (field == QStringLiteral("title")) return track.title;
    if (field == QStringLiteral("artist")) return track.artist;
    if (field == QStringLiteral("album")) return track.album;
    if (field == QStringLiteral("albumArtist")) return track.albumArtist;
    if (field == QStringLiteral("genre")) return track.genre;
    if (field == QStringLiteral("comment")) return track.comment;
    if (field == QStringLiteral("year")) return track.year > 0 ? QString::number(track.year) : QString();
    if (field == QStringLiteral("track")) return track.trackNumber > 0 ? QString::number(track.trackNumber) : QString();
    if (field == QStringLiteral("disc")) return track.discNumber > 0 ? QString::number(track.discNumber) : QString();
    if (field == QStringLiteral("musicbrainzReleaseId")) return track.musicbrainzReleaseId;
    if (field == QStringLiteral("musicbrainzAlbumId")) return track.musicbrainzAlbumId;
    if (field == QStringLiteral("musicbrainzAlbumArtistId")) return track.musicbrainzAlbumArtistId;
    if (field == QStringLiteral("musicbrainzArtistId")) return track.musicbrainzArtistId;
    if (field == QStringLiteral("musicbrainzTrackId")) return track.musicbrainzTrackId;
    if (field == QStringLiteral("musicbrainzReleaseGroupId")) return track.musicbrainzReleaseGroupId;
    return {};
}
}

Backend::Backend(FilePicker *picker, QObject *parent, const QString &settingsFile) : QObject(parent)
{
    m_settingsFile = settingsFile.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QStringLiteral("/tagr.ini")
        : settingsFile;
    loadRecentDirectories();
    loadColumnVisibility();
    auto *proxy = new TrackFilterModel(this);
    m_proxy = proxy;
    m_proxy->setSourceModel(&m_model);
    m_proxy->setDynamicSortFilter(true);
    m_proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setSortLocaleAware(true);
    m_proxy->setSortRole(TrackModel::FileNameRole);
    m_proxy->sort(0, Qt::AscendingOrder);
    connect(m_proxy, &QAbstractItemModel::rowsInserted, this, &Backend::tracksChanged);
    connect(m_proxy, &QAbstractItemModel::rowsRemoved, this, &Backend::tracksChanged);
    connect(m_proxy, &QAbstractItemModel::modelReset, this, &Backend::tracksChanged);
    connect(m_proxy, &QAbstractItemModel::layoutChanged, this, &Backend::tracksChanged);

    m_picker = picker ? picker : new PortalFilePicker(this);
    if (picker)
        picker->setParent(this);
    if (!m_recentDirectories.isEmpty())
        m_picker->setDefaultFolder(m_recentDirectories.first());
    connect(m_picker, &FilePicker::directorySelected, this, [this](const QUrl &url) {
        if (url.isLocalFile())
            openPaths({url.toLocalFile()});
    });
    connect(m_picker, &FilePicker::filesSelected, this, [this](const QList<QUrl> &urls) {
        QStringList paths;
        for (const auto &url : urls)
            if (url.isLocalFile())
                paths.append(url.toLocalFile());
        openPaths(paths);
    });
    connect(m_picker, &FilePicker::imageSelected, this, [this](const QUrl &url) {
        if (url.isLocalFile())
            setCoverFromFile(url.toLocalFile());
    });
    connect(m_picker, &FilePicker::failed, this, [this](const QString &message) {
        setStatus(message, true);
    });
    connect(&m_loadWatcher, &QFutureWatcher<LoadResult>::finished, this, &Backend::finishLoading);
}

Backend::~Backend()
{
    if (m_loadWatcher.isRunning())
        m_loadWatcher.waitForFinished();
}

QString Backend::filterText() const
{
    return static_cast<TrackFilterModel *>(m_proxy)->needle();
}

void Backend::setFilterText(const QString &text)
{
    if (text == filterText())
        return;
    static_cast<TrackFilterModel *>(m_proxy)->setNeedle(text);
    emit filterChanged();
    emit tracksChanged();
}

int Backend::selectedCount() const
{
    int count = 0;
    for (const auto &track : m_model.tracks())
        if (track.selected)
            ++count;
    return count;
}

int Backend::dirtyCount() const
{
    int count = 0;
    for (const auto &track : m_model.tracks())
        if (track.dirty)
            ++count;
    return count;
}

QString Backend::yearText() const { return fieldValue(QStringLiteral("year")); }
QString Backend::trackText() const { return fieldValue(QStringLiteral("track")); }
QString Backend::discText() const { return fieldValue(QStringLiteral("disc")); }
QString Backend::musicbrainzReleaseId() const { return fieldValue(QStringLiteral("musicbrainzReleaseId")); }
QString Backend::musicbrainzAlbumId() const { return fieldValue(QStringLiteral("musicbrainzAlbumId")); }
QString Backend::musicbrainzAlbumArtistId() const { return fieldValue(QStringLiteral("musicbrainzAlbumArtistId")); }
QString Backend::musicbrainzArtistId() const { return fieldValue(QStringLiteral("musicbrainzArtistId")); }
QString Backend::musicbrainzTrackId() const { return fieldValue(QStringLiteral("musicbrainzTrackId")); }
QString Backend::musicbrainzReleaseGroupId() const { return fieldValue(QStringLiteral("musicbrainzReleaseGroupId")); }

const Track *Backend::firstSelected() const
{
    for (const auto &track : m_model.tracks())
        if (track.selected)
            return &track;
    return nullptr;
}

QList<Track> Backend::selectedTracks() const
{
    QList<Track> tracks;
    for (const auto &track : m_model.tracks())
        if (track.selected)
            tracks.append(track);
    return tracks;
}

QString Backend::fieldValue(const QString &field) const
{
    const Track *track = firstSelected();
    return track ? valueForTrack(*track, field) : QString();
}

QStringList Backend::fieldValues(const QString &field) const
{
    QStringList values;
    for (const auto &track : selectedTracks()) {
        const QString value = valueForTrack(track, field);
        if (!values.contains(value))
            values.append(value);
    }
    return values;
}

bool Backend::fieldMixed(const QString &field) const
{
    const auto tracks = selectedTracks();
    if (tracks.size() < 2)
        return false;
    const QString first = valueForTrack(tracks.first(), field);
    for (int index = 1; index < tracks.size(); ++index)
        if (valueForTrack(tracks.at(index), field) != first)
            return true;
    return false;
}

bool Backend::hasCover() const
{
    const Track *track = firstSelected();
    return track && !track->cover.isEmpty();
}

QString Backend::technicalInfo() const
{
    const Track *track = firstSelected();
    if (!track)
        return {};
    const QLocale locale;
    return tr("%1 · %2 Hz · %3 channels · %4 kbps")
        .arg(track->format, locale.toString(track->sampleRate),
             locale.toString(track->channels), locale.toString(track->bitrate));
}

QString Backend::coverInfo() const
{
    const Track *track = firstSelected();
    if (!track || track->cover.isEmpty())
        return {};
    const QImage image = QImage::fromData(track->cover);
    const QLocale locale;
    QStringList lines{track->coverMime};
    if (!image.isNull())
        lines.append(QStringLiteral("%1 × %2").arg(locale.toString(image.width()), locale.toString(image.height())));
    lines.append(tr("%1 kB").arg(locale.toString(track->cover.size() / 1024.0, 'f', 1)));
    lines.append(tr("Front cover"));
    return lines.join(QLatin1Char('\n'));
}

QImage Backend::currentCoverImage() const
{
    const Track *track = firstSelected();
    return track && !track->cover.isEmpty() ? QImage::fromData(track->cover) : QImage();
}

void Backend::updateDirty(Track &track) const
{
    const auto clean = m_clean.constFind(track.path);
    track.dirty = clean == m_clean.constEnd() || !sameEditableTags(track, clean.value());
    track.coverChanged = clean == m_clean.constEnd() || track.cover != clean.value().cover
        || track.coverMime != clean.value().coverMime;
}

void Backend::setField(const QString &field, const QString &value)
{
    static const QStringList fields = {
        QStringLiteral("title"), QStringLiteral("artist"), QStringLiteral("album"),
        QStringLiteral("albumArtist"), QStringLiteral("genre"), QStringLiteral("comment"),
        QStringLiteral("year"), QStringLiteral("track"), QStringLiteral("disc"),
        QStringLiteral("musicbrainzReleaseId"), QStringLiteral("musicbrainzAlbumId"),
        QStringLiteral("musicbrainzAlbumArtistId"), QStringLiteral("musicbrainzArtistId"),
        QStringLiteral("musicbrainzTrackId"), QStringLiteral("musicbrainzReleaseGroupId"),
    };
    if (!fields.contains(field) || selectedCount() == 0)
        return;

    const QList<Track> before = selectedTracks();
    bool changed = false;
    for (int row = 0; row < m_model.rowCount(); ++row) {
        Track track = m_model.tracks().at(row);
        if (!track.selected)
            continue;
        const int number = value.toInt();
        if (field == QStringLiteral("title")) track.title = value;
        else if (field == QStringLiteral("artist")) track.artist = value;
        else if (field == QStringLiteral("album")) track.album = value;
        else if (field == QStringLiteral("albumArtist")) track.albumArtist = value;
        else if (field == QStringLiteral("genre")) track.genre = value;
        else if (field == QStringLiteral("comment")) track.comment = value;
        else if (field == QStringLiteral("year")) track.year = qBound(0, number, 9999);
        else if (field == QStringLiteral("track")) track.trackNumber = qBound(0, number, 9999);
        else if (field == QStringLiteral("disc")) track.discNumber = qBound(0, number, 9999);
        else if (field == QStringLiteral("musicbrainzReleaseId")) track.musicbrainzReleaseId = value;
        else if (field == QStringLiteral("musicbrainzAlbumId")) track.musicbrainzAlbumId = value;
        else if (field == QStringLiteral("musicbrainzAlbumArtistId")) track.musicbrainzAlbumArtistId = value;
        else if (field == QStringLiteral("musicbrainzArtistId")) track.musicbrainzArtistId = value;
        else if (field == QStringLiteral("musicbrainzTrackId")) track.musicbrainzTrackId = value;
        else if (field == QStringLiteral("musicbrainzReleaseGroupId")) track.musicbrainzReleaseGroupId = value;
        updateDirty(track);
        if (!sameEditableTags(track, m_model.tracks().at(row)))
            changed = true;
        m_model.updateTrack(row, track);
    }
    if (!changed)
        return;
    pushHistory(before);
    ++m_formRevision;
    emit formChanged();
    emit dirtyChanged();
    setStatus(tr("%n tracks edited.", "", selectedCount()));
}

void Backend::pushHistory(const QList<Track> &before)
{
    while (m_history.size() > m_historyIndex + 1)
        m_history.removeLast();
    m_history.append({before, selectedTracks()});
    if (m_history.size() > 100) {
        m_history.removeFirst();
    } else {
        ++m_historyIndex;
    }
    emit historyChanged();
}

void Backend::applyHistory(const QList<Track> &tracks)
{
    for (const Track &snapshot : tracks) {
        const int row = m_model.rowForPath(snapshot.path);
        if (row < 0)
            continue;
        Track track = snapshot;
        track.selected = m_model.tracks().at(row).selected;
        updateDirty(track);
        m_model.updateTrack(row, track);
    }
    ++m_formRevision;
    ++m_coverRevision;
    emit formChanged();
    emit coverChanged();
    emit dirtyChanged();
}

void Backend::clearHistory()
{
    m_history.clear();
    m_historyIndex = -1;
    emit historyChanged();
}

void Backend::undo()
{
    if (!canUndo())
        return;
    applyHistory(m_history.at(m_historyIndex).before);
    --m_historyIndex;
    emit historyChanged();
    setStatus(tr("Change undone."));
}

void Backend::redo()
{
    if (!canRedo())
        return;
    ++m_historyIndex;
    applyHistory(m_history.at(m_historyIndex).after);
    emit historyChanged();
    setStatus(tr("Change redone."));
}

int Backend::sourceRowForVisibleRow(int row) const
{
    const QModelIndex proxyIndex = m_proxy->index(row, 0);
    return proxyIndex.isValid() ? m_proxy->mapToSource(proxyIndex).row() : -1;
}

void Backend::selectRow(int row, bool control, bool shift)
{
    const int clickedSourceRow = sourceRowForVisibleRow(row);
    if (clickedSourceRow < 0)
        return;
    const QString clickedPath = m_model.tracks().at(clickedSourceRow).path;

    if (shift && !m_anchorPath.isEmpty()) {
        const int anchorSource = m_model.rowForPath(m_anchorPath);
        const QModelIndex anchorProxy = anchorSource >= 0
            ? m_proxy->mapFromSource(m_model.index(anchorSource)) : QModelIndex();
        if (anchorProxy.isValid()) {
            if (!control)
                for (auto &track : m_model.tracks()) track.selected = false;
            const int first = qMin(anchorProxy.row(), row);
            const int last = qMax(anchorProxy.row(), row);
            for (int visibleRow = first; visibleRow <= last; ++visibleRow) {
                const int sourceRow = sourceRowForVisibleRow(visibleRow);
                if (sourceRow >= 0)
                    m_model.tracks()[sourceRow].selected = true;
            }
        }
    } else {
        if (!control)
            for (auto &track : m_model.tracks()) track.selected = false;
        auto &track = m_model.tracks()[clickedSourceRow];
        track.selected = control ? !track.selected : true;
        m_anchorPath = clickedPath;
    }
    m_model.refreshAll();
    notifySelection();
}

void Backend::selectAll()
{
    for (auto &track : m_model.tracks())
        track.selected = false;
    for (int row = 0; row < m_proxy->rowCount(); ++row) {
        const int sourceRow = sourceRowForVisibleRow(row);
        if (sourceRow >= 0)
            m_model.tracks()[sourceRow].selected = true;
    }
    m_model.refreshAll();
    notifySelection();
}

void Backend::clearSelection()
{
    for (auto &track : m_model.tracks())
        track.selected = false;
    m_anchorPath.clear();
    m_model.refreshAll();
    notifySelection();
}

void Backend::notifySelection()
{
    ++m_formRevision;
    ++m_coverRevision;
    emit selectionChanged();
    emit formChanged();
    emit coverChanged();
}

void Backend::sortBy(const QString &column)
{
    static const QHash<QString, int> roles = {
        {QStringLiteral("fileName"), TrackModel::FileNameRole},
        {QStringLiteral("path"), TrackModel::PathRole},
        {QStringLiteral("title"), TrackModel::TitleRole},
        {QStringLiteral("artist"), TrackModel::ArtistRole},
        {QStringLiteral("albumArtist"), TrackModel::AlbumArtistRole},
        {QStringLiteral("album"), TrackModel::AlbumRole},
        {QStringLiteral("genre"), TrackModel::GenreRole},
        {QStringLiteral("trackNumber"), TrackModel::TrackNumberRole},
        {QStringLiteral("duration"), TrackModel::DurationRole},
        {QStringLiteral("format"), TrackModel::FormatRole},
    };
    if (!roles.contains(column))
        return;
    const Qt::SortOrder order = m_sortColumn == column && m_proxy->sortOrder() == Qt::AscendingOrder
        ? Qt::DescendingOrder : Qt::AscendingOrder;
    m_sortColumn = column;
    m_proxy->setSortRole(roles.value(column));
    m_proxy->sort(0, order);
    emit sortChanged();
}

void Backend::loadRecentDirectories()
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    const auto stored = settings.value(QStringLiteral("recentDirectories")).toStringList();
    for (const QString &directory : stored) {
        if (!directory.isEmpty() && !m_recentDirectories.contains(directory))
            m_recentDirectories.append(directory);
        if (m_recentDirectories.size() == 8)
            break;
    }
}

bool Backend::columnVisible(const QString &column, bool fallback) const
{
    const auto overrideIterator = m_columnVisibility.constFind(column);
    return overrideIterator != m_columnVisibility.constEnd() ? overrideIterator.value() : fallback;
}

void Backend::setColumnVisible(const QString &column, bool visible)
{
    if (columnVisible(column, columnVisibleDefault(column)) == visible)
        return;
    m_columnVisibility.insert(column, visible);
    persistColumnVisibility();
    if (column == QStringLiteral("path")) {
        emit showPathColumnChanged();
    } else if (column == QStringLiteral("title")) {
        emit showTitleColumnChanged();
    } else if (column == QStringLiteral("artist")) {
        emit showArtistColumnChanged();
    } else if (column == QStringLiteral("albumArtist")) {
        emit showAlbumArtistColumnChanged();
    } else if (column == QStringLiteral("album")) {
        emit showAlbumColumnChanged();
    } else if (column == QStringLiteral("genre")) {
        emit showGenreColumnChanged();
    } else if (column == QStringLiteral("trackNumber")) {
        emit showTrackColumnChanged();
    } else if (column == QStringLiteral("duration")) {
        emit showDurationColumnChanged();
    } else if (column == QStringLiteral("format")) {
        emit showFormatColumnChanged();
    }
}

bool Backend::showPathColumn() const { return columnVisible(QStringLiteral("path"), columnVisibleDefault(QStringLiteral("path"))); }
bool Backend::showTitleColumn() const { return columnVisible(QStringLiteral("title"), columnVisibleDefault(QStringLiteral("title"))); }
bool Backend::showArtistColumn() const { return columnVisible(QStringLiteral("artist"), columnVisibleDefault(QStringLiteral("artist"))); }
bool Backend::showAlbumArtistColumn() const { return columnVisible(QStringLiteral("albumArtist"), columnVisibleDefault(QStringLiteral("albumArtist"))); }
bool Backend::showAlbumColumn() const { return columnVisible(QStringLiteral("album"), columnVisibleDefault(QStringLiteral("album"))); }
bool Backend::showGenreColumn() const { return columnVisible(QStringLiteral("genre"), columnVisibleDefault(QStringLiteral("genre"))); }
bool Backend::showTrackColumn() const { return columnVisible(QStringLiteral("trackNumber"), columnVisibleDefault(QStringLiteral("trackNumber"))); }
bool Backend::showDurationColumn() const { return columnVisible(QStringLiteral("duration"), columnVisibleDefault(QStringLiteral("duration"))); }
bool Backend::showFormatColumn() const { return columnVisible(QStringLiteral("format"), columnVisibleDefault(QStringLiteral("format"))); }

bool Backend::columnVisibleDefault(const QString &column) const
{
    static const QHash<QString, bool> defaults = {
        {QStringLiteral("path"), true},
        {QStringLiteral("title"), true},
        {QStringLiteral("artist"), true},
        {QStringLiteral("albumArtist"), false},
        {QStringLiteral("album"), true},
        {QStringLiteral("genre"), false},
        {QStringLiteral("trackNumber"), true},
        {QStringLiteral("duration"), true},
        {QStringLiteral("format"), true},
    };
    return defaults.value(column, true);
}

void Backend::loadColumnVisibility()
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    static const QStringList columnKeys = {
        QStringLiteral("path"), QStringLiteral("title"), QStringLiteral("artist"),
        QStringLiteral("albumArtist"), QStringLiteral("album"), QStringLiteral("genre"),
        QStringLiteral("trackNumber"), QStringLiteral("duration"), QStringLiteral("format"),
    };
    for (const QString &key : columnKeys) {
        const QVariant stored = settings.value(QStringLiteral("columns/%1").arg(key), QVariant());
        if (stored.isValid())
            m_columnVisibility.insert(key, stored.toBool());
    }
}

void Backend::persistColumnVisibility()
{
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    const QStringList columnKeys = {
        QStringLiteral("path"), QStringLiteral("title"), QStringLiteral("artist"),
        QStringLiteral("albumArtist"), QStringLiteral("album"), QStringLiteral("genre"),
        QStringLiteral("trackNumber"), QStringLiteral("duration"), QStringLiteral("format"),
    };
    for (const QString &key : columnKeys) {
        if (m_columnVisibility.contains(key))
            settings.setValue(QStringLiteral("columns/%1").arg(key), m_columnVisibility.value(key));
    }
    settings.sync();
}

void Backend::rememberDirectory(const QString &directory)
{
    if (directory.isEmpty())
        return;
    const QString clean = QDir::cleanPath(directory);
    if (!m_recentDirectories.isEmpty() && m_recentDirectories.constFirst() == clean)
        return;
    m_recentDirectories.removeAll(clean);
    m_recentDirectories.prepend(clean);
    while (m_recentDirectories.size() > 8)
        m_recentDirectories.removeLast();
    QDir().mkpath(QFileInfo(m_settingsFile).absolutePath());
    QSettings settings(m_settingsFile, QSettings::IniFormat);
    settings.setValue(QStringLiteral("recentDirectories"), m_recentDirectories);
    settings.sync();
    emit recentDirectoriesChanged();
    if (settings.status() != QSettings::NoError)
        setStatus(tr("Could not save recent directories."), true);
}

void Backend::openDirectoryFromEditor(const QString &path)
{
    QString directory = path.trimmed();
    if (directory.startsWith(QStringLiteral("file://")))
        directory = QUrl(directory).toLocalFile();
    if (directory == QStringLiteral("~"))
        directory = QDir::homePath();
    else if (directory.startsWith(QStringLiteral("~/")))
        directory = QDir::homePath() + directory.mid(1);
    if (directory.isEmpty()) {
        setStatus(tr("Enter a directory path."), true);
        return;
    }
    const QFileInfo info(directory);
    if (!info.isDir()) {
        setStatus(tr("Directory does not exist: %1").arg(directory), true);
        return;
    }
    openPaths({info.absoluteFilePath()});
}

void Backend::playPath(const QString &path)
{
    if (path.isEmpty())
        return;
    // Ask the desktop's default handler instead of QDesktopServices: the latter
    // goes through the xdg-desktop-portal and pops an "Open With" chooser.
    const QString opener = QStandardPaths::findExecutable(QStringLiteral("xdg-open"));
    if (!opener.isEmpty() && QProcess::startDetached(opener, {path}))
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QString Backend::firstSelectedPath() const
{
    const Track *track = firstSelected();
    return track ? track->path : QString();
}

bool Backend::renameTrackFile(const QString &oldPath, const QString &newPath)
{
    if (oldPath == newPath || newPath.isEmpty())
        return false;
    const QFileInfo oldInfo(oldPath);
    if (!oldInfo.exists())
        return false;
    const QString targetPath = oldInfo.absoluteDir().absoluteFilePath(newPath);
    if (QFileInfo::exists(targetPath))
        return false;
    if (!QFile::rename(oldPath, targetPath))
        return false;
    const int row = m_model.rowForPath(oldPath);
    if (row < 0)
        return true;
    Track track;
    QString error;
    if (!readTrack(targetPath, track, error))
        return false;
    track.selected = m_model.tracks().at(row).selected;
    track.dirty = false;
    m_clean.take(oldPath);
    m_clean.insert(targetPath, track);
    m_model.updateTrack(row, track);
    ++m_formRevision;
    emit formChanged();
    emit dirtyChanged();
    setStatus(tr("Renamed to %1.").arg(newPath));
    return true;
}

void Backend::chooseDirectory()
{
    m_picker->openDirectory();
}

void Backend::chooseFiles()
{
    m_picker->openFiles();
}

void Backend::chooseCover()
{
    if (selectedCount() == 0) {
        setStatus(tr("Select one or more tracks first."), true);
        return;
    }
    m_picker->openImage();
}

void Backend::openPaths(const QStringList &paths)
{
    if (paths.isEmpty() || m_loading) {
        if (m_loading)
            setStatus(tr("Wait for the current folder to finish loading."), true);
        return;
    }
    m_sourcePaths = paths;
    const QFileInfo first(paths.first());
    m_directory = first.isDir() ? first.absoluteFilePath() : first.absolutePath();
    emit directoryChanged();
    m_loading = true;
    emit loadingChanged();
    setStatus(tr("Loading audio files…"));
    m_loadWatcher.setFuture(QtConcurrent::run([paths] {
        LoadResult result;
        QString collectionError;
        const QStringList files = collectAudioFiles(paths, &collectionError);
        if (!collectionError.isEmpty())
            result.errors.append(collectionError);
        for (const QString &path : files) {
            Track track;
            QString error;
            if (readTrack(path, track, error))
                result.tracks.append(track);
            else
                result.errors.append(error);
        }
        return result;
    }));
}

void Backend::finishLoading()
{
    const LoadResult result = m_loadWatcher.result();
    m_clean.clear();
    for (const auto &track : result.tracks)
        m_clean.insert(track.path, track);
    m_model.setTracks(result.tracks);
    m_proxy->sort(0, m_proxy->sortOrder());
    m_anchorPath.clear();
    clearHistory();
    m_loading = false;
    emit loadingChanged();
    emit tracksChanged();
    emit dirtyChanged();
    if (m_proxy->rowCount() > 0)
        selectRow(0);
    else
        notifySelection();
    if (!result.errors.isEmpty())
        setStatus(result.errors.join(QLatin1Char('\n')), true);
    else
        setStatus(tr("%n tracks loaded.", "", trackCount()));
    if (trackCount() > 0)
        rememberDirectory(m_directory);
}

bool Backend::setCoverFromFile(const QString &path)
{
    if (selectedCount() == 0) {
        setStatus(tr("Select one or more tracks first."), true);
        return false;
    }
    const QFileInfo info(path);
    if (!info.isFile() || info.size() > 20 * 1024 * 1024) {
        setStatus(tr("Choose a JPEG or PNG image smaller than 20 MB."), true);
        return false;
    }
    QImageReader reader(path);
    const QByteArray format = reader.imageFormat(path).toLower();
    if (format != "jpeg" && format != "jpg" && format != "png") {
        setStatus(tr("Cover art must be a JPEG or PNG image."), true);
        return false;
    }
    const QImage validation = reader.read();
    if (validation.isNull()) {
        setStatus(tr("Could not decode the cover image: %1").arg(reader.errorString()), true);
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatus(tr("Could not read the cover image: %1").arg(file.errorString()), true);
        return false;
    }
    const QByteArray bytes = file.readAll();
    const QString mime = format == "png" ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    const QList<Track> before = selectedTracks();
    for (int row = 0; row < m_model.rowCount(); ++row) {
        Track track = m_model.tracks().at(row);
        if (!track.selected)
            continue;
        track.cover = bytes;
        track.coverMime = mime;
        updateDirty(track);
        m_model.updateTrack(row, track);
    }
    pushHistory(before);
    ++m_coverRevision;
    emit coverChanged();
    emit dirtyChanged();
    setStatus(tr("Cover art updated for %n tracks.", "", selectedCount()));
    return true;
}

void Backend::removeCover()
{
    if (selectedCount() == 0)
        return;
    const QList<Track> before = selectedTracks();
    bool changed = false;
    for (int row = 0; row < m_model.rowCount(); ++row) {
        Track track = m_model.tracks().at(row);
        if (!track.selected || (track.cover.isEmpty() && track.coverMime.isEmpty()))
            continue;
        track.cover.clear();
        track.coverMime.clear();
        updateDirty(track);
        m_model.updateTrack(row, track);
        changed = true;
    }
    if (!changed)
        return;
    pushHistory(before);
    ++m_coverRevision;
    emit coverChanged();
    emit dirtyChanged();
    setStatus(tr("Cover art removed from %n tracks.", "", selectedCount()));
}

bool Backend::save()
{
    if (m_loading)
        return false;
    int attempted = 0;
    int saved = 0;
    QStringList errors;
    const int rows = m_model.rowCount();
    for (int row = 0; row < rows; ++row) {
        Track track = m_model.tracks().at(row);
        if (!track.dirty)
            continue;
        ++attempted;
        QString error;
        if (!writeTrackAtomic(track, error)) {
            errors.append(error);
            continue;
        }
        Track savedTrack;
        if (!readTrack(track.path, savedTrack, error)) {
            savedTrack = track;
            savedTrack.size = QFileInfo(track.path).size();
        }
        savedTrack.selected = track.selected;
        savedTrack.dirty = false;
        savedTrack.coverChanged = false;
        m_clean.insert(savedTrack.path, savedTrack);
        m_model.updateTrack(row, savedTrack);
        ++saved;
    }
    if (attempted == 0) {
        setStatus(tr("No unsaved changes."));
        return true;
    }
    if (saved > 0)
        clearHistory();
    ++m_formRevision;
    ++m_coverRevision;
    emit formChanged();
    emit coverChanged();
    emit dirtyChanged();
    if (!errors.isEmpty()) {
        setStatus(tr("Saved %1 of %2 files. %3").arg(saved).arg(attempted).arg(errors.join(QLatin1Char('\n'))), true);
        return false;
    }
    setStatus(tr("%n files saved.", "", saved));
    return true;
}

void Backend::discardChanges()
{
    QStringList errors;
    for (int row = 0; row < m_model.rowCount(); ++row) {
        const Track current = m_model.tracks().at(row);
        if (!current.dirty)
            continue;
        Track restored;
        QString error;
        if (readTrack(current.path, restored, error)) {
            restored.selected = current.selected;
            m_clean.insert(restored.path, restored);
            m_model.updateTrack(row, restored);
        } else {
            errors.append(error);
        }
    }
    clearHistory();
    ++m_formRevision;
    ++m_coverRevision;
    emit formChanged();
    emit coverChanged();
    emit dirtyChanged();
    if (errors.isEmpty())
        setStatus(tr("Unsaved changes discarded."));
    else
        setStatus(errors.join(QLatin1Char('\n')), true);
}

void Backend::setStatus(const QString &message, bool error)
{
    m_statusMessage = message;
    m_statusIsError = error;
    emit statusChanged();
}
