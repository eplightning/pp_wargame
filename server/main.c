#include "config.h"
#include "protocol.h"
#include "main_queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Entry point serwera, wczytuje konfiguracje i przekazuje kontrole dalej
int main(int argc, char *argv[])
{
    printf("WarSrv %s by Bartosz Slawianowski\n", WARSRV_VERSION);

    server_config_t config;

    config.main_queue_key = MAINQUEUE_DEFAULT_KEY;
    config.start_resources = GAME_DEFAULT_RESOURCES;

    for (int i = 1; i < argc; i++) {
        if (strcmp("-queue", argv[i]) == 0 && i + 1 < argc) {
            config.main_queue_key = atoi(argv[i + 1]);
        } else if (strcmp("-start-resources", argv[i]) == 0 && i + 1 < argc) {
            config.start_resources = atoi(argv[i + 1]);
        }
    }

    printf("Trwa uruchamianie serwera z nastepujaca konfiguracja: \n");
    printf("    Number kolejki: %d\n", config.main_queue_key);
    printf("    Poczatkowe surowce: %d\n", config.start_resources);

    int status = start_main_queue(config);

    printf("Serwer zakonczyl prace ze statusem %d\n", status);

    return status;
}

