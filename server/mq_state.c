#include "mq_state.h"

#include <string.h>

void init_seat(mq_seat_t *seat)
{
    seat->is_connected = 0;
    seat->client_queue = 0;
    seat->server_queue = 0;
    seat->server_fd = 0;

    memset(seat->player_name, 0, sizeof(seat->player_name));
    memset(seat->player_agent, 0, sizeof(seat->player_agent));
}

void init_state(mq_state_t *state)
{
    state->game_in_progress = 0;
    state->game_sid = 0;

    init_seat(&state->seats[0]);
    init_seat(&state->seats[1]);
}
