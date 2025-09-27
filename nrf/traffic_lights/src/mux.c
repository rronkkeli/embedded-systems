#include <zephyr/kernel.h>

#include "mux.h"

// Define mutex
K_MUTEX_DEFINE(lmux);

// Define signals for each color
K_CONDVAR_DEFINE(rsig);
K_CONDVAR_DEFINE(ysig);
K_CONDVAR_DEFINE(gsig);
K_CONDVAR_DEFINE(sig_ok);

// FIFO for hold time
K_FIFO_DEFINE(ht_fifo);

// Define semaphore for each led task
K_SEM_DEFINE(threads_ready, 0, 3);

/*
    Each led task can set a bit in this atomic variable to signal debug task that they have updated the runtime.

    The debug task constantly checks this variable and prints the debug info when all data has been updated.

    Led tasks don't update the values until the debug task zeroes this variable.
*/
atomic_t ltime_set = 0;