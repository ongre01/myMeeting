QT += core gui widgets multimedia testlib

TEMPLATE = app
TARGET = tst_appshell

CONFIG += testcase c++17

!equals(QT_MAJOR_VERSION, 6): error("myMeeting tests require Qt 6.x.")
!win32: error("myMeeting tests support Windows 10/11 only.")

INCLUDEPATH += ..

SOURCES += \
    tst_appshell.cpp \
    ../applicationlog.cpp \
    ../aibackendclient.cpp \
    ../mainwindow.cpp \
    ../meetingexporter.cpp \
    ../meetingminutes.cpp \
    ../meetingstorage.cpp \
    ../audiorecorder.cpp \
    ../wavfilewriter.cpp

HEADERS += \
    ../applicationlog.h \
    ../aibackendclient.h \
    ../mainwindow.h \
    ../meetingexporter.h \
    ../meetingminutes.h \
    ../meetingstorage.h \
    ../audiorecorder.h \
    ../wavfilewriter.h

FORMS += \
    ../mainwindow.ui
