#include "game_screen.h"
#include "message_stack.h"
#include "protocol.h"
#include "drawing_server.h"

#include <ncurses.h>

#include <pthread.h>
#include <semaphore.h>
#include <sys/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>

// niestety globale zeby w przypadku sygnałów nie pozostało dużo śmieci
int cq = -1;
int draw_queue = -1;
int shared_mem = -1;

void message_queue_cleanup(int type)
{
    if (type == SIGTERM || type == SIGQUIT || type == SIGINT) {
        if (cq > -1) {
            msgctl(cq, IPC_RMID, 0);
        }

        if (draw_queue > -1) {
            msgctl(draw_queue, IPC_RMID, 0);
        }

        if (shared_mem > -1) {
            shmctl(shared_mem, IPC_RMID, 0);
        }

        endwin();

        exit(0);
    }
}

void event_listener(int draw_queue, event_listener_data_t *data, int client_queue, char seat)
{
    // inicjalizacja reszty zmiennych
    data->enemy_name[0] = 0;
    data->points[0] = 0;
    data->points[1] = 0;
    data->resources = 0;
    data->army.heavy = 0;
    data->army.horsemen = 0;
    data->army.light = 0;
    data->army.workers = 0;
    data->seat = seat;
    data->game_started = 0;

    // teraz czekamy na eventy serwera
    char msg_bytes[GAMEQUEUE_MAX_MESSAGE_SIZE];

    while (1) {
        int status = msgrcv(client_queue, msg_bytes, GAMEQUEUE_MAX_MESSAGE_LENGTH, 0, 0);

        if (status < 0) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        // pierwszym polem tego będzie long
        long *type = (long*) msg_bytes;

        // semafor opuszczamy
        if (sem_wait(&data->mutex) < 0)
            break;

        if (*type == GAMEQUEUE_MSG_EVENT_UNIT_TRAINED) {
            event_msg_unit_trained_t *msg = (event_msg_unit_trained_t*) msg_bytes;

            switch (msg->type) {
            case UNIT_HEAVY_INFANTRY:
                data->army.heavy = msg->count;
                break;
            case UNIT_HORSEMEN:
                data->army.horsemen = msg->count;
                break;
            case UNIT_WORKER:
                data->army.workers = msg->count;
                break;
            case UNIT_LIGHT_INFANTRY:
                data->army.light = msg->count;
                break;
            }

            draw_msg_refresh_t out;
            out.id = DRAW_SERVER_REFRESH;
            msgsnd(draw_queue, &out, sizeof(draw_msg_refresh_t) - sizeof(long), 0);
        } else if (*type == GAMEQUEUE_MSG_EVENT_BATTLE) {
            event_msg_battle_t *msg = (event_msg_battle_t*) msg_bytes;

            data->points[1] = msg->enemy_score;
            data->points[0] = msg->your_score;
            data->army.heavy = msg->remaining.heavy;
            data->army.horsemen = msg->remaining.horsemen;
            data->army.workers = msg->remaining.workers;
            data->army.light = msg->remaining.light;

            draw_msg_stack_t out;
            out.id = DRAW_SERVER_STACK_APPEND;

            if (msg->result == BATTLE_RESULT_WIN) {
                snprintf(out.info, 255, "Wygrales bitwe (%s) ze stratami: l %d, c %d, j %d",
                         (msg->type == BATTLE_TYPE_ATTACK ? "atak" : "obrona"), msg->losses.light, msg->losses.heavy, msg->losses.horsemen);
            } else {
                snprintf(out.info, 255, "Przegrales bitwe (%s) ze stratami: l %d, c %d, j %d",
                         (msg->type == BATTLE_TYPE_ATTACK ? "atak" : "obrona"), msg->losses.light, msg->losses.heavy, msg->losses.horsemen);
            }

            msgsnd(draw_queue, &out, sizeof(draw_msg_stack_t) - sizeof(long), 0);
        } else if (*type == GAMEQUEUE_MSG_EVENT_RESOURCE_GAIN) {
            event_msg_resource_gain_t *msg = (event_msg_resource_gain_t*) msg_bytes;

            data->resources = msg->resources;

            draw_msg_refresh_t out;
            out.id = DRAW_SERVER_REFRESH;
            msgsnd(draw_queue, &out, sizeof(draw_msg_refresh_t) - sizeof(long), 0);
        } else if (*type == GAMEQUEUE_MSG_EVENT_GAME_FINISHED) {
            event_msg_game_finished_t *msg = (event_msg_game_finished_t*) msg_bytes;

            draw_msg_stack_t out;
            out.id = DRAW_SERVER_STACK_APPEND;

            if (msg->win == GAME_FINISH_WIN_YES) {
                snprintf(out.info, 30, "Wygrales gre!");
            } else {
                snprintf(out.info, 30, "Przegrales gre :(");
            }

            msgsnd(draw_queue, &out, sizeof(draw_msg_stack_t) - sizeof(long), 0);

            // podnosimy
            sem_post(&data->mutex);

            break;
        } else if (*type == GAMEQUEUE_MSG_EVENT_GAME_STARTED) {
            event_msg_game_started_t *msg = (event_msg_game_started_t*) msg_bytes;

            data->resources = msg->starting_resources;
            strncpy(data->enemy_name, msg->opponent_name, 32);
            data->game_started = 1;

            draw_msg_refresh_t out;
            out.id = DRAW_SERVER_REFRESH;
            msgsnd(draw_queue, &out, sizeof(draw_msg_refresh_t) - sizeof(long), 0);
        } else if (*type == GAMEQUEUE_MSG_SERVER_CMD_ATTACK_ACK) {
            cmd_msg_attack_ack_t *msg = (cmd_msg_attack_ack_t*) msg_bytes;

            data->army.heavy = msg->remaining.heavy;
            data->army.horsemen = msg->remaining.horsemen;
            data->army.workers = msg->remaining.workers;
            data->army.light = msg->remaining.light;

            draw_msg_stack_t out;
            out.id = DRAW_SERVER_STACK_APPEND;

            if (msg->status == GAMEQUEUE_STATUS_CMD_ATTACK_WORKERS) {
                snprintf(out.info, 50, "Nie mozesz wyslac robotnikow na walke");
            } else if (msg->status == GAMEQUEUE_STATUS_CMD_ATTACK_NO_UNITS) {
                snprintf(out.info, 50, "Nie masz tylu jednostek");
            } else if (msg->status == GAMEQUEUE_STATUS_CMD_ATTACK_OK) {
                snprintf(out.info, 50, "Atak wyslany pomyslnie");
            } else if (msg->status == GAMEQUEUE_STATUS_CMD_ATTACK_ALL_ZERO) {
                snprintf(out.info, 50, "Nie mozna wyslac 0 jednostek do walki");
            } else {
                snprintf(out.info, 50, "Nie udalo sie zlecic ataku z nieznanego powodu");
            }

            msgsnd(draw_queue, &out, sizeof(draw_msg_stack_t) - sizeof(long), 0);
        } else if (*type == GAMEQUEUE_MSG_SERVER_CMD_TRAIN_ACK) {
            cmd_msg_train_ack_t *msg = (cmd_msg_train_ack_t*) msg_bytes;

            data->resources = msg->resources;

            draw_msg_stack_t out;
            out.id = DRAW_SERVER_STACK_APPEND;

            if (msg->status == GAMEQUEUE_STATUS_CMD_TRAIN_NO_RES) {
                snprintf(out.info, 50, "Nie masz wystarczajaco surowcow na trening");
            } else if (msg->status == GAMEQUEUE_STATUS_CMD_TRAIN_UNKNOWN) {
                snprintf(out.info, 50, "Nie udalo sie zlecic treningu z nieznanego powodu");
            } else if (msg->status == GAMEQUEUE_STATUS_CMD_TRAIN_OK) {
                snprintf(out.info, 50, "Trening zlecony pomyslnie");
            }

            msgsnd(draw_queue, &out, sizeof(draw_msg_stack_t) - sizeof(long), 0);
        }

        // podnosimy
        sem_post(&data->mutex);
    }

    // koniec
    msgctl(client_queue, IPC_RMID, 0);
}

