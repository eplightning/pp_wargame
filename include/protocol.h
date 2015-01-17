#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <sys/types.h>

// pomocnicze struktury i definicje
#define GAME_DEFAULT_RESOURCES  300
#define GAME_PROTOCOL_VERSION   1
#define GAME_WIN_POINTS         5

// ---------------------
// główna kolejka serwera
// ---------------------

// pomocnicze struktury i definicje
#define MAINQUEUE_DEFAULT_KEY                   117000
#define MAINQUEUE_SEAT_FIRST                    0
#define MAINQUEUE_SEAT_SECOND                   1

// join request (client->server)
#define MAINQUEUE_MSG_JOIN                      1
#define MAINQUEUE_REQUEST_ID_MIN                100
#define MAINQUEUE_STATUS_JOIN_OK                0
#define MAINQUEUE_STATUS_JOIN_GAME_IN_PROGRESS  -1
#define MAINQUEUE_STATUS_JOIN_UNKNOWN           -2

typedef struct mq_msg_join {
    long id; // MAINQUEUE_MSG_JOIN
    long request_id; // ID wiadomości jaką ma wysłać serwer
    char preferred_seat; // preferowany numer gracza MAINQUEUE_SEAT_*
    char player_name[33]; // NULL-terminated nazwa gracza (max 32 znaki, gdyż jeden musi być NULL-em)
    char player_agent[33]; // NULL-terminated nazwa programu klienta (max 32 znaki, gdyż jeden musi być NULL-em)
} mq_msg_join_t;

typedef struct mq_msg_join_ack {
    long id; // request_id komendy join
    int status; // MAINQUEUE_STATUS_JOIN_*
    // poniższe pola są wypełnianie tylko w przypadku MAINQUEUE_STATUS_JOIN_OK
    key_t client_queue; // klucz kolejki z której wiadomości odbiera klient, usuwa klient
    key_t server_queue; // klucz kolejki z której wiadomości odbiera serwer, usuwa serwer
    char seat; // numer gracza MAINQUEUE_SEAT_*
} mq_msg_join_ack_t;

// ---------------------
// komunikacja z graczem
// ---------------------

// pomocnicze struktury i definicje
#define UNIT_MIN_VALUE                  0
#define UNIT_LIGHT_INFANTRY             0
#define UNIT_HEAVY_INFANTRY             1
#define UNIT_HORSEMEN                   2
#define UNIT_WORKER                     3
#define UNIT_MAX_VALUE                  3

#define GAMEQUEUE_MAX_MESSAGE_LENGTH    (sizeof(event_msg_battle_t) - sizeof(long))
#define GAMEQUEUE_MAX_MESSAGE_SIZE      (sizeof(event_msg_battle_t))
#define GAMEQUEUE_MAX_CMD_LENGTH        (sizeof(cmd_msg_attack_t) - sizeof(long))
#define GAMEQUEUE_MAX_CMD_SIZE          (sizeof(cmd_msg_attack_t))

typedef struct war_army {
    unsigned int light;
    unsigned int heavy;
    unsigned int horsemen;
    unsigned int workers;
} war_army_t;

// ---------------------
// komendy
// ---------------------

// komenda treningu jednostek
#define GAMEQUEUE_MSG_CLIENT_CMD_TRAIN      500
#define GAMEQUEUE_MSG_SERVER_CMD_TRAIN_ACK  600
#define GAMEQUEUE_STATUS_CMD_TRAIN_OK       0
#define GAMEQUEUE_STATUS_CMD_TRAIN_NO_RES   -1
#define GAMEQUEUE_STATUS_CMD_TRAIN_UNKNOWN  -2

typedef struct cmd_msg_train {
    long id; // GAMEQUEUE_MSG_CLIENT_CMD_TRAIN
    int training_id; // ID treningu (potem należy go przesłać w eventach unit_trained)
    char type; // UNIT_*
    unsigned int count; // ilość jednostek do wytrenowania
} cmd_msg_train_t;

typedef struct cmd_msg_train_ack {
    long id; // GAMEQUEUE_MSG_SERVER_CMD_TRAIN_ACK
    int training_id; // ID treningu (to wyżej)
    int status; // GAMEQUEUE_STATUS_CMD_TRAIN_*
    int resources; // ilość surowców pozostała
} cmd_msg_train_ack_t;

