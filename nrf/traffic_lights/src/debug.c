#include <zephyr/kernel.h>

#include "debug.h"
#include "mux.h"

uint64_t rtime = 0;
uint64_t ytime = 0;
uint64_t gtime = 0;

K_THREAD_DEFINE(debugth, 512, debug_task, &rtime, &ytime, &gtime, 2, 0, 0);

void debug_task(uint64_t *r, uint64_t *y, uint64_t *g) {
    uint64_t sequence_elapsed;
    int counter = 0;

    while (1) {
        // Check if all led tasks have updated their runtime variables and print the information about sequence time
        if (atomic_cas(&ltime_set, 7, 0)) {
            k_mutex_lock(&lmux, K_FOREVER);
            printk("Red task took: %lld ns\n", *r);
            printk("Yellow task took: %lld ns\n", *y);
            printk("Green task took: %lld ns\n", *g);
            printk("Sequence took: %lld ns\n", *r + *y + *g);
            k_mutex_unlock(&lmux);

            counter++;
        }

        if (counter == 5) {
            break;
        } else {
            k_msleep(100);
        }
    }
    
}