char pick_unit(int draw_queue)
{
    char unit = UNIT_MIN_VALUE;

    int chr = 0;

    draw_msg_unit_t drawmsg;
    drawmsg.id = DRAW_SERVER_UNIT_SELECT;

    do {
        if (chr == KEY_LEFT) {
            if (unit > UNIT_MIN_VALUE)
                unit--;
        } else if (chr == KEY_RIGHT) {
            if (unit < UNIT_MAX_VALUE)
                unit++;
        } else if (chr == KEY_ENTER || chr == '\n') {
            break;
        }

        if (unit == UNIT_LIGHT_INFANTRY) {
            strcpy(drawmsg.title, "Lekka");
        } else if (unit == UNIT_HEAVY_INFANTRY) {
            strcpy(drawmsg.title, "Ciezka");
        } else if (unit == UNIT_HORSEMEN) {
            strcpy(drawmsg.title, "Jezdzcy");
        } else {
            strcpy(drawmsg.title, "Robotnicy");
        }

        msgsnd(draw_queue, &drawmsg, sizeof(draw_msg_unit_t) - sizeof(long), 0);
    } while ((chr = getch()));

    return unit;
}

unsigned int pick_number(int draw_queue, const char *info)
{
    unsigned int number = 0;
    int chr = 0;

    draw_msg_number_t drawmsg;
    drawmsg.id = DRAW_SERVER_NUMBER_SELECT;
    strcpy(drawmsg.title, info);

    do {
        if (chr == KEY_LEFT) {
            if (number > 0)
                number--;
        } else if (chr == KEY_RIGHT) {
            number++;
        } else if (chr == KEY_ENTER || chr == '\n') {
            break;
        }

        drawmsg.number = number;
        msgsnd(draw_queue, &drawmsg, sizeof(draw_msg_number_t) - sizeof(long), 0);
    } while ((chr = getch()));

    return number;
}

