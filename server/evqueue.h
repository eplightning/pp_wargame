#ifndef EVQUEUE_H
#define EVQUEUE_H

#include <semaphore.h>

#define EVQUEUE_CAPACITY    255
#define EVQUEUE_ITEM_SIZE   255

// Type dla kolejki logicznej
#define EVQUEUE_LOGIC_TYPE_FIRST_TRAIN      1
#define EVQUEUE_LOGIC_TYPE_FIRST_ATTACK     2
#define EVQUEUE_LOGIC_TYPE_SECOND_TRAIN     100
#define EVQUEUE_LOGIC_TYPE_SECOND_ATTACK    101
#define EVQUEUE_LOGIC_TYPE_DIE              200

typedef struct evqueue_item {
    long type;
    long session_id;
    char data[EVQUEUE_ITEM_SIZE];
} evqueue_item_t;

typedef struct evqueue {
    unsigned int head;
    unsigned int tail;
    sem_t available_semaphore;
    sem_t access_semaphore;
    sem_t free_semaphore;
    evqueue_item_t items[EVQUEUE_CAPACITY];
} evqueue_t;

int evqueue_init(evqueue_t *q);
void evqueue_free(evqueue_t *q);
void evqueue_add(evqueue_t *q, long type, long sid, const void *item, size_t item_size);
evqueue_item_t *evqueue_get(evqueue_t *q, long max_sid);

#endif // EVQUEUE_H

