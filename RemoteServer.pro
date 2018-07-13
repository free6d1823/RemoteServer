QT += core
QT -= gui

TARGET = RemoteServer
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    server.cpp \
    client.cpp

HEADERS += \
    common.h

