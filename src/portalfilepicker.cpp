// Adapted from Omacut's portal file picker.
// Omacut: Copyright David Heinemeier Hansson, licensed under the MIT License.
// https://github.com/omacom-io/omacut

#include "portalfilepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QRandomGenerator>
#include <QStandardPaths>

namespace {
struct PortalFilterRule {
    uint type;
    QString pattern;
};

using PortalFilterRules = QList<PortalFilterRule>;

struct PortalFileFilter {
    QString name;
    PortalFilterRules rules;
};

using PortalFileFilters = QList<PortalFileFilter>;

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFileFilter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFileFilter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

void registerPortalFilterTypes() {
    static const bool registered = [] {
        qDBusRegisterMetaType<PortalFilterRule>();
        qDBusRegisterMetaType<PortalFilterRules>();
        qDBusRegisterMetaType<PortalFileFilter>();
        qDBusRegisterMetaType<PortalFileFilters>();
        return true;
    }();
    Q_UNUSED(registered);
}

PortalFileFilter audioFilter() {
    return {
        QObject::tr("Audio files"),
        {
            {1, QStringLiteral("audio/*")},
            {0, QStringLiteral("*.mp3")},
            {0, QStringLiteral("*.flac")},
            {0, QStringLiteral("*.m4a")},
            {0, QStringLiteral("*.mp4")},
            {0, QStringLiteral("*.ogg")},
            {0, QStringLiteral("*.oga")},
            {0, QStringLiteral("*.opus")},
            {0, QStringLiteral("*.wav")},
            {0, QStringLiteral("*.aiff")},
            {0, QStringLiteral("*.aif")},
            {0, QStringLiteral("*.ape")},
            {0, QStringLiteral("*.wv")},
            {0, QStringLiteral("*.mpc")},
        },
    };
}

PortalFileFilters audioFilters() {
    return {audioFilter()};
}

PortalFileFilter imageFilter() {
    return {
        QObject::tr("Images"),
        {
            {1, QStringLiteral("image/jpeg")},
            {1, QStringLiteral("image/png")},
            {0, QStringLiteral("*.jpg")},
            {0, QStringLiteral("*.jpeg")},
            {0, QStringLiteral("*.png")},
        },
    };
}

PortalFileFilters imageFilters() {
    return {imageFilter()};
}

QString portalToken() {
    return QStringLiteral("tagr_%1").arg(QRandomGenerator::global()->generate());
}

QByteArray portalPathBytes(const QString &path) {
    QByteArray bytes = path.toUtf8();
    bytes.append('\0');
    return bytes;
}

}  // namespace

QString PortalFilePicker::openFolder() {
    if (!m_defaultFolder.isEmpty() && QDir(m_defaultFolder).exists())
        return m_defaultFolder;
    const QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    return !music.isEmpty() && QDir(music).exists() ? music : QDir::homePath();
}

void PortalFilePicker::setDefaultFolder(const QString &folder) {
    m_defaultFolder = folder;
}

PortalFilePicker::PortalFilePicker(QObject *parent) : FilePicker(parent) {
    registerPortalFilterTypes();
}

void PortalFilePicker::openDirectory() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), QObject::tr("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("directory"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(openFolder()));

    requestFile(QStringLiteral("OpenFile"), QObject::tr("Open Directory"), options,
                Action::Directory);
}

void PortalFilePicker::openFiles() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), QObject::tr("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), true);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(openFolder()));
    options.insert(QStringLiteral("filters"), QVariant::fromValue(audioFilters()));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(audioFilter()));

    requestFile(QStringLiteral("OpenFile"), QObject::tr("Open Audio Files"), options,
                Action::Files);
}

void PortalFilePicker::openImage() {
    QVariantMap options;
    options.insert(QStringLiteral("accept_label"), QObject::tr("Open"));
    options.insert(QStringLiteral("modal"), true);
    options.insert(QStringLiteral("multiple"), false);
    options.insert(QStringLiteral("current_folder"), portalPathBytes(openFolder()));
    options.insert(QStringLiteral("filters"), QVariant::fromValue(imageFilters()));
    options.insert(QStringLiteral("current_filter"), QVariant::fromValue(imageFilter()));

    requestFile(QStringLiteral("OpenFile"), QObject::tr("Open Image"), options, Action::Image);
}

