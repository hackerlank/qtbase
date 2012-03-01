# zlib dependency satisfied by bundled 3rd party zlib or system zlib
qtConfig(system-zlib) {
    QMAKE_USE_PRIVATE += zlib
} else:!static {
    # build in the zlib source code into this library
    include($$PWD/zlib.pri)
} else {
    # use the code that was built into QtCore
    INCLUDEPATH +=  $$PWD/zlib
    !no_core_dep {
        CONFIG += qt
        QT_PRIVATE += core
    }
}
