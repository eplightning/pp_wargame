#ifndef PLAYER_PROCESSES_H
#define PLAYER_PROCESSES_H

#include "evqueue.h"

void player_process_incoming(long sid, int server_fd, evqueue_t *logic_queue, unsigned char seat);
void player_process_outgoing(long sid, int client_fd, int server_fd, evqueue_t *player_queue);

void spawn_player_process(long sid, int client_fd, int server_fd, evqueue_t *player_queue, evqueue_t *logic_queue, unsigned char seat, int *children);

#endif // PLAYER_PROCESSES_H

