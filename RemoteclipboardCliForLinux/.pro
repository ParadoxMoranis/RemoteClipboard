QT += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = RemotePostboard
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    tcpclient.cpp \
    clipboardmonitor.cpp

HEADERS += \
    mainwindow.h \
    tcpclient.h