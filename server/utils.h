#ifndef UTILS_H
#define UTILS_H

#include <semaphore.h>

int sem_wait2(sem_t *sem);

int signal2(int signum, void (*action)(int));

#endif // UTILS_H

