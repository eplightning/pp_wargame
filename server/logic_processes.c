#include "logic_processes.h"
#include "evqueue.h"
#include "mq_state.h"
#include "protocol.h"
#include "list.h"
#include "config.h"
#include "utils.h"
#include "game_stuff.h"

#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <sys/signal.h>

int keep_running = 1;
int data_fd = -1;

void logic_signal_handler(int type)
{
    if (type == SIGINT || type == SIGTERM || type == SIGQUIT) {
        keep_running = 0;

        if (data_fd >= 0)
            shmctl(data_fd, IPC_RMID, 0);
    }
}

void spawn_logic_process(server_config_t *config, long sid, mq_state_t *state, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue, int *children)
{
    // struktura współdzielona pomiędzy podprocesami logicznymi (procesowaniem komend i asynchronicznymi zdarzeniami)
    // przydział pamięci
    data_fd = shmget(IPC_PRIVATE, sizeof(game_data_t), 0600);

    if (data_fd < 0) {
        perror("Gra: Nie mozna utworzyc wspoldzielonej pamieci dla danych gry\n");
        return;
    }

    game_data_t *data = shmat(data_fd, 0, 0);

    // inicjalizacja
    if (init_game_data(config, data) < 0) {
        perror("Gra: Nie mozna utworzyc semafora dla danych gry\n");
        return;
    }

    int pid;

    // proces obsługujący logike dla komend wysyłanych przez graczy (a dokładniej przesyłanych nam przez proces odbioru danych graczy)
    if ((pid = fork()) == 0) {
        signal2(SIGINT, logic_signal_handler);
        signal2(SIGTERM, logic_signal_handler);
        signal2(SIGQUIT, logic_signal_handler);
        spawn_command_process(data, sid, player0_queue, player1_queue, logic_queue);
        exit(0);
        return;
    }

    if (pid < 0)
        exit(1);

    children[0] = pid;

    // proces wydarzeń asynchronicznych
    if ((pid = fork()) == 0) {
        signal2(SIGINT, logic_signal_handler);
        signal2(SIGTERM, logic_signal_handler);
        signal2(SIGQUIT, logic_signal_handler);
        spawn_events_process(data, sid, state, player0_queue, player1_queue, logic_queue);
        exit(0);
        return;
    }

    if (pid < 0)
        exit(1);

    children[1] = pid;
}

