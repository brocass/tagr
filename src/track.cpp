#include "track.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryFile>

#include <taglib/aifffile.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/tag.h>
#include <taglib/textidentificationframe.h>
#include <taglib/tpropertymap.h>
#include <taglib/wavfile.h>
#include <taglib/xiphcomment.h>

#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {
QString fromTag(const TagLib::String &value)
{
    const auto utf8 = value.to8Bit(true);
    return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
}

TagLib::String toTag(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
}

QByteArray fromBytes(const TagLib::ByteVector &value)
{
    return QByteArray(value.data(), static_cast<qsizetype>(value.size()));
}

TagLib::ByteVector toBytes(const QByteArray &value)
{
    return TagLib::ByteVector(value.constData(), static_cast<unsigned int>(value.size()));
}

QString firstProperty(const TagLib::PropertyMap &properties, const char *key)
{
    const auto values = properties.value(key);
    return values.isEmpty() ? QString() : fromTag(values.front());
}

int numberProperty(const TagLib::PropertyMap &properties, const char *key)
{
    return firstProperty(properties, key).section('/', 0, 0).toInt();
}

TagLib::ID3v2::Tag *id3Tag(TagLib::File *file)
{
    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(file))
        return mpeg->ID3v2Tag();
    if (auto *wav = dynamic_cast<TagLib::RIFF::WAV::File *>(file))
        return wav->ID3v2Tag();
    if (auto *aiff = dynamic_cast<TagLib::RIFF::AIFF::File *>(file))
        return aiff->tag();
    return nullptr;
}

TagLib::Ogg::XiphComment *xiphTag(TagLib::FileRef &file)
{
    return dynamic_cast<TagLib::Ogg::XiphComment *>(file.tag());
}

QString id3Text(TagLib::ID3v2::Tag *tag, const char *frameId)
{
    if (!tag || tag->frameList(frameId).isEmpty())
        return {};
    return fromTag(tag->frameList(frameId).front()->toString());
}

void setId3Text(TagLib::ID3v2::Tag *tag, const char *frameId, const QString &value)
{
    const auto frames = tag->frameList(frameId);
    for (auto *frame : frames)
        tag->removeFrame(frame, true);
    if (!value.isEmpty()) {
        auto *frame = new TagLib::ID3v2::TextIdentificationFrame(frameId, TagLib::String::UTF8);
        frame->setText(toTag(value));
        tag->addFrame(frame);
    }
}

QString xiphText(TagLib::Ogg::XiphComment *tag, const char *key)
{
    if (!tag || !tag->fieldListMap().contains(key) || tag->fieldListMap()[key].isEmpty())
        return {};
    return fromTag(tag->fieldListMap()[key].front());
}

void setXiphText(TagLib::Ogg::XiphComment *tag, const char *key, const QString &value)
{
    tag->removeFields(key);
    if (!value.isEmpty())
        tag->addField(key, toTag(value), false);
}

void setXiphText(TagLib::Ogg::XiphComment *tag, const TagLib::String &key, const QString &value)
{
    tag->removeFields(key);
    if (!value.isEmpty())
        tag->addField(key, toTag(value), false);
}

TagLib::String musicBrainzKey(const QString &key)
{
    return TagLib::String(QStringToTString(key));
}

QString musicBrainzMp4Name(const QString &picardKey)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("MUSICBRAINZ_RELEASEID"), QStringLiteral("MusicBrainz Release Id")},
        {QStringLiteral("MUSICBRAINZ_ALBUMID"), QStringLiteral("MusicBrainz Album Id")},
        {QStringLiteral("MUSICBRAINZ_ALBUMARTISTID"), QStringLiteral("MusicBrainz Album Artist Id")},
        {QStringLiteral("MUSICBRAINZ_ARTISTID"), QStringLiteral("MusicBrainz Artist Id")},
        {QStringLiteral("MUSICBRAINZ_TRACKID"), QStringLiteral("MusicBrainz Track Id")},
        {QStringLiteral("MUSICBRAINZ_RELEASEGROUPID"), QStringLiteral("MusicBrainz Release Group Id")},
    };
    return names.value(picardKey, picardKey);
}

