TARGET = tst_bench_qstring
QT -= gui
QT += core-private testlib
SOURCES += main.cpp \
    utf8codec_qt53.cpp \
    utf8codec_qt53_impl.cpp

TESTDATA = utf-8.txt

contains(QT_CONFIG, icu) {
    DEFINES += QT_HAVE_ICU
    win32 {
        CONFIG(static, static|shared) {
            CONFIG(debug, debug|release) {
                LIBS_PRIVATE += -lsicuind -lsicuucd -lsicudtd
            } else {
                LIBS_PRIVATE += -lsicuin -lsicuuc -lsicudt
            }
        } else {
            LIBS_PRIVATE += -licuin -licuuc
        }
    } else {
        LIBS_PRIVATE += -licui18n -licuuc
    }
}

APPDATASOURCES = $$PWD/extracted-data/*
application_data_utf8.commands = perl $$PWD/generatelist_char.pl ${QMAKE_FILE_BASE}Data > ${QMAKE_FILE_OUT} < ${QMAKE_FILE_IN}
application_data_utf8.output = extracted_${QMAKE_FILE_BASE}.cpp
application_data_utf8.input = APPDATASOURCES
application_data_utf8.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += application_data_utf8

GENDATASOURCES = $$PWD/generated-data
generated_data_utf8.commands = cd $$PWD && perl ./generateutf8data.pl ${QMAKE_FILE_IN}/* > $$OUT_PWD/${QMAKE_FILE_OUT}
generated_data_utf8.output = generated_data.cpp
generated_data_utf8.input = GENDATASOURCES
generated_data_utf8.variable_out = GENERATED_SOURCES
QMAKE_EXTRA_COMPILERS += generated_data_utf8

sse4:QMAKE_CXXFLAGS += -msse4
else:ssse3:QMAKE_FLAGS += -mssse3
else:sse2:QMAKE_CXXFLAGS += -msse2
neon:QMAKE_CXXFLAGS += -mfpu=neon
