#ifndef LIST_H
#define LIST_H

typedef struct list {
    unsigned int count;
    unsigned int item_size;
    unsigned int capacity;
    char items;
} list_t;

int list_add(list_t *list, void *item);
void *list_next(list_t *list, unsigned int *iterator);
void list_remove(list_t *list, char *item);

#endif // LIST_H

