#include <QDir>
#include <QFile>
#include <QImage>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "filepicker.h"
#include "theme.h"
#include "translator.h"

class FakePicker : public FilePicker {
public:
    using FilePicker::FilePicker;
    void openDirectory() override {}
    void openFiles() override {}
    void openImage() override {}
};

class TagrTests : public QObject {
    Q_OBJECT

private slots:
    void mp3RoundTripPreservesAudioPropertiesAndCover()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("sample.mp3");
        const QString coverPath = dir.filePath("cover.png");
        QVERIFY(makeAudio(path, "libmp3lame"));
        QVERIFY(makeCover(coverPath));

        Track original;
        QString error;
        QVERIFY2(readTrack(path, original, error), qPrintable(error));
        Track edited = original;
        edited.title = QStringLiteral("Midnight Signal");
        edited.artist = QStringLiteral("Aurora Fields");
        edited.album = QStringLiteral("Test Patterns");
        edited.albumArtist = QStringLiteral("Aurora Fields");
        edited.genre = QStringLiteral("Electronic");
        edited.comment = QStringLiteral("Native metadata test");
        edited.year = 2026;
        edited.trackNumber = 3;
        edited.discNumber = 1;
        QVERIFY2(loadBytes(coverPath, edited.cover, error), qPrintable(error));
        edited.coverMime = QStringLiteral("image/png");
        edited.coverChanged = true;
        QVERIFY2(writeTrackAtomic(edited, error), qPrintable(error));