bool PortalFilePicker::connectToRequestPath(const QString &path) {
    m_pendingPath = path;
    return QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
        QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
        this, SLOT(handleResponse(uint,QVariantMap)));
}

void PortalFilePicker::closePendingRequest() {
    if (m_pendingPath.isEmpty())
        return;
    QDBusInterface request(QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                           QStringLiteral("org.freedesktop.portal.Request"),
                           QDBusConnection::sessionBus());
    request.asyncCall(QStringLiteral("Close"));
}

void PortalFilePicker::requestFile(const QString &method, const QString &title,
                                   QVariantMap options, Action action) {
    // A request whose Response never arrives (the portal crashed or was
    // killed with the dialog up) must not wedge every later dialog behind
    // it: close the stuck one best-effort and replace it.
    if (m_pendingAction != Action::None) {
        closePendingRequest();
        clearPending();
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.FileChooser"),
                          bus);
    if (!portal.isValid()) {
        emit failed(QObject::tr("The XDG desktop portal file chooser is not available."));
        return;
    }

    // Subscribe to the Response signal at the request path the portal will
    // derive from our handle_token *before* making the call, so a response
    // can't slip past while our match rule is still being installed.
    const QString token = portalToken();
    options.insert(QStringLiteral("handle_token"), token);
    QString sender = bus.baseService().mid(1);
    sender.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString predictedPath =
        QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);

    m_pendingAction = action;
    if (!connectToRequestPath(predictedPath)) {
        clearPending();
        emit failed(QObject::tr("Could not listen for the portal file picker response."));
        return;
    }

    const int generation = ++m_pendingGeneration;
    auto *watcher = new QDBusPendingCallWatcher(
        portal.asyncCall(method, QString(), title, options), this);
    m_pendingWatcher = watcher;

    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, generation]() {
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        watcher->deleteLater();
        if (m_pendingWatcher == watcher)
            m_pendingWatcher = nullptr;

        // Superseded by a newer request, or a response may already have
        // been handled while the reply was in flight.
        if (generation != m_pendingGeneration || m_pendingAction == Action::None)
            return;

        if (reply.isError()) {
            clearPending();
            emit failed(QObject::tr("The portal file picker failed: %1")
                            .arg(reply.error().message()));
            return;
        }

        // Old portal versions can hand back a different request path than the
        // handle_token predicts; move our subscription over if so.
        const QString actualPath = reply.value().path();
        if (actualPath != m_pendingPath) {
            QDBusConnection::sessionBus().disconnect(
                QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
                QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
                this, SLOT(handleResponse(uint,QVariantMap)));
            if (!connectToRequestPath(actualPath)) {
                clearPending();
                emit failed(QObject::tr("Could not listen for the portal file picker response."));
            }
        }
    });
}

void PortalFilePicker::handleResponse(uint response, const QVariantMap &results) {
    const Action action = m_pendingAction;
    clearPending();

    if (response != 0)
        return;

    const QStringList uris = results.value(QStringLiteral("uris")).toStringList();
    if (uris.isEmpty())
        return;

    if (action == Action::Files) {
        QList<QUrl> urls;
        urls.reserve(uris.size());
        for (const QString &uri : uris)
            urls.append(QUrl(uri));
        emit filesSelected(urls);
        return;
    }

    const QUrl url(uris.first());
    if (action == Action::Directory)
        emit directorySelected(url);
    else if (action == Action::Image)
        emit imageSelected(url);
}

void PortalFilePicker::clearPending() {
    // A caller that never gets its reply must not outlive the request it
    // belonged to (repeated stuck requests would otherwise pile them up).
    if (m_pendingWatcher) {
        m_pendingWatcher->deleteLater();
        m_pendingWatcher = nullptr;
    }
    if (!m_pendingPath.isEmpty()) {
        QDBusConnection::sessionBus().disconnect(
            QStringLiteral("org.freedesktop.portal.Desktop"), m_pendingPath,
            QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"),
            this, SLOT(handleResponse(uint,QVariantMap)));
    }

    m_pendingPath.clear();
    m_pendingAction = Action::None;
}
