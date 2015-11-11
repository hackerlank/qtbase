TARGET = tst_tostring
CONFIG   += console testcase parallel_test
CONFIG   -= app_bundle
QT += testlib
qtHaveModule(network): QT += network
qtHaveModule(widgets): QT += widgets
SOURCES += tst_tostring.cpp

linux|freebsd: QMAKE_LFLAGS += -rdynamic
