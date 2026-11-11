#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QSortFilterProxyModel>
#include <QStringList>

#include "track.h"
#include "trackmodel.h"

class FilePicker;

struct LoadResult {
    QList<Track> tracks;
    QStringList errors;
};

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *model READ model CONSTANT)
    Q_PROPERTY(QString directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(QStringList recentDirectories READ recentDirectories NOTIFY recentDirectoriesChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(int trackCount READ trackCount NOTIFY tracksChanged)
    Q_PROPERTY(int visibleCount READ visibleCount NOTIFY tracksChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY dirtyChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool statusIsError READ statusIsError NOTIFY statusChanged)
    Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY sortChanged)
    Q_PROPERTY(int sortOrder READ sortOrder NOTIFY sortChanged)
    Q_PROPERTY(int formRevision READ formRevision NOTIFY formChanged)
    Q_PROPERTY(QString title READ title NOTIFY formChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY formChanged)
    Q_PROPERTY(QString album READ album NOTIFY formChanged)
    Q_PROPERTY(QString albumArtist READ albumArtist NOTIFY formChanged)
    Q_PROPERTY(QString genre READ genre NOTIFY formChanged)
    Q_PROPERTY(QString comment READ comment NOTIFY formChanged)
    Q_PROPERTY(QString yearText READ yearText NOTIFY formChanged)
    Q_PROPERTY(QString trackText READ trackText NOTIFY formChanged)
    Q_PROPERTY(QString discText READ discText NOTIFY formChanged)
    Q_PROPERTY(int coverRevision READ coverRevision NOTIFY coverChanged)
    Q_PROPERTY(bool hasCover READ hasCover NOTIFY coverChanged)
    Q_PROPERTY(QString technicalInfo READ technicalInfo NOTIFY formChanged)
    Q_PROPERTY(QString coverInfo READ coverInfo NOTIFY coverChanged)
    Q_PROPERTY(bool showPathColumn READ showPathColumn NOTIFY showPathColumnChanged)
    Q_PROPERTY(bool showTitleColumn READ showTitleColumn NOTIFY showTitleColumnChanged)
    Q_PROPERTY(bool showArtistColumn READ showArtistColumn NOTIFY showArtistColumnChanged)
    Q_PROPERTY(bool showAlbumArtistColumn READ showAlbumArtistColumn NOTIFY showAlbumArtistColumnChanged)
    Q_PROPERTY(bool showAlbumColumn READ showAlbumColumn NOTIFY showAlbumColumnChanged)
    Q_PROPERTY(bool showGenreColumn READ showGenreColumn NOTIFY showGenreColumnChanged)
    Q_PROPERTY(bool showTrackColumn READ showTrackColumn NOTIFY showTrackColumnChanged)
    Q_PROPERTY(bool showDurationColumn READ showDurationColumn NOTIFY showDurationColumnChanged)
    Q_PROPERTY(bool showFormatColumn READ showFormatColumn NOTIFY showFormatColumnChanged)

public:
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr, const QString &settingsFile = {});
    ~Backend() override;

    QAbstractItemModel *model() { return m_proxy; }
    QString directory() const { return m_directory; }
    QStringList recentDirectories() const { return m_recentDirectories; }
    QString filterText() const;
    void setFilterText(const QString &text);
    int trackCount() const { return m_model.rowCount(); }
    int visibleCount() const { return m_proxy->rowCount(); }
    int selectedCount() const;
    int dirtyCount() const;
    bool loading() const { return m_loading; }
    bool canUndo() const { return m_historyIndex >= 0; }
    bool canRedo() const { return m_historyIndex + 1 < m_history.size(); }
    QString statusMessage() const { return m_statusMessage; }
    bool statusIsError() const { return m_statusIsError; }
    QString sortColumn() const { return m_sortColumn; }
    int sortOrder() const { return static_cast<int>(m_proxy->sortOrder()); }
    int formRevision() const { return m_formRevision; }
    int coverRevision() const { return m_coverRevision; }

    QString title() const { return fieldValue(QStringLiteral("title")); }
    QString artist() const { return fieldValue(QStringLiteral("artist")); }
    QString album() const { return fieldValue(QStringLiteral("album")); }
    QString albumArtist() const { return fieldValue(QStringLiteral("albumArtist")); }
    QString genre() const { return fieldValue(QStringLiteral("genre")); }
    QString comment() const { return fieldValue(QStringLiteral("comment")); }
    QString yearText() const;
    QString trackText() const;
    QString discText() const;
    bool hasCover() const;
    QString technicalInfo() const;
    QString coverInfo() const;
    QImage currentCoverImage() const;

    Q_INVOKABLE QString fieldValue(const QString &field) const;
    Q_INVOKABLE QStringList fieldValues(const QString &field) const;
    Q_INVOKABLE bool fieldMixed(const QString &field) const;
    Q_INVOKABLE void setField(const QString &field, const QString &value);
    Q_INVOKABLE void selectRow(int row, bool control = false, bool shift = false);
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void sortBy(const QString &column);
    Q_INVOKABLE void openPaths(const QStringList &paths);
    Q_INVOKABLE void openDirectoryFromEditor(const QString &path);
    Q_INVOKABLE void chooseDirectory();
    Q_INVOKABLE void chooseFiles();
    Q_INVOKABLE void chooseCover();
    Q_INVOKABLE bool setCoverFromFile(const QString &path);
    Q_INVOKABLE void removeCover();
    Q_INVOKABLE bool save();
    Q_INVOKABLE void discardChanges();
    Q_INVOKABLE void clearStatus() { setStatus(QString()); }
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

    Q_INVOKABLE void playPath(const QString &path);
    Q_INVOKABLE bool renameTrackFile(const QString &oldPath, const QString &newPath);
    Q_INVOKABLE QString firstSelectedPath() const;

    Q_INVOKABLE bool columnVisible(const QString &column, bool fallback) const;
    Q_INVOKABLE void setColumnVisible(const QString &column, bool visible);

    Q_INVOKABLE QString musicbrainzReleaseId() const;
    Q_INVOKABLE QString musicbrainzAlbumId() const;
    Q_INVOKABLE QString musicbrainzAlbumArtistId() const;
    Q_INVOKABLE QString musicbrainzArtistId() const;
    Q_INVOKABLE QString musicbrainzTrackId() const;
    Q_INVOKABLE QString musicbrainzReleaseGroupId() const;

    bool showPathColumn() const;
    bool showTitleColumn() const;
    bool showArtistColumn() const;
    bool showAlbumArtistColumn() const;
    bool showAlbumColumn() const;
    bool showGenreColumn() const;
    bool showTrackColumn() const;
    bool showDurationColumn() const;
    bool showFormatColumn() const;