void spawn_command_process(game_data_t *data, long sid, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue)
{
    evqueue_item_t *item;

    while (keep_running) {
        item = evqueue_get(logic_queue, sid);

        // dead
        if (item == 0)
            break;

        // klient wysyła nam komendy zanim się gra zaczeła ...
        if (data->start_event_sent == 0)
            continue;

        // pozostała wiadomość po starej sesji, olewamy
        if (item->session_id < sid) {
            continue;
        // nowa sesja w trakcie, albo dostaliśmy polecenie żeby skończyć robote
        } else if (item->session_id > sid || item->type == EVQUEUE_LOGIC_TYPE_DIE) {
            // umieramy
            break;
        }

        // atak!
        if (item->type == EVQUEUE_LOGIC_TYPE_FIRST_ATTACK || item->type == EVQUEUE_LOGIC_TYPE_SECOND_ATTACK) {
            // bierzemy mutex (semafor) jako że będzie robić coś na danych gry
            if (sem_wait(&data->mutex) < 0) {
                break;
            }

            // ktory zlecil
            unsigned char seat = (item->type == EVQUEUE_LOGIC_TYPE_FIRST_ATTACK) ? 0 : 1;
            player_data_t *player = (seat == 0) ? &data->player0 : &data->player1;
            evqueue_t *playerq = (seat == 0) ? player0_queue : player1_queue;

            // struktura
            cmd_msg_attack_t *cmd = (cmd_msg_attack_t*) item->data;

            // wojska się zgadzają?
            if (cmd->army.heavy > player->army.heavy || cmd->army.horsemen > player->army.horsemen
                || cmd->army.light > player->army.light) {
                cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_NO_UNITS);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
            } else if (cmd->army.workers > 0) {
                cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_WORKERS);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
            } else if (cmd->army.heavy == 0 && cmd->army.horsemen == 0 && cmd->army.light == 0) {
                cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_ALL_ZERO);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
            } else if (data->attacks.count >= ATTACK_LIST_CAPACITY) {
                // brakuje miejsca na liscie atakow ...
                cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_UNKNOWN);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
            } else {
                // tworzymy atak
                attack_list_item_t attack;
                attack.is_valid = 1;
                attack.seconds_elapsed = 0;
                attack.attack_id = cmd->attack_id;
                attack.attacker = seat;
                attack.army.heavy = cmd->army.heavy;
                attack.army.horsemen = cmd->army.horsemen;
                attack.army.light = cmd->army.light;
                attack.army.workers = 0;

                // dodajemy na liste
                if (list_add((list_t*) &data->attacks, &attack) < 0) {
                    // brakuje miejsca na liscie atakow ...
                    cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_UNKNOWN);
                    evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
                } else {
                    // odejmujemy wojska
                    player->army.heavy -= cmd->army.heavy;
                    player->army.horsemen -= cmd->army.horsemen;
                    player->army.light -= cmd->army.light;

                    cmd_msg_attack_ack_t response = create_cmd_attack_ack(player, cmd->attack_id, GAMEQUEUE_STATUS_CMD_ATTACK_OK);
                    evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_attack_ack_t));
                }
            }

            // koniec modyfikacji
            sem_post(&data->mutex);
        // trening
        } else if (item->type == EVQUEUE_LOGIC_TYPE_FIRST_TRAIN || item->type == EVQUEUE_LOGIC_TYPE_SECOND_TRAIN) {
            // bierzemy mutex (semafor) jako że będzie robić coś na danych gry
            if (sem_wait(&data->mutex) < 0) {
                break;
            }

            // ktory zlecil
            unsigned char seat = (item->type == EVQUEUE_LOGIC_TYPE_FIRST_TRAIN) ? 0 : 1;
            player_data_t *player = (seat == 0) ? &data->player0 : &data->player1;
            evqueue_t *playerq = (seat == 0)  ? player0_queue : player1_queue;

            // struktura
            cmd_msg_train_t *cmd = (cmd_msg_train_t*) item->data;

            // koszt
            int cost = unit_calculate_cost(cmd->type, cmd->count);

            // dziwny typ albo miejsca brakuje
            if ((cmd->type > UNIT_MAX_VALUE || cmd->type < UNIT_MIN_VALUE) || data->trainings.count >= TRAIN_LIST_CAPACITY) {
                cmd_msg_train_ack_t response = create_cmd_train_ack(player, cmd->training_id, GAMEQUEUE_STATUS_CMD_TRAIN_UNKNOWN);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_train_ack_t));
            // zero jednostek
            } else if (cmd->count <= 0) {
                cmd_msg_train_ack_t response = create_cmd_train_ack(player, cmd->training_id, GAMEQUEUE_STATUS_CMD_TRAIN_UNKNOWN);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_train_ack_t));
            // surowce
            } else if (cost > player->resources) {
                cmd_msg_train_ack_t response = create_cmd_train_ack(player, cmd->training_id, GAMEQUEUE_STATUS_CMD_TRAIN_NO_RES);
                evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_train_ack_t));
            } else {
                // tworzymy trening
                train_list_item_t train;
                train.is_valid = 1;
                train.remaining = cmd->count;
                train.seconds_elapsed = 0;
                train.training_id = cmd->training_id;
                train.unit_type = cmd->type;
                train.player = seat;

                // dodajemy na liste
                if (list_add((list_t*) &data->trainings, &train) < 0) {
                    // brakuje miejsca na liscie treningów ...
                    cmd_msg_train_ack_t response = create_cmd_train_ack(player, cmd->training_id, GAMEQUEUE_STATUS_CMD_TRAIN_UNKNOWN);
                    evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_train_ack_t));
                } else {
                    // odejmujemy surowce
                    player->resources -= cost;

                    cmd_msg_train_ack_t response = create_cmd_train_ack(player, cmd->training_id, GAMEQUEUE_STATUS_CMD_TRAIN_OK);
                    evqueue_add(playerq, response.id, sid, &response, sizeof(cmd_msg_train_ack_t));
                }
            }

            // koniec modyfikacji
            sem_post(&data->mutex);
        }
    }
}

