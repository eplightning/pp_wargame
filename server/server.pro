TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CFLAGS += -std=gnu99
QMAKE_CFLAGS += -pthread
LIBS += -pthread

SOURCES += main.c \
    main_queue.c \
    mq_state.c \
    evqueue.c \
    player_processes.c \
    logic_processes.c \
    list.c \
    utils.c \
    game_stuff.c

HEADERS += \
    protocol.h \
    config.h \
    main_queue.h \
    mq_state.h \
    evqueue.h \
    player_processes.h \
    logic_processes.h \
    list.h \
    utils.h \
    game_stuff.h

DISTFILES +=
