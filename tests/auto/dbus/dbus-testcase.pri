# Detect whether we have a running D-Bus session bus or we can start one
defineTest(qtTestCheckDBus) {
    !isEmpty(QMAKE_DBUS_TESTS): return()

    cmd = dbus-run-session --
    equals(QMAKE_HOST.os, Windows):isEmpty(QMAKE_SH): cmd += echo.
    else: cmd += true

    QMAKE_DBUS_TESTS = 0

    # check dbus-run-session first
    system($$cmd) {
        QMAKE_DBUS_TESTS = 2
    } else {
        # no dbus-run-session, check if the session bus is running or can be autolaunched
        unset(TESTRUNNER)

        cmd = dbus-send --session --type=signal / local.AutotestCheck.Hello
        equals(QMAKE_HOST.os, Windows): cmd += >NUL 2>NUL
        else: cmd += >/dev/null 2>/dev/null
        system($$cmd) {
            QMAKE_DBUS_TESTS = 1
        } else {
            qtConfig(dbus-linked): \
                error("QtDBus is enabled but session bus is not available. Please check the installation.")
            else: \
                warning("QtDBus is enabled with runtime support, but session bus is not available. Skipping QtDBus tests.")
        }
    }

    export(QMAKE_DBUS_TESTS)
    cache(QMAKE_DBUS_TESTS, set stash)
}

cross_compile {
    # We have to assume they just work and that dbus-run-session is properly installed
    QMAKE_DBUS_TESTS = 2
} else {
    # Check how to do it
    qtTestCheckDBus()
}
equals(QMAKE_DBUS_TESTS, 2): TESTRUNNER = dbus-run-session --
