QT -= gui
CONFIG += c++17 console
TEMPLATE = app

INCLUDEPATH += include

SOURCES += \
    src/main.cpp \
    src/physical_uart.cpp \
    src/transport.cpp \
    src/channel.cpp \


HEADERS += \
    include/async_queue.h \
    include/physical_uart.h \
    include/transport.h \
    include/channel.h \