QString musicBrainzFreeformKey(const QString &picardKey)
{
    return QStringLiteral("----:com.apple.iTunes:%1").arg(musicBrainzMp4Name(picardKey));
}

void writeMusicBrainzValue(TagLib::FileRef &file, const QString &key, const QString &value)
{
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (!mp4->tag())
            return;
        const TagLib::String tagKey = TagLib::String(QStringToTString(musicBrainzFreeformKey(key)));
        if (value.isEmpty()) {
            if (mp4->tag()->contains(tagKey))
                mp4->tag()->removeItem(tagKey);
            return;
        }
        TagLib::StringList values{TagLib::String(QStringToTString(value))};
        mp4->tag()->setItem(tagKey, TagLib::MP4::Item(values));
        return;
    }
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        if (auto *xiph = flac->xiphComment(true))
            setXiphText(xiph, musicBrainzKey(key), value);
        return;
    }
    if (auto *xiph = xiphTag(file)) {
        setXiphText(xiph, musicBrainzKey(key), value);
        return;
    }
    if (auto *id3 = id3Tag(file.file())) {
        const TagLib::String frameKey = TagLib::ID3v2::UserTextIdentificationFrame::keyToTXXX(musicBrainzKey(key));
        TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(id3, frameKey);
        if (value.isEmpty()) {
            if (frame)
                id3->removeFrame(frame, true);
            return;
        }
        if (!frame) {
            TagLib::StringList values{TagLib::String(QStringToTString(value))};
            frame = new TagLib::ID3v2::UserTextIdentificationFrame(frameKey, values, TagLib::String::UTF8);
            id3->addFrame(frame);
        }
        frame->setText(TagLib::String(QStringToTString(value)));
    }
}

QString readMusicBrainzValue(TagLib::FileRef &file, const QString &key)
{
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (mp4->tag()) {
            const TagLib::String tagKey = TagLib::String(QStringToTString(musicBrainzFreeformKey(key)));
            if (mp4->tag()->contains(tagKey)) {
                const TagLib::MP4::Item item = mp4->tag()->item(tagKey);
                const TagLib::StringList values = item.toStringList();
                if (!values.isEmpty())
                    return fromTag(values.front());
            }
        }
    }
    const TagLib::String tagKey = musicBrainzKey(key);
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        if (auto *xiph = flac->xiphComment(false)) {
            const auto values = xiph->fieldListMap().value(tagKey);
            if (!values.isEmpty())
                return fromTag(values.front());
        }
    }
    if (auto *xiph = xiphTag(file)) {
        const auto values = xiph->fieldListMap().value(tagKey);
        if (!values.isEmpty())
            return fromTag(values.front());
    }
    if (auto *id3 = id3Tag(file.file())) {
        const TagLib::String frameKey = TagLib::ID3v2::UserTextIdentificationFrame::keyToTXXX(tagKey);
        TagLib::ID3v2::UserTextIdentificationFrame *frame = TagLib::ID3v2::UserTextIdentificationFrame::find(id3, frameKey);
        if (frame) {
            const TagLib::StringList values = frame->fieldList();
            if (!values.isEmpty())
                return fromTag(values.back());
        }
    }
    return {};
}

void readExtendedTags(TagLib::FileRef &file, Track &track)
{
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (mp4->tag()) {
            const auto artists = mp4->tag()->item("aART").toStringList();
            if (!artists.isEmpty())
                track.albumArtist = fromTag(artists.front());
            if (mp4->tag()->contains("disk"))
                track.discNumber = mp4->tag()->item("disk").toIntPair().first;
        }
        return;
    }
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        if (auto *xiph = flac->xiphComment(false)) {
            track.albumArtist = xiphText(xiph, "ALBUMARTIST");
            track.discNumber = xiphText(xiph, "DISCNUMBER").section('/', 0, 0).toInt();
        }
        return;
    }
    if (auto *xiph = xiphTag(file)) {
        track.albumArtist = xiphText(xiph, "ALBUMARTIST");
        track.discNumber = xiphText(xiph, "DISCNUMBER").section('/', 0, 0).toInt();
        return;
    }
    if (auto *id3 = id3Tag(file.file())) {
        track.albumArtist = id3Text(id3, "TPE2");
        track.discNumber = id3Text(id3, "TPOS").section('/', 0, 0).toInt();
    }
}