int game_main_loop(int client_queue, int server_queue, char seat)
{
    // do sprzątania pozostałości
    signal(SIGINT, message_queue_cleanup);
    signal(SIGTERM, message_queue_cleanup);
    signal(SIGQUIT, message_queue_cleanup);
    cq = client_queue;

    // kolejka dla drawing servera i shm
    int draw_queue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0600);
    int shared_mem = shmget(IPC_PRIVATE, sizeof(event_listener_data_t), 0600);

    if (shared_mem < 0 || draw_queue < 0) {
        perror("Blad podczas tworzenia kolejki wew. klienta i/lub pamieci wspoldzielonej");
        return 1;
    }

    event_listener_data_t *eldata = shmat(shared_mem, 0, 0);

    // semafor dobrze by było już teraz zainicjować
    if (sem_init(&eldata->mutex, 1, 1) < 0) {
        perror("Nie udalo sie zainicjowac semafora");
        return 1;
    }

    // inicjalizacja
    initscr();
    raw();
    keypad(stdscr, 1);
    noecho();

    // proces który rysuje
    if (fork() == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        drawing_server_process(draw_queue, eldata);
        return 0;
    }

    // proces który odbiera wiadomości
    if (fork() == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        event_listener(draw_queue, eldata, client_queue, seat);
        return 0;
    }

    int option = 0;
    int chr = 0;

    draw_msg_mainmenu_t drawmsg;
    drawmsg.id = DRAW_SERVER_MAIN_MENU;

    do {
        if (eldata->game_started == 0)
            continue;

        if (chr == KEY_LEFT) {
            option--;
            if (option < 0)
                option = 0;
        } else if (chr == KEY_RIGHT) {
            option++;

            if (option > 2)
                option = 2;
        } else if (chr == KEY_ENTER || chr == '\n') {
            if (option == 0) {
                char unit = pick_unit(draw_queue);
                unsigned int count = pick_number(draw_queue, "Ilosc jednostek: ");

                cmd_msg_train_t msg;

                msg.id = GAMEQUEUE_MSG_CLIENT_CMD_TRAIN;
                msg.count = count;
                msg.training_id = rand(); // nie uzywane
                msg.type = unit;

                msgsnd(server_queue, &msg, sizeof(cmd_msg_train_t) - sizeof(long), 0);
            } else if (option == 1) {
                unsigned int light = pick_number(draw_queue, "Ilosc lekkiej: ");
                unsigned int heavy = pick_number(draw_queue, "Ilosc ciezkiej: ");
                unsigned int horsemen = pick_number(draw_queue, "Ilosc jezdzcow: ");

                cmd_msg_attack_t msg;

                msg.id = GAMEQUEUE_MSG_CLIENT_CMD_ATTACK;
                msg.army.heavy = heavy;
                msg.army.horsemen = horsemen;
                msg.army.light = light;
                msg.army.workers = 0;
                msg.attack_id = rand(); // nie uzywane

                msgsnd(server_queue, &msg, sizeof(cmd_msg_attack_t) - sizeof(long), 0);
            } else if (option == 2) {
                draw_msg_shutdown_t out;
                out.id = DRAW_SERVER_SHUTDOWN;
                msgsnd(draw_queue, &out, sizeof(draw_msg_shutdown_t) - sizeof(long), 0);

                break;
            }
        }

        if (option == 0) {
            strcpy(drawmsg.item, "Trenuj");
        } else if (option == 1) {
            strcpy(drawmsg.item, "Atakuj");
        } else {
            strcpy(drawmsg.item, "Koniec");
        }

        msgsnd(draw_queue, &drawmsg, sizeof(draw_msg_mainmenu_t) - sizeof(long), 0);
    } while ((chr = getch()));

    msgctl(client_queue, IPC_RMID, 0);
    msgctl(draw_queue, IPC_RMID, 0);
    shmctl(shared_mem, IPC_RMID, 0);
    sem_destroy(&eldata->mutex);
    endwin();

    return 0;
}
