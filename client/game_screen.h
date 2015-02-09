#ifndef GAME_SCREEN_H
#define GAME_SCREEN_H

#include "protocol.h"

#include <semaphore.h>

typedef struct event_listener_data {
    char enemy_name[33];
    int points[2];
    int resources;
    war_army_t army;
    char seat;
    char game_started;
    sem_t mutex;
} event_listener_data_t;

int game_main_loop(int client_queue, int server_queue, char seat);

#endif // GAME_SCREEN_H

