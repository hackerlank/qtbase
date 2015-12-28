TEMPLATE = subdirs

SUBDIRS += \
    corelib \
    dbus \
    gui \
    network \
    opengl \
    sql \
    testlib \
    tools \
    xml \
    concurrent \
    other \
    widgets \
    printsupport \
    cmake \
    installed_cmake

installed_cmake.depends = cmake

uikit: SUBDIRS  = corelib gui

wince:                                      SUBDIRS -= printsupport
cross_compile:                              SUBDIRS -= tools cmake installed_cmake
!qtHaveModule(opengl):                      SUBDIRS -= opengl
!qtHaveModule(gui):                         SUBDIRS -= gui
!qtHaveModule(widgets):                     SUBDIRS -= widgets
!qtHaveModule(printsupport):                SUBDIRS -= printsupport
!qtHaveModule(concurrent):                  SUBDIRS -= concurrent
!qtHaveModule(network):                     SUBDIRS -= network
!qtHaveModule(dbus):                        SUBDIRS -= dbus

# Disable the QtDBus tests if we can't start a session bus
qtHaveModule(dbus) {
    include(dbus/dbus-testcase.pri)
    equals(QMAKE_DBUS_TESTS, 0):            SUBDIRS -= dbus
}
