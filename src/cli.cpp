#include "cli.h"

#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QObject>
#include <QSaveFile>
#include <QTextStream>

#include "track.h"

namespace {
QTextStream &errorStream()
{
    static QTextStream stream(stderr);
    return stream;
}

QTextStream &outputStream()
{
    static QTextStream stream(stdout);
    return stream;
}

bool parseOptions(QCommandLineParser &parser, const QStringList &arguments, bool &help)
{
    if (!parser.parse(arguments)) {
        errorStream() << parser.errorText() << Qt::endl;
        return false;
    }
    help = parser.isSet(QStringLiteral("help"));
    return true;
}

void addMetadataOptions(QCommandLineParser &parser)
{
    parser.addOptions({
        {{QStringLiteral("title")}, QObject::tr("Set the title."), QObject::tr("value")},
        {{QStringLiteral("artist")}, QObject::tr("Set the artist."), QObject::tr("value")},
        {{QStringLiteral("album")}, QObject::tr("Set the album."), QObject::tr("value")},
        {{QStringLiteral("album-artist")}, QObject::tr("Set the album artist."), QObject::tr("value")},
        {{QStringLiteral("genre")}, QObject::tr("Set the genre."), QObject::tr("value")},
        {{QStringLiteral("comment")}, QObject::tr("Set the comment."), QObject::tr("value")},
        {{QStringLiteral("year")}, QObject::tr("Set the year (0 clears it)."), QObject::tr("number")},
        {{QStringLiteral("track")}, QObject::tr("Set the track number (0 clears it)."), QObject::tr("number")},
        {{QStringLiteral("disc")}, QObject::tr("Set the disc number (0 clears it)."), QObject::tr("number")},
        {{QStringLiteral("cover")}, QObject::tr("Set cover art from a JPEG or PNG file."), QObject::tr("path")},
        {{QStringLiteral("remove-cover")}, QObject::tr("Remove embedded cover art.")},
    });
}

bool numberOption(const QCommandLineParser &parser, const QString &name, int &value)
{
    if (!parser.isSet(name))
        return true;
    bool ok = false;
    value = parser.value(name).toInt(&ok);
    if (!ok || value < 0 || value > 9999) {
        errorStream() << QObject::tr("%1 must be a whole number from 0 to 9999.").arg(name) << Qt::endl;
        return false;
    }
    return true;
}

bool loadCover(const QString &path, QByteArray &bytes, QString &mime, QString &error)
{
    const QFileInfo info(path);
    if (!info.isFile() || info.size() > 20 * 1024 * 1024) {
        error = QObject::tr("Choose a JPEG or PNG image smaller than 20 MB.");
        return false;
    }
    QImageReader reader(path);
    const QByteArray format = reader.imageFormat(path).toLower();
    if (format != "jpeg" && format != "jpg" && format != "png") {
        error = QObject::tr("Cover art must be a JPEG or PNG image.");
        return false;
    }
    if (reader.read().isNull()) {
        error = QObject::tr("Could not decode the cover image: %1").arg(reader.errorString());
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not read the cover image: %1").arg(file.errorString());
        return false;
    }
    bytes = file.readAll();
    mime = format == "png" ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
    return true;
}

int readCommand(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Read audio metadata without opening the Tagr window."));
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("json")}, QObject::tr("Write machine-readable JSON.")});
    parser.addPositionalArgument(QStringLiteral("path"), QObject::tr("Audio files to read."), QObject::tr("path..."));
    bool help = false;
    if (!parseOptions(parser, arguments, help))
        return 1;
    if (help) {
        parser.showHelp();
        return 0;
    }
    const QStringList paths = parser.positionalArguments();
    if (paths.isEmpty()) {
        errorStream() << QObject::tr("Provide at least one audio file.") << Qt::endl;
        return 1;
    }
    QJsonArray results;
    bool failed = false;
    for (const QString &path : paths) {
        Track track;
        QString error;
        if (!readTrack(path, track, error)) {
            errorStream() << error << Qt::endl;
            if (parser.isSet(QStringLiteral("json")))
                results.append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("error"), error}});
            failed = true;
            continue;
        }
        if (parser.isSet(QStringLiteral("json"))) {
            results.append(trackToJson(track));
        } else {
            outputStream() << track.path << Qt::endl;
            outputStream() << QObject::tr("Title: %1").arg(track.title) << Qt::endl;
            outputStream() << QObject::tr("Artist: %1").arg(track.artist) << Qt::endl;
            outputStream() << QObject::tr("Album: %1").arg(track.album) << Qt::endl;
            outputStream() << QObject::tr("Album artist: %1").arg(track.albumArtist) << Qt::endl;
            outputStream() << QObject::tr("Genre: %1").arg(track.genre) << Qt::endl;
            outputStream() << QObject::tr("Year: %1").arg(track.year) << Qt::endl;
            outputStream() << QObject::tr("Track: %1").arg(track.trackNumber) << Qt::endl;
            outputStream() << QObject::tr("Disc: %1").arg(track.discNumber) << Qt::endl;
            outputStream() << QObject::tr("Comment: %1").arg(track.comment) << Qt::endl;
        }
    }
    if (parser.isSet(QStringLiteral("json")))
        outputStream() << QJsonDocument(results).toJson(QJsonDocument::Compact) << Qt::endl;
    return failed ? 1 : 0;
}

