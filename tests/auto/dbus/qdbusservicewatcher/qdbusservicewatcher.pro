CONFIG += testcase
TARGET = tst_qdbusservicewatcher
QT = core dbus testlib
SOURCES += tst_qdbusservicewatcher.cpp
include(../dbus-testcase.pri)
