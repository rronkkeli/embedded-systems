#include <zephyr/kernel.h>

#include "mux.h"

// Define mutex for every color to be used with signals
K_MUTEX_DEFINE(rmux);
K_MUTEX_DEFINE(ymux);
K_MUTEX_DEFINE(gmux);

// Define signals for each color
K_CONDVAR_DEFINE(rsig);
K_CONDVAR_DEFINE(ysig);
K_CONDVAR_DEFINE(gsig);

// FIFO for hold time
K_FIFO_DEFINE(ht_fifo);