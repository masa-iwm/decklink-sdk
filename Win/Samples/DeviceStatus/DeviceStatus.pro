# -LICENSE-START-
# Copyright (c) 2020 Blackmagic Design
#
# Permission is hereby granted, free of charge, to any person or organization
# obtaining a copy of the software and accompanying documentation covered by
# this license (the "Software") to use, reproduce, display, distribute,
# execute, and transmit the Software, and to prepare derivative works of the
# Software, and to permit third-parties to whom the Software is furnished to
# do so, all subject to the following:
#
# The copyright notices in the Software and this entire statement, including
# the above license grant, this restriction and the following disclaimer,
# must be included in all copies of the Software, in whole or in part, and
# all derivative works of the Software, unless such copies or derivative
# works are solely in the form of machine-executable object code generated by
# a source language processor.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
# SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
# FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
# -LICENSE-END-

CONFIG += qt
CONFIG += embed_manifest_exe
CONFIG += c++11

QT += core gui widgets

TARGET = DeviceStatus
TEMPLATE = app

INCLUDEPATH += .
macx:INCLUDEPATH += ../../../Mac/include
win32:INCLUDEPATH += ../../../Win/include
unix:!mac:INCLUDEPATH += ../../../Linux/include

macx:LIBS += -framework CoreFoundation
unix:!mac:LIBS += -ldl
win32:LIBS += -lole32 -lopengl32

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += main.cpp \
    DeckLinkDeviceListModel.cpp \
    DeckLinkProfileCallback.cpp \
    DeckLinkStatusDataTableModel.cpp \
    DeviceStatus.cpp \
    platform.cpp

unix:!macx:SOURCES += ../../../Linux/include/DeckLinkAPIDispatch.cpp
macx:SOURCES += ../../../Mac/include/DeckLinkAPIDispatch.cpp

HEADERS += \
    DeckLinkDeviceListModel.h \
    DeckLinkProfileCallback.h \
    DeckLinkStatusDataTableModel.h \
    com_ptr.h \
    DeviceStatus.h \
    platform.h

FORMS += \
    DeviceStatus.ui

win32 {
    MIDL_FILES = "../../../Win/include/DeckLinkAPI.idl"

    MIDL.name = Compiling IDL
    MIDL.input = MIDL_FILES
    MIDL.output = ${QMAKE_FILE_BASE}.h
    MIDL.variable_out = HEADERS
    MIDL_CONFIG += no_link
    contains(QMAKE_TARGET.arch, x86_64)     {
        MIDL.commands = midl.exe /env win64 /h ${QMAKE_FILE_BASE}.h /W1 /char signed /D "NDEBUG" /robust /nologo ${QMAKE_FILE_IN}
    } else {
        MIDL.commands = midl.exe /env win32 /h ${QMAKE_FILE_BASE}.h /W1 /char signed /D "NDEBUG" /robust /nologo ${QMAKE_FILE_IN}
    }
    QMAKE_EXTRA_COMPILERS += MIDL
}