bool writeExtendedTags(TagLib::FileRef &file, const Track &track, QString &error)
{
    const QString disc = track.discNumber > 0 ? QString::number(track.discNumber) : QString();
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (!mp4->tag()) {
            error = QObject::tr("This MP4 file has no writable metadata tag.");
            return false;
        }
        if (track.albumArtist.isEmpty())
            mp4->tag()->removeItem("aART");
        else
            mp4->tag()->setItem("aART", TagLib::MP4::Item(TagLib::StringList(toTag(track.albumArtist))));
        if (track.discNumber > 0)
            mp4->tag()->setItem("disk", TagLib::MP4::Item(track.discNumber, 0));
        else
            mp4->tag()->removeItem("disk");
        return true;
    }
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        auto *xiph = flac->xiphComment(true);
        setXiphText(xiph, "ALBUMARTIST", track.albumArtist);
        setXiphText(xiph, "DISCNUMBER", disc);
        return true;
    }
    if (auto *xiph = xiphTag(file)) {
        setXiphText(xiph, "ALBUMARTIST", track.albumArtist);
        setXiphText(xiph, "DISCNUMBER", disc);
        return true;
    }
    if (auto *id3 = id3Tag(file.file())) {
        setId3Text(id3, "TPE2", track.albumArtist);
        setId3Text(id3, "TPOS", disc);
        return true;
    }
    if (!track.albumArtist.isEmpty() || track.discNumber > 0) {
        error = QObject::tr("This format does not support album artist or disc number tags.");
        return false;
    }
    return true;
}

void readId3Cover(TagLib::ID3v2::Tag *tag, Track &track)
{
    if (!tag)
        return;
    const auto frames = tag->frameList("APIC");
    TagLib::ID3v2::AttachedPictureFrame *fallback = nullptr;
    for (auto *frame : frames) {
        auto *picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
        if (!picture)
            continue;
        if (picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover) {
            fallback = picture;
            break;
        }
        if (!fallback)
            fallback = picture;
    }
    if (fallback) {
        track.cover = fromBytes(fallback->picture());
        track.coverMime = fromTag(fallback->mimeType());
    }
}

void readFlacPictures(const TagLib::List<TagLib::FLAC::Picture *> &pictures, Track &track)
{
    TagLib::FLAC::Picture *fallback = nullptr;
    for (auto *picture : pictures) {
        if (picture->type() == TagLib::FLAC::Picture::FrontCover) {
            fallback = picture;
            break;
        }
        if (!fallback)
            fallback = picture;
    }
    if (fallback) {
        track.cover = fromBytes(fallback->data());
        track.coverMime = fromTag(fallback->mimeType());
    }
}

void readCover(TagLib::FileRef &file, Track &track)
{
    if (auto *id3 = id3Tag(file.file())) {
        readId3Cover(id3, track);
        return;
    }
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        readFlacPictures(flac->pictureList(), track);
        return;
    }
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (mp4->tag()) {
            const auto covers = mp4->tag()->item("covr").toCoverArtList();
            if (!covers.isEmpty()) {
                const auto &cover = covers.front();
                track.cover = fromBytes(cover.data());
                track.coverMime = cover.format() == TagLib::MP4::CoverArt::PNG
                    ? QStringLiteral("image/png") : QStringLiteral("image/jpeg");
            }
        }
        return;
    }
    if (auto *xiph = xiphTag(file))
        readFlacPictures(xiph->pictureList(), track);
}

bool writeId3Cover(TagLib::ID3v2::Tag *tag, const Track &track)
{
    if (!tag)
        return false;
    const auto frames = tag->frameList("APIC");
    for (auto *frame : frames)
        tag->removeFrame(frame, true);
    if (!track.cover.isEmpty()) {
        auto *picture = new TagLib::ID3v2::AttachedPictureFrame;
        picture->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        picture->setMimeType(toTag(track.coverMime.isEmpty() ? QStringLiteral("image/jpeg") : track.coverMime));
        picture->setPicture(toBytes(track.cover));
        tag->addFrame(picture);
    }
    return true;
}

