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