void spawn_events_process(game_data_t *data, long sid, mq_state_t *state, evqueue_t *player0_queue, evqueue_t *player1_queue, evqueue_t *logic_queue)
{
    // po pierwsze wysyłamy info że gra się zaczeła
    event_msg_game_started_t started;
    started.id = GAMEQUEUE_MSG_EVENT_GAME_STARTED;
    started.starting_resources = data->player0.resources;
    strncpy(started.opponent_name, state->seats[1].player_name, 32);
    evqueue_add(player0_queue, started.id, sid, &started, sizeof(event_msg_game_started_t));

    started.starting_resources = data->player1.resources;
    strncpy(started.opponent_name, state->seats[0].player_name, 32);
    evqueue_add(player1_queue, started.id, sid, &started, sizeof(event_msg_game_started_t));

    data->start_event_sent = 1;

    // zaczynamy clocki
    double accumulator = 0.0;
    double time_old;
    struct timespec timer;
    keep_running = 1;

    clock_gettime(CLOCK_REALTIME, &timer);
    time_old = (timer.tv_sec) + ((double) timer.tv_nsec / 1E9);

    while (keep_running) {
        // pobieramy obecny czas
        double time_new;
        clock_gettime(CLOCK_REALTIME, &timer);
        time_new = (timer.tv_sec) + ((double) timer.tv_nsec / 1E9);

        // dodajemy do akumulatora i ustawiamy nowy czas jako stary
        accumulator += (time_new - time_old);
        time_old = time_new;

        // nie mineła sekunda? to śpimy (1 sekunda - akumulator)
        if (accumulator < 1.0) {
            usleep(1E6 - (accumulator * 1E6));
            continue;
        }

        // wybiła sekunda ....
        accumulator -= 1.0;

        // bierzemy mutex
        if (sem_wait(&data->mutex) < 0) {
            break;
        }

        // dodajemy surowce
        data->player0.resources += calculate_resource_gain(data->player0.army.workers);
        data->player1.resources += calculate_resource_gain(data->player1.army.workers);

        send_event_resgain(&data->player0, player0_queue, sid);
        send_event_resgain(&data->player1, player1_queue, sid);

        // trening jednostek
        if (data->trainings.count > 0) {
            unsigned int iterator = 0;

            train_list_item_t *training = 0;

            while ((training = list_next((list_t*) &data->trainings, &iterator)) != 0) {
                training->seconds_elapsed++;

                if (unit_train_time(training->unit_type) <= training->seconds_elapsed) {
                    training->seconds_elapsed = 0;
                    training->remaining--;

                    player_data_t *player;
                    evqueue_t *playerq;
                    unsigned int units = 0;

                    if (training->player == MAINQUEUE_SEAT_FIRST) {
                        player = &data->player0;
                        playerq = player0_queue;
                    } else {
                        player = &data->player1;
                        playerq = player1_queue;
                    }

                    switch (training->unit_type) {
                    case UNIT_WORKER:
                        units = ++player->army.workers;
                        break;
                    case UNIT_LIGHT_INFANTRY:
                        units = ++player->army.light;
                        break;
                    case UNIT_HEAVY_INFANTRY:
                        units = ++player->army.heavy;
                        break;
                    case UNIT_HORSEMEN:
                        units = ++player->army.horsemen;
                    }

                    send_event_trained(playerq, training, units, sid);

                    if (training->remaining <= 0) {
                        list_remove((list_t*) &data->trainings, (char*) training);
                    }
                }
            }
        }

        // walka...
        if (data->attacks.count > 0) {
            unsigned int iterator = 0;

            attack_list_item_t *attack = 0;

            while ((attack = list_next((list_t*) &data->attacks, &iterator)) != 0) {
                attack->seconds_elapsed++;

                if (attack->seconds_elapsed < attack_time()) {
                    continue;
                }

                // okreslamy kto atakuje kto sie broni
                player_data_t *attacker;
                player_data_t *defender;
                evqueue_t *attackerq;
                evqueue_t *defenderq;

                if (attack->attacker == MAINQUEUE_SEAT_FIRST) {
                    attacker = &data->player0;
                    defender = &data->player1;
                    attackerq = player0_queue;
                    defenderq = player1_queue;
                } else {
                    attacker = &data->player1;
                    defender = &data->player0;
                    attackerq = player1_queue;
                    defenderq = player0_queue;
                }

                // liczymy moc
                double attack_ap = attack_power(&attack->army);
                double attack_def = attack_defense(&attack->army);
                double defense_ap = attack_power(&defender->army);
                double defense_def = attack_defense(&defender->army);

                // sprawdzamy rezultat i dodajemy punkt
                char win = 0; // defender win

                if (attack_ap > defense_def) {
                    attacker->points++;
                    win = 1;
                }

                // straty
                war_army_t attack_losses = attack_getloss(defense_ap / attack_def, &attack->army);
                war_army_t defense_losses = attack_getloss(attack_ap / defense_def, &defender->army);

                // liczymy łączną pozostałą armie
                war_army_t attack_remaining;
                war_army_t defense_remaining;

                // obecna armia + powracająca armia (czyli wysłana armia - straty)
                attack_remaining.workers = attacker->army.workers;
                attack_remaining.heavy = attacker->army.heavy + attack->army.heavy - attack_losses.heavy;
                attack_remaining.light = attacker->army.light + attack->army.light - attack_losses.light;
                attack_remaining.horsemen = attacker->army.horsemen + attack->army.horsemen - attack_losses.horsemen;

                // obecna armia - straty
                defense_remaining.workers = defender->army.workers; // ?
                defense_remaining.heavy = defender->army.heavy - defense_losses.heavy;
                defense_remaining.light = defender->army.light - defense_losses.light;
                defense_remaining.horsemen = defender->army.horsemen - defense_losses.horsemen;

                // wszystkie obliczenia zrobione, wysyłamy eventy do graczy
                send_event_battle(attacker, attackerq, attack, &attack->army, &defender->army, &attack_losses, &attack_remaining,
                                  BATTLE_TYPE_ATTACK, win == 1 ? BATTLE_RESULT_WIN : BATTLE_RESULT_LOSS, defender->points, sid);
                send_event_battle(defender, defenderq, attack, &defender->army, &attack->army, &defense_losses, &defense_remaining,
                                  BATTLE_TYPE_DEFENSE, win == 0 ? BATTLE_RESULT_WIN : BATTLE_RESULT_LOSS, attacker->points, sid);

                // koniec gry?
                if (attacker->points >= GAME_WIN_POINTS) {
                    // wysyłamy eventy które powiadomią klientów o końcu gru, dodatkowo ubije to procesy komunikacji z graczami
                    send_event_end(attackerq, GAME_FINISH_REASON_NORMAL, GAME_FINISH_WIN_YES, sid);
                    send_event_end(defenderq, GAME_FINISH_REASON_NORMAL, GAME_FINISH_WIN_NO, sid);

                    // to zakończy proces logiczny przetwarzający komendy
                    evqueue_add(logic_queue, EVQUEUE_LOGIC_TYPE_DIE, sid, 0, 0);

                    // interakcja z główną kolejką
                    state->seats[0].is_connected = 0;
                    state->seats[1].is_connected = 0;
                    state->game_in_progress = 0;

                    printf("LP: Sesja gry zakonczona\n");

                    // koniec zabawy
                    keep_running = 0;
                } else {
                    // swap wojska
                    attacker->army.workers = attack_remaining.workers;
                    attacker->army.heavy = attack_remaining.heavy;
                    attacker->army.light = attack_remaining.light;
                    attacker->army.horsemen = attack_remaining.horsemen;
                    defender->army.workers = defense_remaining.workers;
                    defender->army.heavy = defense_remaining.heavy;
                    defender->army.light = defense_remaining.light;
                    defender->army.horsemen = defense_remaining.horsemen;

                    // usuwamy
                    list_remove((list_t*) &data->attacks, (char*) attack);
                }
            }
        }

        // oddajemy resources dla drugiego procesu logicznego
        sem_post(&data->mutex);
    }
}

