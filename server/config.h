#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>

#define WARSRV_VERSION  "0.1"

typedef struct server_config {
    key_t main_queue_key;
    int start_resources;
} server_config_t;

#endif // CONFIG_H