void writeFlacPictures(TagLib::FLAC::File *flac, const Track &track)
{
    const auto pictures = flac->pictureList();
    for (auto *picture : pictures)
        flac->removePicture(picture, true);
    if (!track.cover.isEmpty()) {
        auto *picture = new TagLib::FLAC::Picture;
        picture->setType(TagLib::FLAC::Picture::FrontCover);
        picture->setMimeType(toTag(track.coverMime.isEmpty() ? QStringLiteral("image/jpeg") : track.coverMime));
        picture->setData(toBytes(track.cover));
        flac->addPicture(picture);
    }
}

void writeXiphPictures(TagLib::Ogg::XiphComment *xiph, const Track &track)
{
    xiph->removeAllPictures();
    if (!track.cover.isEmpty()) {
        auto *picture = new TagLib::FLAC::Picture;
        picture->setType(TagLib::FLAC::Picture::FrontCover);
        picture->setMimeType(toTag(track.coverMime.isEmpty() ? QStringLiteral("image/jpeg") : track.coverMime));
        picture->setData(toBytes(track.cover));
        xiph->addPicture(picture);
    }
}

bool writeCover(TagLib::FileRef &file, const Track &track, QString &error)
{
    if (auto *id3 = id3Tag(file.file()))
        return writeId3Cover(id3, track);
    if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file.file())) {
        writeFlacPictures(flac, track);
        return true;
    }
    if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file.file())) {
        if (!mp4->tag()) {
            error = QObject::tr("This MP4 file has no writable metadata tag.");
            return false;
        }
        TagLib::MP4::CoverArtList covers;
        if (!track.cover.isEmpty()) {
            const auto format = track.coverMime.contains("png", Qt::CaseInsensitive)
                ? TagLib::MP4::CoverArt::PNG : TagLib::MP4::CoverArt::JPEG;
            TagLib::MP4::CoverArt cover(format, toBytes(track.cover));
            covers.append(cover);
        }
        mp4->tag()->setItem("covr", TagLib::MP4::Item(covers));
        return true;
    }
    if (auto *xiph = xiphTag(file)) {
        writeXiphPictures(xiph, track);
        return true;
    }
    error = QObject::tr("Cover art editing is not supported for this audio format.");
    return false;
}

bool copyFileContents(const QString &sourcePath, QTemporaryFile &temporary, QString &error)
{
    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not read %1: %2").arg(sourcePath, source.errorString());
        return false;
    }
    if (!temporary.open()) {
        error = QObject::tr("Could not create a temporary file beside %1: %2")
                    .arg(sourcePath, temporary.errorString());
        return false;
    }
    while (!source.atEnd()) {
        const QByteArray block = source.read(1024 * 1024);
        if (block.isEmpty() && source.error() != QFileDevice::NoError) {
            error = QObject::tr("Could not read %1: %2").arg(sourcePath, source.errorString());
            return false;
        }
        if (temporary.write(block) != block.size()) {
            error = QObject::tr("Could not write the temporary copy: %1").arg(temporary.errorString());
            return false;
        }
    }
    if (!temporary.flush()) {
        error = QObject::tr("Could not flush the temporary copy: %1").arg(temporary.errorString());
        return false;
    }
    temporary.close();
    QFile::setPermissions(temporary.fileName(), source.permissions());
    return true;
}
}

bool sameEditableTags(const Track &a, const Track &b)
{
    return a.title == b.title && a.artist == b.artist && a.album == b.album
        && a.albumArtist == b.albumArtist && a.genre == b.genre && a.comment == b.comment
        && a.year == b.year && a.trackNumber == b.trackNumber && a.discNumber == b.discNumber
        && a.musicbrainzReleaseId == b.musicbrainzReleaseId && a.musicbrainzAlbumId == b.musicbrainzAlbumId
        && a.musicbrainzAlbumArtistId == b.musicbrainzAlbumArtistId && a.musicbrainzArtistId == b.musicbrainzArtistId
        && a.musicbrainzTrackId == b.musicbrainzTrackId && a.musicbrainzReleaseGroupId == b.musicbrainzReleaseGroupId
        && a.cover == b.cover && a.coverMime == b.coverMime;
}

