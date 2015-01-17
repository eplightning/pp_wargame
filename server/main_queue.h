#ifndef MAIN_QUEUE_H
#define MAIN_QUEUE_H

#include "config.h"

void signal_handler(int type);
void kill_children(int *children);
int start_main_queue(server_config_t config);
int find_and_create_queue(key_t *key, int *fd, server_config_t *config, char server);
void join_ack_send_error(int queue, long request_id, int status);

#endif // MAIN_QUEUE_H

