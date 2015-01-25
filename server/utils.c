#include "utils.h"

#include <sys/errno.h>
#include <semaphore.h>

int sem_wait2(sem_t *sem)
{
    return sem_wait(sem);
    /*int status;

    while ((status = sem_wait(sem)) < 0) {
        if (errno == EINTR) {
            continue;
        }

        break;
    }

    return status;*/
}
