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

    printk("Address of red signal: %x\n", &rsig);
    printk("Address of yellow signal: %x\n", &ysig);
    printk("Address of green signal: %x\n", &gsig);

    for (int i = 0; i < 3; i++) {
        k_sem_take(&threads_ready, K_FOREVER);
        printk("Sem: %d\n", i);
        k_yield();
    }

    while (1) {
        // Re-enable interrupts if they were disabled (the function checks)
        interrupt_enable();
        if (state == Auto && !paused) {
            k_mutex_lock(&lmux, K_FOREVER);
            printk("Signal sent to ");

            switch (color) {
                case Red:
                    k_condvar_signal(&rsig);
                    printk("Red");
                    break;
                case Yellow:
                    k_condvar_signal(&ysig);
                    printk("Yellow");
                    break;
                default:
                    k_condvar_signal(&gsig);
                    printk("Green");
                    break;
            }

            printk(" from main\n");
            k_mutex_unlock(&lmux);
        }
        
        k_msleep(HOLD_TIME_MS);
    }

    return 0;
}


