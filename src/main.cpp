// Tagr — a native, keyboard-first audio tag editor for Omarchy.

#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "cli.h"
#include "theme.h"
#include "translator.h"

class CoverImageProvider : public QQuickImageProvider {
public:
    explicit CoverImageProvider(Backend *backend)
        : QQuickImageProvider(QQuickImageProvider::Image), m_backend(backend) {}

    QImage requestImage(const QString &, QSize *size, const QSize &requestedSize) override
    {
        QImage image = m_backend->currentCoverImage();
        if (requestedSize.isValid() && !image.isNull())
            image = image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (size)
            *size = image.size();
        return image;
    }

private:
    Backend *m_backend;
};

int main(int argc, char *argv[])
{
    const bool gui = argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("open");
    if (!gui)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    QGuiApplication app(argc, argv);
    QFont interfaceFont = app.font();
    interfaceFont.setPointSize(9);
    QGuiApplication::setFont(interfaceFont);
    app.setApplicationName(QStringLiteral("tagr"));
    app.setApplicationVersion(QStringLiteral("0.4.2"));
    app.setDesktopFileName(QStringLiteral("tagr"));
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("tagr")));

    Translator translator;
    app.installTranslator(&translator);
    if (!gui)
        return runCli(app.arguments());

    QQuickStyle::setStyle(QStringLiteral("Material"));
    Theme theme;
    Backend backend;
    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty(QStringLiteral("translator"), &translator);
    engine.rootContext()->setContextProperty(QStringLiteral("theme"), &theme);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.addImageProvider(QStringLiteral("tagrcover"), new CoverImageProvider(&backend));
    engine.load(QUrl(QStringLiteral("qrc:/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    if (argc > 2) {
        QStringList paths;
        for (int index = 2; index < argc; ++index)
            paths.append(QString::fromLocal8Bit(argv[index]));
        backend.openPaths(paths);
    }
    return app.exec();
}
