/* My goal is to get full grades in Embedded Programming courses, because it will help me
 * to better understand embedded systems programming and Zephyr RTOS, and possibly
 * open up career opportunities in embedded systems development in the future. I see
 * this skill as a valuable asset in the tech industry.
*/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "ledctl.h"
#include "buttons.h"
#include "dispatcher.h"
#include "mux.h"

int main(void)
{
    if (!init_leds()) {
        return 0;
    }
    printk("Initialized leds\n");
    
    if (!init_buttons()) {
        return 0;
    }
    printk("Initialized buttons\n");
    
    if (!init_uart()) {
        return 0;
    }
    printk("Initialized uart\n");

    for (int i = 0; i < 3; i++) {
        k_sem_take(&threads_ready, K_FOREVER);
        printk("Sem: %d\n", i);
        k_msleep(100);
    }

    // Try to start the sequence until it starts. Do not block because signals are not
    // preserved if the receiver is not listening yet.
    printk("Trying to start a sequence");
    k_mutex_lock(&lmux, K_FOREVER);
    k_condvar_broadcast(&rsig);
    k_condvar_wait(&sig_ok, &lmux, K_FOREVER);
    k_mutex_unlock(&lmux);

    printk("OK!\nMain thread done!\n");

    return 0;
}


