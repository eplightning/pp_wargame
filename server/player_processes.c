#include "player_processes.h"
#include "evqueue.h"
#include "protocol.h"

#include <unistd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <stdlib.h>
#include <stdio.h>

int keep_running_player = 1;

void player_signal_handler(int type)
{
    if (type == SIGINT || type == SIGTERM || type == SIGQUIT) {
        keep_running_player = 0;
    }
}

void spawn_player_process(long sid, int client_fd, int server_fd, evqueue_t *player_queue, evqueue_t *logic_queue, unsigned char seat, int *children)
{
    int pid;

    // Pierwszy podproces tego procesu oczekuje na wiadomości od gracza (komendy)
    if ((pid = fork()) == 0) {
        signal(SIGINT, player_signal_handler);
        signal(SIGTERM, player_signal_handler);
        signal(SIGQUIT, player_signal_handler);
        player_process_incoming(sid, server_fd, logic_queue, seat);
        exit(0);
        return;
    }

    if (pid < 0)
        exit(1);
    children[seat + 2] = pid;

    // Drugi podproces tego procesu czyta wew. kolejke serwera player_queue (którą wypełnia układ logiczny)
    // i wysyła do użytkownika wiadomości z niej
    if ((pid = fork()) == 0) {
        signal(SIGINT, player_signal_handler);
        signal(SIGTERM, player_signal_handler);
        signal(SIGQUIT, player_signal_handler);
        player_process_outgoing(sid, client_fd, server_fd, player_queue);
        exit(0);
        return;
    }

    if (pid < 0)
        exit(1);

    children[seat + 4] = pid;
}

void player_process_incoming(long sid, int server_fd, evqueue_t *logic_queue, unsigned char seat)
{
    int status;
    char data[GAMEQUEUE_MAX_CMD_SIZE];

    while (keep_running_player) {
        status = msgrcv(server_fd, &data, GAMEQUEUE_MAX_CMD_LENGTH, 0, MSG_NOERROR);

        if (status < 0) {
            if (errno == EINTR) {
                continue;
            }

            break;
        }

        // pierwsze pole jest zawsze msg_id, więc możemy z lenistwa takie coś
        long *msg_id = (long*) data;

        if (*msg_id == GAMEQUEUE_MSG_CLIENT_CMD_TRAIN) {
            cmd_msg_train_t *train_msg = (cmd_msg_train_t*) data;

            long type;

            if (seat == MAINQUEUE_SEAT_FIRST) {
                type = EVQUEUE_LOGIC_TYPE_FIRST_TRAIN;
            } else {
                type = EVQUEUE_LOGIC_TYPE_SECOND_TRAIN;
            }

            evqueue_add(logic_queue, type, sid, train_msg, sizeof(cmd_msg_train_t));
        } else if (*msg_id == GAMEQUEUE_MSG_CLIENT_CMD_ATTACK) {
            cmd_msg_attack_t *attack_msg = (cmd_msg_attack_t*) data;

            long type;

            if (seat == MAINQUEUE_SEAT_FIRST) {
                type = EVQUEUE_LOGIC_TYPE_FIRST_ATTACK;
            } else {
                type = EVQUEUE_LOGIC_TYPE_SECOND_ATTACK;
            }

            evqueue_add(logic_queue, type, sid, attack_msg, sizeof(cmd_msg_attack_t));
        }
    }
}

void player_process_outgoing(long sid, int client_fd, int server_fd, evqueue_t *player_queue)
{
    evqueue_item_t *item;

    while (keep_running_player) {
        item = evqueue_get(player_queue, sid);

        // blad
        if (item == 0)
            break;

        // ignorujemy zdarzenia z session id starszym od nas
        if (item->session_id < sid) {
            continue;
        }
        // jeśli dostajemy zdarzenie z session id młodszym od nas to znaczy że gra już się skończyła a my z jakiegoś powodu się nie zamkneliśmy?
        else if (item->session_id > sid) {
            // usuwamy kolejke (co sprawi że proces incoming też się zamknie)
            msgctl(server_fd, IPC_RMID, 0);
            break;
        }

        if (item->type == GAMEQUEUE_MSG_EVENT_GAME_FINISHED) {
            // wysyłamy zdarzenie
            msgsnd(client_fd, item->data, GAMEQUEUE_MAX_MESSAGE_LENGTH, 0);

            // koniec naszej roboty
            break;
        } else {
            // pozostałe wydarzenia przekazuje graczowi bez zmian
            msgsnd(client_fd, item->data, GAMEQUEUE_MAX_MESSAGE_LENGTH, 0);
        }
    }

    msgctl(server_fd, IPC_RMID, 0);
}
