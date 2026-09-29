QT += core gui widgets multimedia

TEMPLATE = app
TARGET = myMeeting

CONFIG += c++17
win32:CONFIG += windows

!equals(QT_MAJOR_VERSION, 6): error("myMeeting requires Qt 6.x.")
!win32: error("myMeeting supports Windows 10/11 only.")

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    meetingstorage.cpp \
    audiorecorder.cpp \
    wavfilewriter.cpp

HEADERS += \
    mainwindow.h \
    meetingstorage.h \
    audiorecorder.h \
    wavfilewriter.h

FORMS += \
    mainwindow.ui
