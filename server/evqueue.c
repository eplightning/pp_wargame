#include "evqueue.h"
#include "utils.h"

#include <string.h>
#include <semaphore.h>

int evqueue_init(evqueue_t *q)
{
    q->head = 0;
    q->tail = 0;
    int status = sem_init(&q->access_semaphore, 1, 1);
    int status2 = sem_init(&q->available_semaphore, 1, 0);
    int status3 = sem_init(&q->free_semaphore, 1, EVQUEUE_CAPACITY);

    if (status < 0 || status2 < 0 || status3 < 0) {
        return -1;
    }

    return 0;
}

void evqueue_free(evqueue_t *q)
{
    sem_destroy(&q->access_semaphore);
    sem_destroy(&q->available_semaphore);
    sem_destroy(&q->free_semaphore);
}

void evqueue_add(evqueue_t *q, long type, long sid, const void *item, size_t item_size)
{
    if (item_size > EVQUEUE_ITEM_SIZE)
        return;

    int state = sem_wait2(&q->free_semaphore);

    if (state < 0)
        return;

    state = sem_wait2(&q->access_semaphore);

    if (state < 0)
        return;

    unsigned int old_head = q->head++;

    if (q->head >= EVQUEUE_CAPACITY)
        q->head = 0;

    q->items[old_head].type = type;
    q->items[old_head].session_id = sid;

    if (item_size > 0)
        memcpy(q->items[old_head].data, item, item_size);

    sem_post(&q->access_semaphore);
    sem_post(&q->available_semaphore);
}

evqueue_item_t *evqueue_get(evqueue_t *q, long max_sid)
{
    int state = sem_wait2(&q->available_semaphore);

    if (state < 0)
        return 0;

    state = sem_wait2(&q->access_semaphore);

    if (state < 0)
        return 0;

    evqueue_item_t *item = &q->items[q->tail];

    // takie małe zabezpieczenie żeby kolejki ze starych sesji
    // które z jakiegoś powodu się nie zamkneły
    // nie popsuły nam nowszych sesji gry
    if (item->session_id <= max_sid) {
        q->tail++;

        if (q->tail >= EVQUEUE_CAPACITY)
            q->tail = 0;

        sem_post(&q->access_semaphore);
        sem_post(&q->free_semaphore);
    } else {
        sem_post(&q->access_semaphore);
        sem_post(&q->available_semaphore);
    }

    return item;
}
