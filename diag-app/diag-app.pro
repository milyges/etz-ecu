#-------------------------------------------------
#
# Project created by QtCreator 2015-01-21T22:28:03
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = diag-app
TEMPLATE = app
CONFIG += serialport

SOURCES += main.cpp\
        wndmain.cpp

HEADERS  += wndmain.h

FORMS    += wndmain.ui
