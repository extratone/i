# JavaScriptCore - qmake build info
CONFIG += building-libs
include($$PWD/../WebKit.pri)

TEMPLATE = lib
CONFIG += staticlib
TARGET = JavaScriptCore

CONFIG += depend_includepath

contains(QT_CONFIG, embedded):CONFIG += embedded

CONFIG(QTDIR_build) {
    GENERATED_SOURCES_DIR = $$PWD/generated
    OLDDESTDIR = $$DESTDIR
    include($$QT_SOURCE_TREE/src/qbase.pri)
    INSTALLS =
    DESTDIR = $$OLDDESTDIR
    PRECOMPILED_HEADER = $$PWD/../WebKit/qt/WebKit_pch.h
    DEFINES *= NDEBUG
}

isEmpty(GENERATED_SOURCES_DIR):GENERATED_SOURCES_DIR = tmp
GENERATED_SOURCES_DIR_SLASH = $$GENERATED_SOURCES_DIR/
win32-*: GENERATED_SOURCES_DIR_SLASH ~= s|/|\|

INCLUDEPATH += $$GENERATED_SOURCES_DIR

!CONFIG(QTDIR_build) {
     OBJECTS_DIR = tmp
}

include($$OUTPUT_DIR/config.pri)

CONFIG -= warn_on
*-g++*:QMAKE_CXXFLAGS += -Wreturn-type -fno-strict-aliasing
#QMAKE_CXXFLAGS += -Wall -Wno-undef -Wno-unused-parameter

CONFIG(release):!CONFIG(QTDIR_build) {
    contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
    unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
}

linux-*: DEFINES += HAVE_STDINT_H
freebsd-*: DEFINES += HAVE_PTHREAD_NP_H

DEFINES += BUILD_WEBKIT

win32-*: DEFINES += _HAS_TR1=0

# Pick up 3rdparty libraries from INCLUDE/LIB just like with MSVC
win32-g++ {
    TMPPATH            = $$quote($$(INCLUDE))
    QMAKE_INCDIR_POST += $$split(TMPPATH,";")
    TMPPATH            = $$quote($$(LIB))
    QMAKE_LIBDIR_POST += $$split(TMPPATH,";")
}

DEFINES += WTF_USE_JAVASCRIPTCORE_BINDINGS=1

DEFINES += WTF_CHANGES=1

include(JavaScriptCore.pri)

QMAKE_EXTRA_TARGETS += generated_files

qt-port: lessThan(QT_MINOR_VERSION, 4) {
    DEFINES += QT_BEGIN_NAMESPACE="" QT_END_NAMESPACE=""
}

*-g++*:QMAKE_CXXFLAGS_RELEASE -= -O2
*-g++*:QMAKE_CXXFLAGS_RELEASE += -O3
