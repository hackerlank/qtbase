CONFIG += testcase
TARGET = tst_qdbuspendingreply
QT = core dbus testlib
SOURCES += tst_qdbuspendingreply.cpp
include(../dbus-testcase.pri)