void send_event_end(evqueue_t *playerq, int reason, char win, long sid)
{
    event_msg_game_finished_t event;

    event.id = GAMEQUEUE_MSG_EVENT_GAME_FINISHED;
    event.reason = reason;
    event.win = win;

    evqueue_add(playerq, event.id, sid, &event, sizeof(event_msg_game_finished_t));
}

void send_event_battle(player_data_t *player, evqueue_t *playerq, attack_list_item_t *attack, war_army_t *your, war_army_t *enemy, war_army_t *losses, war_army_t *remaining, char type, char result, int enemy_score, long sid)
{
    event_msg_battle_t event;

    // główne
    event.id = GAMEQUEUE_MSG_EVENT_BATTLE;
    event.attack_id = attack->attack_id;
    event.enemy_score = enemy_score;
    event.your_score = player->points;
    event.type = type;
    event.result = result;

    // wojska
    event.armies[0].heavy = your->heavy;
    event.armies[0].horsemen = your->horsemen;
    event.armies[0].light = your->light;
    event.armies[0].workers = your->workers;
    event.armies[1].heavy = enemy->heavy;
    event.armies[1].horsemen = enemy->horsemen;
    event.armies[1].light = enemy->light;
    event.armies[1].workers = enemy->workers;

    // pozostałości
    event.remaining.heavy = remaining->heavy;
    event.remaining.horsemen = remaining->horsemen;
    event.remaining.light = remaining->light;
    event.remaining.workers = remaining->workers;

    // straty
    event.losses.heavy = losses->heavy;
    event.losses.horsemen = losses->horsemen;
    event.losses.light = losses->light;
    event.losses.workers = losses->workers;

    evqueue_add(playerq, event.id, sid, &event, sizeof(event_msg_battle_t));
}


