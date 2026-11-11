#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    ~FilePicker() override = default;

    virtual void openDirectory() = 0;
    virtual void openFiles() = 0;
    virtual void openImage() = 0;

    // Preferred starting folder for the file picker dialogs. Base does nothing;
    // PortalFilePicker uses it as the dialog's current_folder.
    virtual void setDefaultFolder(const QString &folder) { Q_UNUSED(folder); }

signals:
    void directorySelected(const QUrl &url);
    void filesSelected(const QList<QUrl> &urls);
    void imageSelected(const QUrl &url);
    void failed(const QString &message);
};
