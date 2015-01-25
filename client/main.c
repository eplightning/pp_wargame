#include "protocol.h"
#include "game_screen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

void get_player_name(char *buffer)
{
    int i = 0;

    printf("# Podaj nazwe gracza ([1-32] znaki): \n");

    while (i < 32 && read(0, &buffer[i], 1) > 0) {
        if (buffer[i] == '\n') {
            if (i <= 0) {
                printf("# Podaj nazwe gracza ([1-32] znaki): \n");
                continue;
            }

            break;
        }

        i++;
    }

    for (; i < 33; i++) {
        buffer[i] = 0;
    }
}

char get_preffered_seat()
{
    printf("# Podaj preferowana pozycje (0 - pierwszy gracz, 1 - drugi): \n");

    char seat = 0;

    read(0, &seat, 1);

    if (seat == 0) {
        read(0, &seat, 1);
        return MAINQUEUE_SEAT_FIRST;
    }

    read(0, &seat, 1);
    return MAINQUEUE_SEAT_SECOND;
}

int establish_mq_connection()
{
    key_t key;

    printf("# Numer kolejki [puste zeby uzyc domyslnego]: \n");

    char number[11];
    int i = 0;

    while (i < 11 && read(0, &number[i], 1) > 0) {
        if (number[i] == '\n') {
            break;
        }

        i++;
    }

    if (i <= 0) {
        key = MAINQUEUE_DEFAULT_KEY;
    } else {
        number[i] = 0;
        key = atoi(number);

        if (key <= 0)
            key = MAINQUEUE_DEFAULT_KEY;
    }

    printf("Laczenie z kolejka %d ...\n", key);

    int mq = msgget(key, 0);

    if (mq < 0) {
        perror("Blad podczas laczenie z kolejka");
        return -1;
    }

    return mq;
}

int ask_server_to_join(int main_queue, char *player_name, char preffered_seat, mq_msg_join_ack_t *response)
{
    // na ten id odpowie nam
    long reqid = MAINQUEUE_REQUEST_ID_MIN + rand();

    // tworzymy wiadomosc
    mq_msg_join_t msg;
    msg.id = MAINQUEUE_MSG_JOIN;
    msg.preferred_seat = preffered_seat;
    msg.request_id = reqid;
    strcpy(msg.player_name, player_name);
    strcpy(msg.player_agent, "wg_client 0.1");

    int status;

    // wysylamy
    status = msgsnd(main_queue, &msg, sizeof(mq_msg_join_t) - sizeof(long), 0);

    if (status < 0) {
        perror("Nie mozna wyslac join requesta");
        return status;
    }

    // odbieramy
    status = msgrcv(main_queue, response, sizeof(mq_msg_join_ack_t) - sizeof(long), reqid, 0);

    if (status < 0)
        perror("Nie mozna odebrac odpowiedzi na join requesta");

    return status;
}

int main()
{
    signal(SIGCHLD, SIG_IGN);
    srand(time(NULL));

    // wiadomość powitalna
    printf("wg_client 0.1 by Bartosz Slawianowski\n");

    // pobieramy nazwe gracza
    char player_name[33];

    get_player_name(player_name);

    // pobieramy seat
    char preffered = get_preffered_seat();

    // próbujemy się połączyć
    mq_msg_join_ack_t message;
    message.status = MAINQUEUE_STATUS_JOIN_UNKNOWN;

    do {
        int main_queue = establish_mq_connection();

        if (main_queue < 0)
            continue;

        int ask_status = ask_server_to_join(main_queue, player_name, preffered, &message);

        if (ask_status < 0)
            continue;

        if (message.status == MAINQUEUE_STATUS_JOIN_UNKNOWN) {
            printf("Serwer odrzucil nasze zadanie z powodem nieznanym\n");
        } else if (message.status == MAINQUEUE_STATUS_JOIN_GAME_IN_PROGRESS) {
            printf("Serwer odrzucil nasze zadanie z powodu trwajacej juz gry\n");
        }
    } while(message.status != MAINQUEUE_STATUS_JOIN_OK);

    printf("Pomyslnie dolaczono do gry, seat: %d\n", message.seat);

    // laczymy z kolejka kliencka
    int client_queue = msgget(message.client_queue, 0);

    if (client_queue < 0) {
        perror("Nie mozna bylo sie polaczyc z kolejka kliencka");
        return 1;
    }

    // laczymy z kolejka serwerowa
    int server_queue = msgget(message.server_queue, 0);

    if (server_queue < 0) {
        perror("Nie mozna bylo sie polaczyc z kolejka serwerowa");
        return 2;
    }

    // teraz przekazuje kontrole do interfejsu
    return game_main_loop(client_queue, server_queue, message.seat);
}
