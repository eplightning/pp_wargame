#include "utils.h"

#include <sys/signal.h>

// signal bez ustawiania SA_RESTART
int signal2(int signum, void (*action)(int))
{
    struct sigaction opts;
    struct sigaction old;

    opts.sa_handler = action;
    opts.sa_flags = SA_NOCLDWAIT | SA_NODEFER;

    sigemptyset(&opts.sa_mask);

    return sigaction(signum, &opts, &old);
}
