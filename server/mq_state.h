#ifndef SEAT
#define SEAT

#include <sys/types.h>

typedef struct mq_seat {
    char is_connected;
    char player_name[33];
    char player_agent[33];
    key_t client_queue;
    key_t server_queue;
    int server_fd;
} mq_seat_t;

typedef struct mq_state {
    int main_queue;
    char game_in_progress;
    long game_sid;
    mq_seat_t seats[2];
} mq_state_t;

void init_seat(mq_seat_t *seat);
void init_state(mq_state_t *state);

#endif // SEAT