// komenda wysłania wojsk
#define GAMEQUEUE_MSG_CLIENT_CMD_ATTACK         501
#define GAMEQUEUE_MSG_SERVER_CMD_ATTACK_ACK     601
#define GAMEQUEUE_STATUS_CMD_ATTACK_OK          0
#define GAMEQUEUE_STATUS_CMD_ATTACK_NO_UNITS    -1
#define GAMEQUEUE_STATUS_CMD_ATTACK_WORKERS     -2
#define GAMEQUEUE_STATUS_CMD_ATTACK_ALL_ZERO    -3
#define GAMEQUEUE_STATUS_CMD_ATTACK_UNKNOWN     -4

typedef struct cmd_msg_attack {
    long id; // GAMEQUEUE_MSG_CLIENT_CMD_ATTACK
    int attack_id; // ID walki (potem należy go przesłać w eventach battle)
    war_army_t army; // armia którą chcemy wysłać
} cmd_msg_attack_t;

typedef struct cmd_msg_attack_ack {
    long id; // GAMEQUEUE_MSG_SERVER_CMD_ATTACK_ACK
    int attack_id; // ID walki (to wyżej)
    int status; // GAMEQUEUE_STATUS_CMD_ATTACK_*
    war_army_t remaining; // pozostała dostępna armia
} cmd_msg_attack_ack_t;

// ---------------------
// wydarzenia
// ---------------------

// początek gry
#define GAMEQUEUE_MSG_EVENT_GAME_STARTED    100

typedef struct event_msg_game_started {
    long id; // GAMEQUEUE_MSG_EVENT_GAME_STARTED
    char opponent_name[33]; // nazwa przeciwnika
    int starting_resources; // surowce z którymi zaczyna gracz (domyślnie GAME_DEFAULT_RESOURCES)
} event_msg_game_started_t;

// koniec gry
#define GAMEQUEUE_MSG_EVENT_GAME_FINISHED   101
#define GAME_FINISH_REASON_NORMAL           0
#define GAME_FINISH_REASON_ERROR            -1
#define GAME_FINISH_WIN_YES                 1
#define GAME_FINISH_WIN_NO                  0

typedef struct event_msg_game_finished {
    long id; // GAMEQUEUE_MSG_EVENT_GAME_FINISHED
    int reason; // GAME_FINISH_REASON_*
    char win; // GAME_FINISH_WIN_*
} event_msg_game_finished_t;

// przyrost surowców
#define GAMEQUEUE_MSG_EVENT_RESOURCE_GAIN   102

typedef struct event_msg_resource_gain {
    long id; // GAMEQUEUE_MSG_EVENT_RESOURCE_GAIN
    int resources; // ilość surowców (łączna)
} event_msg_resource_gain_t;

// jednostka zbudowana
#define GAMEQUEUE_MSG_EVENT_UNIT_TRAINED      103

typedef struct event_msg_unit_trained {
    long id; // GAMEQUEUE_MSG_EVENT_UNIT_TRAINED
    int training_id; // ID treningu podany w komendzie train
    char type; // UNIT_*
    unsigned int count; // ilość dostępnych jednostek tego typu (łączna)
} event_msg_unit_trained_t;

// rezultat bitwy
#define GAMEQUEUE_MSG_EVENT_BATTLE          104
#define BATTLE_TYPE_ATTACK                  1
#define BATTLE_TYPE_DEFENSE                 0
#define BATTLE_RESULT_WIN                   1
#define BATTLE_RESULT_LOSS                  0

typedef struct event_msg_battle {
    long id; // GAMEQUEUE_MSG_EVENT_BATTLE
    int attack_id; // ID ataku podany w komendzie attack
    char type; // BATTLE_TYPE_*
    char result; // BATTLE_RESULT_*
    war_army_t armies[2]; // jednostki które brały udział w bitwie: armies[0] -> twoja armia, armies[1] -> armia przeciwnika
    war_army_t losses; // poniesione straty (twojej strony)
    war_army_t remaining; // cała armia która pozostała i jest dostępna w mieście po zakończeniu tej walki
    int your_score; // wynik "twoj"
    int enemy_score; // wynik gracza drugiego
} event_msg_battle_t;

#endif // PROTOCOL_H

