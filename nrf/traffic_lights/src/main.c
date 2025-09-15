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

	while (1) {
                // Hold the current state (called here because first state is already declared)
                k_msleep(HOLD_TIME_MS);

                // The simplest state machine ever
		switch (state)
                {
                case Red:
                        state = Yellow;
                        printf("Switching to YELLOW\n");
                        break;
                case Yellow:
                        state = Green;
                        printf("Switching to GREEN\n");
                        break;
                case Green:
                        state = Red;
                        printf("Switching to RED\n");
                        break;
                default:
                        break;
                }

                // Re-enable interrupts if they were disabled (the function checks)
                interrupt_enable();

                // Yield so color changing tasks may run
		k_yield();
	}
	return 0;
}


