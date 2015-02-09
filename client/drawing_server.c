#include "drawing_server.h"
#include "game_screen.h"
#include "message_stack.h"

#include <ncurses.h>

#include <semaphore.h>
#include <sys/errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

void refresh_top_window(WINDOW *wnd, event_listener_data_t *data, message_stack_t *stack)
{
    wclear(wnd);
    box(wnd, 0, 0);

    if (data->game_started == 0) {
        wmove(wnd, 1, 2);
        wprintw(wnd, "Oczekiwanie na poczatek gry ...");
    } else {
        // semafor opuszczamy
        if (sem_wait(&data->mutex) < 0)
            return;

        wmove(wnd, 1, 2);
        wprintw(wnd, "Seat: %d", data->seat);
        wmove(wnd, 1, 10);
        wprintw(wnd, "Wynik: %d-%d", data->points[0], data->points[1]);
        wmove(wnd, 1, 21);
        wprintw(wnd, "Surowce: %d", data->resources);
        wmove(wnd, 2, 2);
        wprintw(wnd, "Przeciwnik: %s", data->enemy_name);
        wmove(wnd, 3, 2);
        wprintw(wnd, "Lekka: %d Ciezka: %d Jezdzcy: %d Robotnicy: %d", data->army.light,
                data->army.heavy, data->army.horsemen, data->army.workers);

        // semafor podnosimy
        sem_post(&data->mutex);

        // eventy
        for (int i = 0; i < stack->current_lines; i++) {
            wmove(wnd, 5 + i, 2);
            wprintw(wnd, "%s", stack->lines[i]);
        }
    }

    wrefresh(wnd);
}

void drawing_server_process(int queue, event_listener_data_t *data)
{
    message_stack_t *stack = message_stack_new(LINES - 12);
    char incoming[sizeof(draw_msg_stack_t)];

    WINDOW *topwnd = newwin(LINES - 6, COLS - 2, 0, 0);
    WINDOW *botwnd = newwin(2, COLS - 2, LINES - 4, 0);

    refresh_top_window(topwnd, data, stack);

    int menu_drawn = 0;

    while (1) {
        int status = msgrcv(queue, incoming, sizeof(draw_msg_stack_t) - sizeof(long), 0, MSG_NOERROR);

        if (status < 0) {
            // nie jestem pewien czy jest sens tutaj ignorować sygnały?
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        long *type = (long*) incoming;

        if (*type == DRAW_SERVER_SHUTDOWN) {
            msgctl(queue, IPC_RMID, 0);
            break;
        } else if (*type == DRAW_SERVER_MAIN_MENU) {
            menu_drawn = 1;
            draw_msg_mainmenu_t *msg = (draw_msg_mainmenu_t*) incoming;

            wclear(botwnd);

            wmove(botwnd, 1, 2);
            wprintw(botwnd, "< %s >", msg->item);

            wrefresh(botwnd);
        } else if (*type == DRAW_SERVER_NUMBER_SELECT) {
            menu_drawn = 1;
            draw_msg_number_t *msg = (draw_msg_number_t*) incoming;

            wclear(botwnd);

            wmove(botwnd, 1, 2);
            wprintw(botwnd, "%s < %d >", msg->title, msg->number);

            wrefresh(botwnd);
        } else if (*type == DRAW_SERVER_UNIT_SELECT) {
            draw_msg_unit_t *msg = (draw_msg_unit_t*) incoming;

            wclear(botwnd);

            wmove(botwnd, 1, 2);
            wprintw(botwnd, "Wybierz typ: < %s >", msg->title);

            wrefresh(botwnd);
        } else if (*type == DRAW_SERVER_STACK_APPEND) {
            draw_msg_stack_t *msg = (draw_msg_stack_t*) incoming;

            // kopiujemy do nowo utworzonej pamięci i dodajemy na stack
            char *event = calloc(255, sizeof(char));
            strncpy(event, msg->info, 255);
            message_stack_add(stack, event);

            refresh_top_window(topwnd, data, stack);
        } else if (*type == DRAW_SERVER_REFRESH) {
            refresh_top_window(topwnd, data, stack);

            if (menu_drawn == 0) {
                wclear(botwnd);

                wmove(botwnd, 1, 2);
                wprintw(botwnd, "< Trenuj >");

                wrefresh(botwnd);

                menu_drawn = 1;
            }
        }
    }

    delwin(botwnd);
    delwin(topwnd);

    message_stack_cleanup(stack);
}
