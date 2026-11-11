QT += core gui qml quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase link_pkgconfig
PKGCONFIG += taglib
TARGET = tagr_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += \
    ../src/backend.h \
    ../src/filepicker.h \
    ../src/portalfilepicker.h \
    ../src/theme.h \
    ../src/track.h \
    ../src/trackmodel.h \
    ../src/translator.h

SOURCES += \
    tagr_tests.cpp \
    ../src/backend.cpp \
    ../src/portalfilepicker.cpp \
    ../src/theme.cpp \
    ../src/track.cpp \
    ../src/trackmodel.cpp \
    ../src/translator.cpp
