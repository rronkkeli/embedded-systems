/* My goal is to get full grades in Embedded Programming courses, because it will help me
 * to better understand embedded systems programming and Zephyr RTOS, and possibly
 * open up career opportunities in embedded systems development in the future. I see
 * this skill as a valuable asset in the tech industry.
*/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/timing/timing.h>

#include "ledctl.h"
#include "buttons.h"
#include "dispatcher.h"
#include "mux.h"
#include "debug.h"

int main(void)
{
    // Initialize timing timer and start it. Leaving this running does not actually cost anything for us
    timing_init();
    timing_start();

    timing_t start = timing_counter_get();

    if (!init_leds()) {
        return 0;
    }
    DBG("Initialized leds\n");
    
    if (!init_buttons()) {
        return 0;
    }
    DBG("Initialized buttons\n");
    
    if (!init_uart()) {
        return 0;
    }
    DBG("Initialized uart\n");

    for (int i = 0; i < 3; i++) {
        k_sem_take(&threads_ready, K_FOREVER);
        DBG("Sem: %d\n", i);
        k_msleep(100);
    }

    // Try to start the sequence until it starts. Do not block because signals are not
    // preserved if the receiver is not listening yet.
    DBG("Trying to start a sequence");
    k_mutex_lock(&lmux, K_FOREVER);
    k_condvar_broadcast(&rsig);
    k_condvar_wait(&sig_ok, &lmux, K_FOREVER);
    k_mutex_unlock(&lmux);

    uint64_t elapsed = timing_cycles_to_ns(timing_counter_get() - start);
    DBG("OK!\nMain thread done! Took: %lld ns\n", elapsed);

    return 0;
}
