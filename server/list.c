#include "list.h"

#include <string.h>

// dodawanie elementu na liste
int list_add(list_t *list, void *item)
{
    if (list->capacity <= list->count) {
        return -1;
    }

    // znajdź wolne miejsce
    char *pointer = &list->items;

    for (unsigned int i = 0; i < list->capacity; i++, pointer += list->item_size) {
        // pierwszym elementem itemu listy jest is_valid (zawsze)
        // jeśli jest 0 to oznacza że miejsce jest wolne
        if (*pointer == 0) {
            memcpy(pointer, item, list->item_size);
            list->count++;
            return i;
        }
    }

    return -1;
}

// iterator listy
void *list_next(list_t *list, unsigned int *iterator)
{
    if (list->count <= 0)
        return 0;

    // pointer
    char *pointer = &list->items;
    pointer += list->item_size * *iterator;

    for (; *iterator < list->capacity; pointer += list->item_size) {
        (*iterator)++;

        if (*pointer == 1) {
            return pointer;
        }
    }

    // nic nie znaleziono już
    return 0;
}

void list_remove(list_t *list, char *item)
{
    // ustawiamy is_valid na 0 i podczas iteracji nie będzie już uwzględniane
    *item = 0;

    if (list->count > 0)
        list->count--;
}