int writeCommand(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Write audio metadata atomically without opening the Tagr window."));
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("json")}, QObject::tr("Write machine-readable JSON.")});
    addMetadataOptions(parser);
    parser.addPositionalArgument(QStringLiteral("path"), QObject::tr("Audio files to update."), QObject::tr("path..."));
    bool help = false;
    if (!parseOptions(parser, arguments, help))
        return 1;
    if (help) {
        parser.showHelp();
        return 0;
    }
    const QStringList paths = parser.positionalArguments();
    static const QStringList options = {
        QStringLiteral("title"), QStringLiteral("artist"), QStringLiteral("album"),
        QStringLiteral("album-artist"), QStringLiteral("genre"), QStringLiteral("comment"),
        QStringLiteral("year"), QStringLiteral("track"), QStringLiteral("disc"),
        QStringLiteral("cover"), QStringLiteral("remove-cover"),
    };
    bool anyEdit = false;
    for (const auto &option : options)
        anyEdit = anyEdit || parser.isSet(option);
    if (paths.isEmpty() || !anyEdit) {
        errorStream() << QObject::tr("Provide at least one audio file and one metadata option.") << Qt::endl;
        return 1;
    }
    if (parser.isSet(QStringLiteral("cover")) && parser.isSet(QStringLiteral("remove-cover"))) {
        errorStream() << QObject::tr("Use either --cover or --remove-cover, not both.") << Qt::endl;
        return 1;
    }
    int year = 0, trackNumber = 0, discNumber = 0;
    if (!numberOption(parser, QStringLiteral("year"), year)
        || !numberOption(parser, QStringLiteral("track"), trackNumber)
        || !numberOption(parser, QStringLiteral("disc"), discNumber))
        return 1;
    QByteArray cover;
    QString coverMime, coverError;
    if (parser.isSet(QStringLiteral("cover"))
        && !loadCover(parser.value(QStringLiteral("cover")), cover, coverMime, coverError)) {
        errorStream() << coverError << Qt::endl;
        return 1;
    }

    QJsonArray results;
    bool failed = false;
    for (const QString &path : paths) {
        Track track;
        QString error;
        if (!readTrack(path, track, error)) {
            failed = true;
            errorStream() << error << Qt::endl;
            if (parser.isSet(QStringLiteral("json")))
                results.append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("saved"), false}, {QStringLiteral("error"), error}});
            continue;
        }
        if (parser.isSet(QStringLiteral("title"))) track.title = parser.value(QStringLiteral("title"));
        if (parser.isSet(QStringLiteral("artist"))) track.artist = parser.value(QStringLiteral("artist"));
        if (parser.isSet(QStringLiteral("album"))) track.album = parser.value(QStringLiteral("album"));
        if (parser.isSet(QStringLiteral("album-artist"))) track.albumArtist = parser.value(QStringLiteral("album-artist"));
        if (parser.isSet(QStringLiteral("genre"))) track.genre = parser.value(QStringLiteral("genre"));
        if (parser.isSet(QStringLiteral("comment"))) track.comment = parser.value(QStringLiteral("comment"));
        if (parser.isSet(QStringLiteral("year"))) track.year = year;
        if (parser.isSet(QStringLiteral("track"))) track.trackNumber = trackNumber;
        if (parser.isSet(QStringLiteral("disc"))) track.discNumber = discNumber;
        if (parser.isSet(QStringLiteral("cover"))) {
            track.cover = cover;
            track.coverMime = coverMime;
            track.coverChanged = true;
        }
        if (parser.isSet(QStringLiteral("remove-cover"))) {
            track.cover.clear();
            track.coverMime.clear();
            track.coverChanged = true;
        }
        if (!writeTrackAtomic(track, error)) {
            failed = true;
            errorStream() << error << Qt::endl;
            if (parser.isSet(QStringLiteral("json")))
                results.append(QJsonObject{{QStringLiteral("path"), path}, {QStringLiteral("saved"), false}, {QStringLiteral("error"), error}});
            continue;
        }
        Track saved;
        if (!readTrack(path, saved, error))
            saved = track;
        auto object = trackToJson(saved);
        object.insert(QStringLiteral("saved"), true);
        if (parser.isSet(QStringLiteral("json")))
            results.append(object);
        else
            outputStream() << QObject::tr("Saved %1").arg(path) << Qt::endl;
    }
    if (parser.isSet(QStringLiteral("json")))
        outputStream() << QJsonDocument(results).toJson(QJsonDocument::Compact) << Qt::endl;
    return failed ? 1 : 0;
}