QStringList supportedAudioSuffixes()
{
    return {QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("m4a"),
            QStringLiteral("mp4"), QStringLiteral("ogg"), QStringLiteral("oga"),
            QStringLiteral("opus"), QStringLiteral("wav"), QStringLiteral("aiff"),
            QStringLiteral("aif"), QStringLiteral("ape"), QStringLiteral("wv"),
            QStringLiteral("mpc")};
}

bool isSupportedAudioFile(const QString &path)
{
    return supportedAudioSuffixes().contains(QFileInfo(path).suffix().toLower());
}

QStringList collectAudioFiles(const QStringList &paths, QString *error)
{
    QStringList files;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir()) {
            QDirIterator iterator(info.absoluteFilePath(), QDir::Files | QDir::Readable,
                                  QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                const QString file = iterator.next();
                if (isSupportedAudioFile(file))
                    files.append(file);
            }
        } else if (info.isFile() && isSupportedAudioFile(path)) {
            files.append(info.absoluteFilePath());
        } else if (error && !path.isEmpty()) {
            *error = QObject::tr("Not a supported audio file or folder: %1").arg(path);
        }
    }
    files.removeDuplicates();
    files.sort(Qt::CaseInsensitive);
    return files;
}

bool readTrack(const QString &path, Track &track, QString &error)
{
    const QFileInfo info(path);
    TagLib::FileRef file(TagLib::FileName(QFile::encodeName(path).constData()));
    if (file.isNull() || !file.tag() || !file.audioProperties()
        || file.audioProperties()->sampleRate() <= 0 || file.audioProperties()->channels() <= 0) {
        error = QObject::tr("Could not read audio metadata from %1.").arg(path);
        return false;
    }

    track = Track();
    track.path = info.absoluteFilePath();
    track.fileName = info.fileName();
    track.format = info.suffix().toUpper();
    track.size = info.size();
    auto *tag = file.tag();
    track.title = fromTag(tag->title());
    track.artist = fromTag(tag->artist());
    track.album = fromTag(tag->album());
    track.genre = fromTag(tag->genre());
    track.comment = fromTag(tag->comment());
    track.year = static_cast<int>(tag->year());
    track.trackNumber = static_cast<int>(tag->track());
    const auto properties = tag->properties();
    track.albumArtist = firstProperty(properties, "ALBUMARTIST");
    track.discNumber = numberProperty(properties, "DISCNUMBER");
    readExtendedTags(file, track);
    track.musicbrainzReleaseId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_RELEASEID"));
    track.musicbrainzAlbumId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ALBUMID"));
    track.musicbrainzAlbumArtistId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ALBUMARTISTID"));
    track.musicbrainzArtistId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ARTISTID"));
    track.musicbrainzTrackId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_TRACKID"));
    track.musicbrainzReleaseGroupId = readMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_RELEASEGROUPID"));
    if (auto *audio = file.audioProperties()) {
        track.durationSeconds = audio->lengthInSeconds();
        track.bitrate = audio->bitrate();
        track.sampleRate = audio->sampleRate();
        track.channels = audio->channels();
    }
    readCover(file, track);
    return true;
}