void send_event_trained(evqueue_t *playerq, train_list_item_t *training, unsigned int units, long sid)
{
    event_msg_unit_trained_t event;

    event.id = GAMEQUEUE_MSG_EVENT_UNIT_TRAINED;
    event.training_id = training->training_id;
    event.type = training->unit_type;
    event.count = units;

    evqueue_add(playerq, event.id, sid, &event, sizeof(event_msg_unit_trained_t));
}

void send_event_resgain(player_data_t *player, evqueue_t *playerq, long sid)
{
    event_msg_resource_gain_t event;

    event.id = GAMEQUEUE_MSG_EVENT_RESOURCE_GAIN;
    event.resources = player->resources;

    evqueue_add(playerq, event.id, sid, &event, sizeof(event_msg_resource_gain_t));
}

cmd_msg_attack_ack_t create_cmd_attack_ack(player_data_t *player, int attack_id, int status)
{
    cmd_msg_attack_ack_t response;

    response.id = GAMEQUEUE_MSG_SERVER_CMD_ATTACK_ACK;
    response.attack_id = attack_id;
    response.status = status;
    response.remaining.heavy = player->army.heavy;
    response.remaining.horsemen = player->army.horsemen;
    response.remaining.light = player->army.light;
    response.remaining.workers = player->army.workers;

    return response;
}

cmd_msg_train_ack_t create_cmd_train_ack(player_data_t *player, int training_id, int status)
{
    cmd_msg_train_ack_t response;

    response.id = GAMEQUEUE_MSG_SERVER_CMD_TRAIN_ACK;
    response.training_id = training_id;
    response.status = status;
    response.resources = player->resources;

    return response;
}

int init_game_data(server_config_t *config, game_data_t *data)
{
    // semafor
    if (sem_init(&data->mutex, 1, 1) < 0) {
        return -1;
    }

    // dane gracza #1
    data->player0.army.heavy = 0;
    data->player0.army.horsemen = 0;
    data->player0.army.light = 0;
    data->player0.army.workers = 0;
    data->player0.resources = config->start_resources;
    data->player0.points = 0;

    // dane gracza #2
    data->player1.army.heavy = 0;
    data->player1.army.horsemen = 0;
    data->player1.army.light = 0;
    data->player1.army.workers = 0;
    data->player1.resources = config->start_resources;
    data->player1.points = 0;

    // listy
    data->attacks.capacity = ATTACK_LIST_CAPACITY;
    data->attacks.count = 0;
    data->attacks.item_size = sizeof(attack_list_item_t);
    data->trainings.capacity = TRAIN_LIST_CAPACITY;
    data->trainings.count = 0;
    data->trainings.item_size = sizeof(train_list_item_t);

    // i info
    data->start_event_sent = 0;

    // done
    return 0;
}
