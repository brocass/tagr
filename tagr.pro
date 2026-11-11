QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release link_pkgconfig
PKGCONFIG += taglib
TARGET = tagr
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/portalfilepicker.h \
    src/theme.h \
    src/track.h \
    src/trackmodel.h \
    src/translator.h

SOURCES += \
    src/backend.cpp \
    src/cli.cpp \
    src/main.cpp \
    src/portalfilepicker.cpp \
    src/theme.cpp \
    src/track.cpp \
    src/trackmodel.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