signals:
    void directoryChanged();
    void recentDirectoriesChanged();
    void filterChanged();
    void tracksChanged();
    void selectionChanged();
    void dirtyChanged();
    void loadingChanged();
    void historyChanged();
    void statusChanged();
    void sortChanged();
    void formChanged();
    void coverChanged();
    void showPathColumnChanged();
    void showTitleColumnChanged();
    void showArtistColumnChanged();
    void showAlbumArtistColumnChanged();
    void showAlbumColumnChanged();
    void showGenreColumnChanged();
    void showTrackColumnChanged();
    void showDurationColumnChanged();
    void showFormatColumnChanged();

private:
    struct HistoryEntry {
        QList<Track> before;
        QList<Track> after;
    };

    const Track *firstSelected() const;
    QList<Track> selectedTracks() const;
    void finishLoading();
    void updateDirty(Track &track) const;
    void notifySelection();
    void setStatus(const QString &message, bool error = false);
    void pushHistory(const QList<Track> &before);
    void applyHistory(const QList<Track> &tracks);
    void clearHistory();
    int sourceRowForVisibleRow(int row) const;
    void reloadSavedTrack(const Track &track);
    void loadRecentDirectories();
    void rememberDirectory(const QString &directory);
    void loadColumnVisibility();
    void persistColumnVisibility();
    bool columnVisibleDefault(const QString &column) const;

    TrackModel m_model;
    QSortFilterProxyModel *m_proxy = nullptr;
    FilePicker *m_picker = nullptr;
    QFutureWatcher<LoadResult> m_loadWatcher;
    QStringList m_sourcePaths;
    QString m_directory;
    QStringList m_recentDirectories;
    QString m_settingsFile;
    QString m_statusMessage;
    bool m_statusIsError = false;
    bool m_loading = false;
    QHash<QString, Track> m_clean;
    QString m_anchorPath;
    QString m_sortColumn = QStringLiteral("fileName");
    QList<HistoryEntry> m_history;
    int m_historyIndex = -1;
    int m_formRevision = 0;
    int m_coverRevision = 0;
    QHash<QString, bool> m_columnVisibility;
};
