SOURCES = aes.cpp
CONFIG -= qt dylib release debug_and_release
CONFIG += debug console
!defined(QMAKE_CFLAGS_AES, "var"):error("This compiler does not support AES New Instructions")
else:QMAKE_CXXFLAGS += $$QMAKE_CFLAGS_AES