        Track saved;
        QVERIFY2(readTrack(path, saved, error), qPrintable(error));
        QCOMPARE(saved.title, edited.title);
        QCOMPARE(saved.artist, edited.artist);
        QCOMPARE(saved.album, edited.album);
        QCOMPARE(saved.albumArtist, edited.albumArtist);
        QCOMPARE(saved.genre, edited.genre);
        QCOMPARE(saved.comment, edited.comment);
        QCOMPARE(saved.year, 2026);
        QCOMPARE(saved.trackNumber, 3);
        QCOMPARE(saved.discNumber, 1);
        QCOMPARE(saved.cover, edited.cover);
        QCOMPARE(saved.coverMime, edited.coverMime);
        QCOMPARE(saved.sampleRate, original.sampleRate);
        QCOMPARE(saved.channels, original.channels);
        QCOMPARE(saved.durationSeconds, original.durationSeconds);
        QVERIFY(QDir(dir.path()).entryList({QStringLiteral(".*.tagr-*")}, QDir::Files).isEmpty());
    }

    void flacMp4AndOpusExtendedTagsRoundTrip()
    {
        QTemporaryDir dir;
        const QList<QPair<QString, QString>> formats = {
            {QStringLiteral("sample.flac"), QStringLiteral("flac")},
            {QStringLiteral("sample.m4a"), QStringLiteral("aac")},
            {QStringLiteral("sample.opus"), QStringLiteral("libopus")},
        };
        for (const auto &[name, codec] : formats) {
            const QString path = dir.filePath(name);
            QVERIFY2(makeAudio(path, codec), qPrintable(path));
            Track track;
            QString error;
            QVERIFY2(readTrack(path, track, error), qPrintable(error));
            track.albumArtist = QStringLiteral("Various Artists");
            track.discNumber = 2;
            track.title = QStringLiteral("Format Check");
            track.musicbrainzReleaseId = QStringLiteral("9a8b7c6d-1111-2222-3333-444455556666");
            track.musicbrainzAlbumId = QStringLiteral("7f6e5d4c-1111-2222-3333-444455556666");
            track.musicbrainzAlbumArtistId = QStringLiteral("6c5b4a39-1111-2222-3333-444455556666");
            track.musicbrainzArtistId = QStringLiteral("5a4938f0-1111-2222-3333-444455556666");
            track.musicbrainzTrackId = QStringLiteral("4b1d0f2a-1111-2222-3333-444455556666");
            track.musicbrainzReleaseGroupId = QStringLiteral("3a29180f-1111-2222-3333-444455556666");
            QVERIFY2(writeTrackAtomic(track, error), qPrintable(error));
            Track saved;
            QVERIFY2(readTrack(path, saved, error), qPrintable(error));
            QCOMPARE(saved.albumArtist, QStringLiteral("Various Artists"));
            QCOMPARE(saved.discNumber, 2);
            QCOMPARE(saved.title, QStringLiteral("Format Check"));
            QCOMPARE(saved.musicbrainzReleaseId, QStringLiteral("9a8b7c6d-1111-2222-3333-444455556666"));
            QCOMPARE(saved.musicbrainzAlbumId, QStringLiteral("7f6e5d4c-1111-2222-3333-444455556666"));
            QCOMPARE(saved.musicbrainzAlbumArtistId, QStringLiteral("6c5b4a39-1111-2222-3333-444455556666"));
            QCOMPARE(saved.musicbrainzArtistId, QStringLiteral("5a4938f0-1111-2222-3333-444455556666"));
            QCOMPARE(saved.musicbrainzTrackId, QStringLiteral("4b1d0f2a-1111-2222-3333-444455556666"));
            QCOMPARE(saved.musicbrainzReleaseGroupId, QStringLiteral("3a29180f-1111-2222-3333-444455556666"));
        }
    }

    void invalidWriteLeavesOriginalBytesUntouched()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("invalid.mp3");
        const QByteArray original = "not an audio stream";
        QVERIFY(writeBytes(path, original));
        Track track;
        track.path = path;
        track.title = QStringLiteral("Must not be written");
        QString error;
        QVERIFY(!writeTrackAtomic(track, error));
        QVERIFY(!error.isEmpty());
        QByteArray after;
        QVERIFY(readBytes(path, after, error));
        QCOMPARE(after, original);
    }

    void backendFiltersSelectsAndSorts()
    {
        QTemporaryDir dir;
        QVERIFY(makeTaggedAudio(dir.filePath("c.mp3"), "Charlie", "Artist C", 3));
        QVERIFY(makeTaggedAudio(dir.filePath("a.mp3"), "Alpha", "Artist A", 1));
        QVERIFY(makeTaggedAudio(dir.filePath("b.mp3"), "Bravo", "Artist B", 2));
        Backend backend(new FakePicker, nullptr, dir.filePath("settings.ini"));
        backend.openPaths({dir.path()});
        QTRY_VERIFY(!backend.loading());
        QCOMPARE(backend.trackCount(), 3);
        QCOMPARE(backend.selectedCount(), 1);
        QCOMPARE(backend.title(), QStringLiteral("Alpha"));

        backend.setFilterText(QStringLiteral("bravo"));
        QCOMPARE(backend.visibleCount(), 1);
        backend.selectAll();
        QCOMPARE(backend.selectedCount(), 1);
        QCOMPARE(backend.title(), QStringLiteral("Bravo"));
        backend.setFilterText(QString());
        backend.selectAll();
        QCOMPARE(backend.selectedCount(), 3);
        backend.sortBy(QStringLiteral("trackNumber"));
        backend.selectRow(0);
        QCOMPARE(backend.title(), QStringLiteral("Alpha"));
        backend.selectRow(2, false, true);
        QCOMPARE(backend.selectedCount(), 3);
        backend.sortBy(QStringLiteral("trackNumber"));
        backend.selectRow(0);
        QCOMPARE(backend.title(), QStringLiteral("Charlie"));
    }

    void backendMultiEditUndoRedoAndSave()
    {
        QTemporaryDir dir;
        const QString first = dir.filePath("first.mp3"), second = dir.filePath("second.mp3");
        QVERIFY(makeTaggedAudio(first, "One", "Artist A", 1));
        QVERIFY(makeTaggedAudio(second, "Two", "Artist B", 2));
        Backend backend(new FakePicker, nullptr, dir.filePath("settings.ini"));
        backend.openPaths({dir.path()});
        QTRY_VERIFY(!backend.loading());
        backend.selectAll();
        QVERIFY(backend.fieldMixed(QStringLiteral("title")));
        QVERIFY(backend.fieldMixed(QStringLiteral("artist")));
        QCOMPARE(backend.fieldValues(QStringLiteral("title")), QStringList({QStringLiteral("One"), QStringLiteral("Two")}));
        QCOMPARE(backend.fieldValues(QStringLiteral("artist")), QStringList({QStringLiteral("Artist A"), QStringLiteral("Artist B")}));

        backend.setField(QStringLiteral("artist"), QStringLiteral("Shared Artist"));
        QCOMPARE(backend.dirtyCount(), 2);
        QCOMPARE(backend.artist(), QStringLiteral("Shared Artist"));
        QVERIFY(!backend.fieldMixed(QStringLiteral("artist")));
        QVERIFY(backend.canUndo());
        backend.undo();
        QCOMPARE(backend.dirtyCount(), 0);
        QVERIFY(backend.fieldMixed(QStringLiteral("artist")));
        QVERIFY(backend.canRedo());
        backend.redo();
        QCOMPARE(backend.dirtyCount(), 2);
        QCOMPARE(backend.artist(), QStringLiteral("Shared Artist"));
        QVERIFY2(backend.save(), qPrintable(backend.statusMessage()));
        QCOMPARE(backend.dirtyCount(), 0);

        for (const QString &path : {first, second}) {
            Track saved;
            QString error;
            QVERIFY2(readTrack(path, saved, error), qPrintable(error));
            QCOMPARE(saved.artist, QStringLiteral("Shared Artist"));
            QCOMPARE(saved.title, path == first ? QStringLiteral("One") : QStringLiteral("Two"));
        }
    }

    void backendCoverUndoRedoAndDiscard()
    {
        QTemporaryDir dir;
        const QString audio = dir.filePath("cover.mp3"), image = dir.filePath("cover.png");
        QVERIFY(makeTaggedAudio(audio, "Cover", "Artist", 1));
        QVERIFY(makeCover(image));
        Backend backend(new FakePicker, nullptr, dir.filePath("settings.ini"));
        backend.openPaths({audio});
        QTRY_VERIFY(!backend.loading());
        QVERIFY(!backend.hasCover());
        QVERIFY2(backend.setCoverFromFile(image), qPrintable(backend.statusMessage()));
        QVERIFY(backend.hasCover());
        QCOMPARE(backend.dirtyCount(), 1);
        backend.undo();
        QVERIFY(!backend.hasCover());
        QCOMPARE(backend.dirtyCount(), 0);
        backend.redo();
        QVERIFY(backend.hasCover());
        backend.discardChanges();
        QVERIFY(!backend.hasCover());
        QCOMPARE(backend.dirtyCount(), 0);
    }

    void backendDirectoryHistoryPersistsAndValidatesPaths()
    {
        QTemporaryDir dir;
        const QString one = dir.filePath("one"), two = dir.filePath("two");
        QVERIFY(QDir(dir.path()).mkpath(one));
        QVERIFY(QDir(dir.path()).mkpath(two));
        QVERIFY(makeTaggedAudio(dir.filePath("one/a.mp3"), "One", "Artist", 1));
        QVERIFY(makeTaggedAudio(dir.filePath("two/b.mp3"), "Two", "Artist", 2));
        const QString settings = dir.filePath("settings.ini");

        Backend first(new FakePicker, nullptr, settings);
        first.openPaths({one});
        QTRY_VERIFY(!first.loading());
        QCOMPARE(first.recentDirectories(), QStringList({one}));
        first.openPaths({two});
        QTRY_VERIFY(!first.loading());
        QCOMPARE(first.recentDirectories(), QStringList({two, one}));

        Backend second(new FakePicker, nullptr, settings);
        QCOMPARE(second.recentDirectories(), QStringList({two, one}));
        second.openDirectoryFromEditor(dir.filePath("missing"));
        QVERIFY(second.statusIsError());
        QCOMPARE(second.trackCount(), 0);
        second.openDirectoryFromEditor(one);
        QTRY_VERIFY(!second.loading());
        QCOMPARE(second.trackCount(), 1);
        QCOMPARE(second.directory(), one);
    }

    void backendColumnVisibilityPersistsAcrossInstances()
    {
        QTemporaryDir dir;
        const QString settings = dir.filePath("settings.ini");
        Backend backend(new FakePicker, nullptr, settings);
        QVERIFY(backend.showPathColumn());
        QVERIFY(backend.showTitleColumn());
        QVERIFY(backend.showArtistColumn());
        QVERIFY(!backend.showAlbumArtistColumn());
        QVERIFY(backend.showAlbumColumn());
        QVERIFY(!backend.showGenreColumn());
        QVERIFY(backend.showTrackColumn());
        QVERIFY(backend.showDurationColumn());
        QVERIFY(backend.showFormatColumn());

        backend.setColumnVisible(QStringLiteral("title"), false);
        backend.setColumnVisible(QStringLiteral("format"), false);
        backend.setColumnVisible(QStringLiteral("genre"), true);
        QVERIFY(!backend.showTitleColumn());
        QVERIFY(!backend.showFormatColumn());
        QVERIFY(backend.showGenreColumn());

        Backend second(new FakePicker, nullptr, settings);
        QVERIFY(!second.showTitleColumn());
        QVERIFY(!second.showFormatColumn());
        QVERIFY(second.showGenreColumn());
        QVERIFY(second.showPathColumn());
        QVERIFY(second.showArtistColumn());
        QVERIFY(!second.showAlbumArtistColumn());
    }

    void backendMusicBrainzFieldsRoundTripThroughEditorFields()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("picard.mp3");
        QVERIFY(makeTaggedAudio(path, QStringLiteral("Picard Song"), QStringLiteral("Picard Artist"), 7));
        Backend backend(new FakePicker, nullptr, dir.filePath("settings.ini"));
        backend.openPaths({dir.path()});
        QTRY_VERIFY(!backend.loading());

        backend.setField(QStringLiteral("musicbrainzReleaseId"), QStringLiteral("9a8b7c6d-1111-2222-3333-444455556666"));
        backend.setField(QStringLiteral("musicbrainzAlbumId"), QStringLiteral("7f6e5d4c-1111-2222-3333-444455556666"));
        backend.setField(QStringLiteral("musicbrainzAlbumArtistId"), QStringLiteral("6c5b4a39-1111-2222-3333-444455556666"));
        backend.setField(QStringLiteral("musicbrainzArtistId"), QStringLiteral("5a4938f0-1111-2222-3333-444455556666"));
        backend.setField(QStringLiteral("musicbrainzTrackId"), QStringLiteral("4b1d0f2a-1111-2222-3333-444455556666"));
        backend.setField(QStringLiteral("musicbrainzReleaseGroupId"), QStringLiteral("3a29180f-1111-2222-3333-444455556666"));
        QVERIFY(backend.save());

        Track saved;
        QString error;
        QVERIFY2(readTrack(path, saved, error), qPrintable(error));
        QCOMPARE(saved.musicbrainzReleaseId, QStringLiteral("9a8b7c6d-1111-2222-3333-444455556666"));
        QCOMPARE(saved.musicbrainzAlbumId, QStringLiteral("7f6e5d4c-1111-2222-3333-444455556666"));
        QCOMPARE(saved.musicbrainzAlbumArtistId, QStringLiteral("6c5b4a39-1111-2222-3333-444455556666"));
        QCOMPARE(saved.musicbrainzArtistId, QStringLiteral("5a4938f0-1111-2222-3333-444455556666"));
        QCOMPARE(saved.musicbrainzTrackId, QStringLiteral("4b1d0f2a-1111-2222-3333-444455556666"));
        QCOMPARE(saved.musicbrainzReleaseGroupId, QStringLiteral("3a29180f-1111-2222-3333-444455556666"));
        QCOMPARE(backend.musicbrainzReleaseId(), QStringLiteral("9a8b7c6d-1111-2222-3333-444455556666"));
    }

    void themeFallsBackAndReadsAccent()
    {
        QTemporaryDir dir;
        Theme fallback(dir.path());
        QCOMPARE(fallback.accent(), QString("#FFD60A"));
        QCOMPARE(fallback.accentForeground(), QString("black"));

        QDir(dir.path()).mkpath("theme");
        QVERIFY(writeBytes(dir.filePath("theme/colors.toml"), "accent = \"#112233\"\n"));
        Theme themed(dir.path());
        QCOMPARE(themed.accent(), QString("#112233"));
        QCOMPARE(themed.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        QVERIFY(writeBytes(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n"));
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        QVERIFY(writeBytes(response, "{\"int\":0}"));
        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        QVERIFY(writeBytes(response, "{\"int\":6}"));
        QTRY_COMPARE(theme.radius(), 6);
    }

    void translatorFallsBackAndUsesRegionalCatalog()
    {
        QTemporaryDir dir;
        Translator fallback(dir.filePath("language"), dir.path());
        QCOMPARE(fallback.language(), QString("en"));
        QVERIFY(fallback.translate("Main", "Save").isNull());

        QVERIFY(writeBytes(dir.filePath("language"), "pt-BR\n"));
        QVERIFY(writeBytes(dir.filePath("pt.json"), R"({"Save": "Guardar", "Open": "Abrir"})"));
        QVERIFY(writeBytes(dir.filePath("pt-BR.json"), R"({"Save": "Salvar"})"));
        Translator translator(dir.filePath("language"), dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        QVERIFY(writeBytes(dir.filePath("language"), "ru\n"));
        QVERIFY(writeBytes(dir.filePath("en.json"), R"({"%n tracks": {"one": "%n track", "other": "%n tracks"}})"));
        QVERIFY(writeBytes(dir.filePath("ru.json"), R"({"%n tracks": {"one": "%n трек", "few": "%n трека", "many": "%n треков", "other": "%n трека"}})"));
        Translator translator(dir.filePath("language"), dir.path());
        QCOMPARE(translator.translate("Main", "%n tracks", nullptr, 21), QString("%n трек"));
        QCOMPARE(translator.translate("Main", "%n tracks", nullptr, 22), QString("%n трека"));
        QCOMPARE(translator.translate("Main", "%n tracks", nullptr, 25), QString("%n треков"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static bool writeBytes(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }

    static bool readBytes(const QString &path, QByteArray &data, QString &error)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = file.errorString();
            return false;
        }
        data = file.readAll();
        return true;
    }

    static bool loadBytes(const QString &path, QByteArray &data, QString &error)
    {
        return readBytes(path, data, error);
    }

    static bool makeAudio(const QString &path, const QString &codec)
    {
        QProcess process;
        process.start(QStringLiteral("ffmpeg"), {
            QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y"),
            QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
            QStringLiteral("sine=frequency=440:duration=0.08"),
            QStringLiteral("-c:a"), codec, path,
        });
        return process.waitForStarted() && process.waitForFinished(10000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    static bool makeTaggedAudio(const QString &path, const QString &title, const QString &artist, int number)
    {
        if (!makeAudio(path, QStringLiteral("libmp3lame")))
            return false;
        Track track;
        QString error;
        if (!readTrack(path, track, error))
            return false;
        track.title = title;
        track.artist = artist;
        track.trackNumber = number;
        return writeTrackAtomic(track, error);
    }

    static bool makeCover(const QString &path)
    {
        QImage image(24, 24, QImage::Format_RGB32);
        image.fill(QColor(QStringLiteral("#FFD60A")));
        return image.save(path, "PNG");
    }
};

QTEST_MAIN(TagrTests)
#include "tagr_tests.moc"
