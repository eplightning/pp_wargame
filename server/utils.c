#include "utils.h"

#include <sys/errno.h>
#include <semaphore.h>
#include <sys/signal.h>

int sem_wait2(sem_t *sem)
{
    return sem_wait(sem);
}

int signal2(int signum, void (*action)(int))
{
    struct sigaction opts;
    struct sigaction old;

    opts.sa_handler = action;
    opts.sa_flags = SA_NOCLDWAIT | SA_NODEFER;

    sigemptyset(&opts.sa_mask);

    return sigaction(signum, &opts, &old);
}
