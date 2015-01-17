#ifndef LOGIC_PROCESSES_H
#define LOGIC_PROCESSES_H

#include "evqueue.h"
#include "mq_state.h"
#include "protocol.h"
#include "list.h"
#include "config.h"

#include <semaphore.h>

#define ATTACK_LIST_CAPACITY    255
#define TRAIN_LIST_CAPACITY     255

typedef struct player_data {
    int resources;
    int points;
    war_army_t army;
} player_data_t;

typedef struct attack_list_item {
    // elementy struktury list_t
    char is_valid;
    // elementy ataku
    int attack_id;
    unsigned int seconds_elapsed;
    war_army_t army;
    char attacker;
} attack_list_item_t;

typedef struct train_list_item {
    // elementy struktury list_t
    char is_valid;
    // elementy treningu
    int training_id;
    unsigned int seconds_elapsed;
    char unit_type;
    unsigned int remaining;
    char player;
} train_list_item_t;

typedef struct attack_list {
    // elementy struktury list_t
    unsigned int count;
    unsigned int item_size;
    unsigned int capacity;
    // ataki
    attack_list_item_t attacks[ATTACK_LIST_CAPACITY];
} attack_list_t;

typedef struct train_list {
    // elementy struktury list_t
    unsigned int count;
    unsigned int item_size;
    unsigned int capacity;
    // treningi
    train_list_item_t trainings[TRAIN_LIST_CAPACITY];
} train_list_t;

typedef struct game_data {
    player_data_t player0;
    player_data_t player1;
    attack_list_t attacks;
    train_list_t trainings;
    sem_t mutex;
    char start_event_sent;
} game_data_t;

void send_event_trained(evqueue_t *playerq, train_list_item_t *training, unsigned int units, long sid);
void send_event_resgain(player_data_t *player, evqueue_t *playerq, long sid);
void send_event_battle(player_data_t *player, evqueue_t *playerq, attack_list_item_t *attack, war_army_t *your, war_army_t *enemy, war_army_t *losses, war_army_t *remaining, char type, char result, int enemy_score, long sid);
void send_event_end(evqueue_t *playerq, int reason, char win, long sid);

cmd_msg_attack_ack_t create_cmd_attack_ack(player_data_t *player, int attack_id, int status);
cmd_msg_train_ack_t create_cmd_train_ack(player_data_t *player, int training_id, int status);

int init_game_data(server_config_t *config, game_data_t *data);
void spawn_logic_process(server_config_t *config, long sid, mq_state_t *state, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue, int *children);
void spawn_command_process(game_data_t *data, long sid, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue);
void spawn_events_process(game_data_t *data, long sid, mq_state_t *state, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue);

#endif // LOGIC_PROCESSES_H