int skillCommand(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Install the bundled Tagr agent skill."));
    parser.addHelpOption();
    parser.addOption({{QStringLiteral("json")}, QObject::tr("Write machine-readable JSON.")});
    parser.addOption({{QStringLiteral("path")}, QObject::tr("Install into this skill directory instead of ~/.agents/skills/tagr."),
                      QObject::tr("directory")});
    bool help = false;
    if (!parseOptions(parser, arguments, help))
        return 1;
    if (help) {
        parser.showHelp();
        return 0;
    }
    if (parser.positionalArguments().size() != 1
        || parser.positionalArguments().first() != QStringLiteral("install")) {
        errorStream() << QObject::tr("Usage: tagr skill install [--json] [--path directory]") << Qt::endl;
        return 1;
    }
    const QString directory = parser.isSet(QStringLiteral("path"))
        ? QFileInfo(parser.value(QStringLiteral("path"))).absoluteFilePath()
        : QDir::homePath() + QStringLiteral("/.agents/skills/tagr");
    if (!QDir().mkpath(directory)) {
        errorStream() << QObject::tr("Could not create %1.").arg(directory) << Qt::endl;
        return 1;
    }
    QFile bundled(QStringLiteral(":/skill/SKILL.md"));
    if (!bundled.open(QIODevice::ReadOnly)) {
        errorStream() << QObject::tr("The bundled skill could not be read.") << Qt::endl;
        return 1;
    }
    const QString target = directory + QStringLiteral("/SKILL.md");
    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly) || output.write(bundled.readAll()) < 0 || !output.commit()) {
        errorStream() << QObject::tr("Could not install the skill: %1").arg(output.errorString()) << Qt::endl;
        return 1;
    }
    if (parser.isSet(QStringLiteral("json"))) {
        outputStream() << QJsonDocument(QJsonObject{
            {QStringLiteral("skill"), QStringLiteral("tagr")},
            {QStringLiteral("installed"), true},
            {QStringLiteral("path"), target},
        }).toJson(QJsonDocument::Compact) << Qt::endl;
    } else {
        outputStream() << QObject::tr("Installed %1").arg(target) << Qt::endl;
    }
    return 0;
}
}

void printCliUsage()
{
    outputStream() << QObject::tr("Usage:") << Qt::endl;
    outputStream() << QStringLiteral("  tagr open [audio-file-or-folder]") << Qt::endl;
    outputStream() << QStringLiteral("  tagr read [--json] path...") << Qt::endl;
    outputStream() << QStringLiteral("  tagr write [--json] [--title value] [--artist value] [options] path...") << Qt::endl;
    outputStream() << QStringLiteral("  tagr skill install [--json] [--path directory]") << Qt::endl;
}

int runCli(const QStringList &arguments)
{
    const QString command = arguments.value(1);
    if (command == QStringLiteral("read"))
        return readCommand(QStringList{QStringLiteral("read")} + arguments.mid(2));
    if (command == QStringLiteral("write"))
        return writeCommand(QStringList{QStringLiteral("write")} + arguments.mid(2));
    if (command == QStringLiteral("skill"))
        return skillCommand(QStringList{QStringLiteral("skill")} + arguments.mid(2));
    if (command.isEmpty() || command == QStringLiteral("--help") || command == QStringLiteral("-h")
        || command == QStringLiteral("help")) {
        printCliUsage();
        return 0;
    }
    errorStream() << QObject::tr("Unknown command: %1").arg(command) << Qt::endl;
    printCliUsage();
    return 1;
}