bool writeTrackAtomic(const Track &track, QString &error)
{
    QFileInfo originalInfo(track.path);
    QString targetPath = originalInfo.absoluteFilePath();
    if (originalInfo.isSymLink()) {
        const QFileInfo targetInfo(originalInfo.symLinkTarget());
        targetPath = targetInfo.isAbsolute() ? originalInfo.symLinkTarget()
            : originalInfo.absoluteDir().absoluteFilePath(originalInfo.symLinkTarget());
    }
    const QFileInfo targetInfo(targetPath);
    const QString temporaryPattern = targetInfo.absoluteDir().filePath(
        QStringLiteral(".%1.tagr-XXXXXX.%2").arg(targetInfo.completeBaseName(), targetInfo.suffix()));
    QTemporaryFile temporary(temporaryPattern);
    if (!copyFileContents(targetPath, temporary, error))
        return false;

    {
        TagLib::FileRef file(TagLib::FileName(QFile::encodeName(temporary.fileName()).constData()));
        if (file.isNull() || !file.tag() || !file.audioProperties()
        || file.audioProperties()->sampleRate() <= 0 || file.audioProperties()->channels() <= 0) {
            error = QObject::tr("Could not open the temporary copy for metadata writing.");
            return false;
        }
        auto *tag = file.tag();
        if (!writeExtendedTags(file, track, error))
            return false;
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_RELEASEID"), track.musicbrainzReleaseId);
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ALBUMID"), track.musicbrainzAlbumId);
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ALBUMARTISTID"), track.musicbrainzAlbumArtistId);
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_ARTISTID"), track.musicbrainzArtistId);
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_TRACKID"), track.musicbrainzTrackId);
        writeMusicBrainzValue(file, QStringLiteral("MUSICBRAINZ_RELEASEGROUPID"), track.musicbrainzReleaseGroupId);
        tag->setTitle(toTag(track.title));
        tag->setArtist(toTag(track.artist));
        tag->setAlbum(toTag(track.album));
        tag->setGenre(toTag(track.genre));
        tag->setComment(toTag(track.comment));
        tag->setYear(static_cast<unsigned int>(qMax(0, track.year)));
        tag->setTrack(static_cast<unsigned int>(qMax(0, track.trackNumber)));
        if (track.coverChanged && !writeCover(file, track, error))
            return false;
        if (!file.save()) {
            error = QObject::tr("TagLib could not save %1. The original was left unchanged.").arg(track.path);
            return false;
        }
    }

    const QByteArray source = QFile::encodeName(temporary.fileName());
    const QByteArray target = QFile::encodeName(targetPath);
    if (::rename(source.constData(), target.constData()) != 0) {
        error = QObject::tr("Could not replace %1 atomically: %2. The original was left unchanged.")
                    .arg(track.path, QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    temporary.setAutoRemove(false);
    return true;
}

QJsonObject trackToJson(const Track &track)
{
    return {
        {QStringLiteral("path"), track.path},
        {QStringLiteral("fileName"), track.fileName},
        {QStringLiteral("title"), track.title},
        {QStringLiteral("artist"), track.artist},
        {QStringLiteral("album"), track.album},
        {QStringLiteral("albumArtist"), track.albumArtist},
        {QStringLiteral("genre"), track.genre},
        {QStringLiteral("comment"), track.comment},
        {QStringLiteral("year"), track.year},
        {QStringLiteral("track"), track.trackNumber},
        {QStringLiteral("disc"), track.discNumber},
        {QStringLiteral("musicbrainzReleaseId"), track.musicbrainzReleaseId},
        {QStringLiteral("musicbrainzAlbumId"), track.musicbrainzAlbumId},
        {QStringLiteral("musicbrainzAlbumArtistId"), track.musicbrainzAlbumArtistId},
        {QStringLiteral("musicbrainzArtistId"), track.musicbrainzArtistId},
        {QStringLiteral("musicbrainzTrackId"), track.musicbrainzTrackId},
        {QStringLiteral("musicbrainzReleaseGroupId"), track.musicbrainzReleaseGroupId},
        {QStringLiteral("format"), track.format},
        {QStringLiteral("durationSeconds"), track.durationSeconds},
        {QStringLiteral("bitrate"), track.bitrate},
        {QStringLiteral("sampleRate"), track.sampleRate},
        {QStringLiteral("channels"), track.channels},
        {QStringLiteral("size"), track.size},
        {QStringLiteral("hasCover"), !track.cover.isEmpty()},
        {QStringLiteral("coverMime"), track.coverMime},
    };
}

QString formatDuration(int seconds)
{
    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainder = seconds % 60;
    return hours > 0
        ? QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0')).arg(remainder, 2, 10, QLatin1Char('0'))
        : QStringLiteral("%1:%2").arg(minutes).arg(remainder, 2, 10, QLatin1Char('0'));
}
