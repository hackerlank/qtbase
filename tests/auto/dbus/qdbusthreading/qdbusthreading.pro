CONFIG += testcase
TARGET = tst_qdbusthreading
QT = core dbus testlib
SOURCES += tst_qdbusthreading.cpp
include(../dbus-testcase.pri)
