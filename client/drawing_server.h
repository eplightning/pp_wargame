#ifndef DRAWING_SERVER_H
#define DRAWING_SERVER_H

#include "game_screen.h"
#include "message_stack.h"

#include <ncurses.h>

#define DRAW_SERVER_REFRESH         1
#define DRAW_SERVER_STACK_APPEND    2
#define DRAW_SERVER_MAIN_MENU       3
#define DRAW_SERVER_NUMBER_SELECT   4
#define DRAW_SERVER_UNIT_SELECT     5
#define DRAW_SERVER_SHUTDOWN        6

typedef struct draw_msg_refresh {
    long id;
} draw_msg_refresh_t;

typedef struct draw_msg_shutdown {
    long id;
} draw_msg_shutdown_t;

typedef struct draw_msg_stack {
    long id;
    char info[255];
} draw_msg_stack_t;

typedef struct draw_msg_mainmenu {
    long id;
    char item[30];
} draw_msg_mainmenu_t;

typedef struct draw_msg_number {
    long id;
    char title[30];
    unsigned int number;
} draw_msg_number_t;

typedef struct draw_msg_unit {
    long id;
    char title[30];
} draw_msg_unit_t;

void drawing_server_process(int queue, event_listener_data_t *data);

void refresh_top_window(WINDOW *wnd, event_listener_data_t *data, message_stack_t *stack);

#endif // DRAWING_SERVER_H

