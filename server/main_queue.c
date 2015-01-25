#include "main_queue.h"
#include "protocol.h"
#include "mq_state.h"
#include "evqueue.h"
#include "player_processes.h"
#include "logic_processes.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/signal.h>

// modyfikowane przez sygnaly
int keep_running_mq = 1;

int start_main_queue(server_config_t config)
{
    int status;
    int children[6] = {0};

    // sygnaly
    signal(SIGCHLD, SIG_IGN);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGQUIT, signal_handler);

    // tworzymy glowna kolejke
    int mainq = msgget(config.main_queue_key, IPC_CREAT | IPC_EXCL | 0666);

    if (mainq < 0) {
        if (errno == EEXIST) {
            perror("BLAD: Kolejka wiadomosci z takim kluczem juz istnieje. Uzyj innej kolejki lub usun ja korzystajac z komendy ipcrm");
        } else {
            perror("BLAD: Nie mozna utworzyc kolejki wiadomosci");
        }

        return 1;
    }

    //
    // Dzielona pamięć
    //

    // stan gry dla głównego procesu (głównie interesuje nas czy gra jest w toku)
    int state_mem = shmget(IPC_PRIVATE, sizeof(mq_state_t), 0600 | IPC_CREAT);

    if (state_mem < 0) {
        perror("BLAD: Nie mozna utworzyc pamieci wspoldzielonej dla glownego stanu gry");
        return 2;
    }

    mq_state_t *state = shmat(state_mem, 0, 0);
    init_state(state);
    state->main_queue = mainq;

    // kolejki nasze serwerowe
    int evq_mem = shmget(IPC_PRIVATE, sizeof(evqueue_t) * 3, 0600 | IPC_CREAT);

    if (evq_mem < 0) {
        msgctl(mainq, IPC_RMID, 0);
        perror("BLAD: Nie mozna utworzyc pamieci wspoldzielonej dla kolejek wew.");
        return 3;
    }

    evqueue_t *player_queue[2];
    evqueue_t *logic_queue;

    player_queue[0] = shmat(evq_mem, 0, 0);
    player_queue[1] = player_queue[0] + 1;
    logic_queue = player_queue[1] + 1;

    if (evqueue_init(player_queue[0]) < 0 || evqueue_init(player_queue[1]) < 0 || evqueue_init(logic_queue) < 0) {
        msgctl(mainq, IPC_RMID, 0);
        shmctl(state_mem, IPC_RMID, 0);
        perror("BLAD: Nie mozna utworzyc semaforow dla kolejek wew. serwera");
        return 4;
    }

    // nastepny ID sesji
    long next_sid = 0; // inkrementowane gdy laczy sie pierwszy z dwoch graczy

    printf("MQ: Glowna kolejka gotowa do odbierania wiadomosci\n");

    // glowna petla glownego procesu serwera
    while (keep_running_mq) {
        // ten proces odbiera wylacznie prosby o dolaczenie
        mq_msg_join_t join_msg;
        status = msgrcv(mainq, &join_msg, sizeof(mq_msg_join_t) - sizeof(long), MAINQUEUE_MSG_JOIN, 0);

        if (status < 0) {
            if (errno == EINTR) {
                continue;
            } else {
                perror("BLAD: Glowny proces serwera ma problemy z kolejka\n");
                break;
            }
        }

        // gra w toku
        if (state->game_in_progress == 1 || (state->seats[0].is_connected && state->seats[1].is_connected)) {
            printf("MQ: Odrzucono polaczenie ze wzgledu na trwajaca juz gre\n");
            join_ack_send_error(mainq, join_msg.request_id, MAINQUEUE_STATUS_JOIN_GAME_IN_PROGRESS);
            continue;
        }

        // bledy wejscia
        if (strnlen(join_msg.player_agent, 34) > 32 || strnlen(join_msg.player_name, 34) > 32 || join_msg.preferred_seat > 1 || join_msg.preferred_seat < 0) {
            printf("MQ: Odrzucono polaczenie ze wzgledu na nieprawidlowe dane\n");
            join_ack_send_error(mainq, join_msg.request_id, MAINQUEUE_STATUS_JOIN_UNKNOWN);
            continue;
        }

        // sprawdzamy ktore miejsce wolne
        unsigned char seat;

        if (!state->seats[0].is_connected && !state->seats[1].is_connected) {
            seat = join_msg.preferred_seat;
            next_sid++; // jeśli nikt nie był połączony to zaczynamy nową sesje
        } else {
            seat = state->seats[0].is_connected ? MAINQUEUE_SEAT_SECOND : MAINQUEUE_SEAT_FIRST;
        }

        // ustawiamy wartosci dla miejsca
        strcpy(state->seats[seat].player_name, join_msg.player_name);
        strcpy(state->seats[seat].player_agent, join_msg.player_agent);

        // tworzymy dwie kolejki
        int client_fd;

        status = find_and_create_queue(&state->seats[seat].client_queue, &client_fd, &config, 0);

        if (status < 0) {
            perror("BLAD: Nie mozna utworzyc kolejki klient <- serwer\n");
            join_ack_send_error(mainq, join_msg.request_id, MAINQUEUE_STATUS_JOIN_UNKNOWN);
            break;
        }

        status = find_and_create_queue(&state->seats[seat].server_queue, &state->seats[seat].server_fd, &config, 1);

        if (status < 0) {
            perror("BLAD: Nie mozna utworzyc kolejki serwer <- klient\n");
            msgctl(client_fd, IPC_RMID, 0);
            join_ack_send_error(mainq, join_msg.request_id, MAINQUEUE_STATUS_JOIN_UNKNOWN);
            break;
        }

        // Sukces, tworzymy procesy komunikacji z klientem
        printf("MQ: Dolaczyl gracz \"%s\" uzywajacy klienta \"%s\" na miejsce %d\n", state->seats[seat].player_name
                                                                                 , state->seats[seat].player_agent
                                                                                 , seat);
        state->seats[seat].is_connected = 1;

        spawn_player_process(next_sid, client_fd, state->seats[seat].server_fd, player_queue[seat], logic_queue, seat, children);

        // Wysylamy informacje do klienta o sukcesie
        mq_msg_join_ack_t ack;

        ack.client_queue = state->seats[seat].client_queue;
        ack.server_queue = state->seats[seat].server_queue;
        ack.seat = seat;
        ack.id = join_msg.request_id;
        ack.status = MAINQUEUE_STATUS_JOIN_OK;

        msgsnd(mainq, &ack, sizeof(mq_msg_join_ack_t) - sizeof(long), 0);

        // Rozpoczynamy gre?
        if (state->seats[0].is_connected && state->seats[1].is_connected) {
            printf("MQ: Dwa miejsce zajete, startowanie sesji gry nr. %ld ...\n", next_sid);
            state->game_in_progress = 1;
            state->game_sid = next_sid;
            spawn_logic_process(&config, next_sid, state, player_queue[0], player_queue[1], logic_queue, children);
        }
    }

    printf("MQ: Koniec serwera");

    // usuwamy IPC
    evqueue_free(player_queue[0]);
    evqueue_free(player_queue[1]);
    evqueue_free(logic_queue);

    shmctl(state_mem, IPC_RMID, 0);
    shmctl(evq_mem, IPC_RMID, 0);
    msgctl(mainq, IPC_RMID, 0);

    if (state->seats[0].server_fd > 0)
        msgctl(state->seats[0].server_fd, IPC_RMID, 0);

    if (state->seats[1].server_fd > 0)
        msgctl(state->seats[1].server_fd, IPC_RMID, 0);

    // ubijamy dzieci
    kill_children(children);

    return 0;
}

void signal_handler(int type)
{
    if (type == SIGINT || type == SIGTERM || type == SIGQUIT) {
        keep_running_mq = 0;
    }
}

void kill_children(int *children)
{
    for (int i = 0; i < 6; i++) {
        if (children[i] > 0) {
            kill(children[i], SIGTERM);
        }
    }
}

int find_and_create_queue(key_t *key, int *fd, server_config_t *config, char server)
{
    key_t i = server ? config->main_queue_key + 2000 : config->main_queue_key + 2100;
    key_t end = i + 100;

    for (; i <= end; i++) {
        int status = msgget(i, 0666 | IPC_CREAT | IPC_EXCL);

        if (status > -1) {
            *fd = status;
            *key = i;

            return 0;
        }
    }

    return -1;
}

void join_ack_send_error(int queue, long request_id, int status)
{
    mq_msg_join_ack_t ack;

    ack.client_queue = 0;
    ack.server_queue = 0;
    ack.seat = 0;
    ack.id = request_id;
    ack.status = status;

    msgsnd(queue, &ack, sizeof(mq_msg_join_ack_t) - sizeof(long), 0);
}
