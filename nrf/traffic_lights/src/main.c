/* My goal is to get full grades in Embedded Programming courses, because it will help me
 * to better understand embedded systems programming and Zephyr RTOS, and possibly
 * open up career opportunities in embedded systems development in the future. I see
 * this skill as a valuable asset in the tech industry.
*/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "ledctl.h"

// Manual drive button is button 0
#define MANUAL DT_ALIAS(sw0)
// Switch to color or turn current color off on manual drive
#define RED_TOGGLE DT_ALIAS(sw1)
#define YELLOW_TOGGLE DT_ALIAS(sw2)
#define GREEN_TOGGLE DT_ALIAS(sw3)
// Toggle blink yellow on manual drive
#define YBLINK_TOGGLE DT_ALIAS(sw4)

// Prevent noise and constant interruptions from button presses
#define DEBOUNCE_TIME_MS 200

// Bind manual drive state to button 0 and create a callback structure
static const struct gpio_dt_spec manual_button = GPIO_DT_SPEC_GET(MANUAL, gpios);
static struct gpio_callback manual_cb_data;

// Bind manual toggle color buttons to buttons 2, 3, 4 and 5 and create callback structures
static const struct gpio_dt_spec red_toggle = GPIO_DT_SPEC_GET(RED_TOGGLE, gpios);
static struct gpio_callback red_cb_data;
static const struct gpio_dt_spec yellow_toggle = GPIO_DT_SPEC_GET(YELLOW_TOGGLE, gpios);
static struct gpio_callback yellow_cb_data;
static const struct gpio_dt_spec green_toggle = GPIO_DT_SPEC_GET(GREEN_TOGGLE, gpios);
static struct gpio_callback green_cb_data;
static const struct gpio_dt_spec yblink_toggle = GPIO_DT_SPEC_GET(YBLINK_TOGGLE, gpios);
static struct gpio_callback yblink_cb_data;

// Interrupt callback for the buttons
void manual_isr(void);
void red_toggle_isr(void);
void yellow_toggle_isr(void);
void green_toggle_isr(void);
void yblink_toggle_isr(void);

// Timestamp of the latest button push
volatile static uint32_t latest_push = 0;

// Current state variables initialized to red
enum State state = Red;
enum State cont = Red;
enum Color color = LRed;

// Stack size and priority for task threads
#define STACKSIZE 512
#define PRIORITY 5

// Define tasks for each color
K_THREAD_DEFINE(redth, STACKSIZE,
		red, &state, &color, &paused,
		PRIORITY, 0, 0);

K_THREAD_DEFINE(yellowth, STACKSIZE,
                yellow, &state, &color, &paused,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(greenth, STACKSIZE,
                green, &state, &color, &paused,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(yblinkth, STACKSIZE,
                yblink, &state, &color, &paused,
                PRIORITY, 0, 0);

// Helper functions and variable to enable an disable interrupts for pause button
// and to detect if the interrupts are enabled or disabled
static bool interrupt_enabled = true;
void interrupt_enable(void);
void interrupt_disable(void);

int main(void)
{
        if (!init_leds()) {
                return 0;
        }

        // Check that buttons are ready
        if (!gpio_is_ready_dt(&manual_button) || !gpio_is_ready_dt(&red_toggle) ||
            !gpio_is_ready_dt(&yellow_toggle) || !gpio_is_ready_dt(&green_toggle) ||
            !gpio_is_ready_dt(&yblink_toggle)) {
                printf("Error: Button device not ready\n");
                return 0;
        }

        // Set led pins as output and buttons as input
	if (gpio_pin_configure_dt(&manual_button, GPIO_INPUT | GPIO_PULL_UP) < 0
            || gpio_pin_configure_dt(&red_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
            || gpio_pin_configure_dt(&yellow_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
            || gpio_pin_configure_dt(&green_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
            || gpio_pin_configure_dt(&yblink_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0) {
                printf("Error: Failed to configure IO\n");
		return 0;
	}

        // Finally configure the button to interrupt and..
        if (gpio_pin_interrupt_configure_dt(&manual_button, GPIO_INT_EDGE_TO_INACTIVE) < 0 ||
            gpio_pin_interrupt_configure_dt(&red_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
            gpio_pin_interrupt_configure_dt(&yellow_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
            gpio_pin_interrupt_configure_dt(&green_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
            gpio_pin_interrupt_configure_dt(&yblink_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0) {
                printf("Error: Failed to configure button interrupt\n");
                return 0;
        }

        // ..add the interrupt handlers
        gpio_init_callback(&manual_cb_data, manual_isr, BIT(manual_button.pin));
        gpio_init_callback(&red_cb_data, red_toggle_isr, BIT(red_toggle.pin));
        gpio_init_callback(&yellow_cb_data, yellow_toggle_isr, BIT(yellow_toggle.pin));
        gpio_init_callback(&green_cb_data, green_toggle_isr, BIT(green_toggle.pin));
        gpio_init_callback(&yblink_cb_data, yblink_toggle_isr, BIT(yblink_toggle.pin));

        if (gpio_add_callback(manual_button.port, &manual_cb_data) < 0 ||
            gpio_add_callback(red_toggle.port, &red_cb_data) < 0 || 
            gpio_add_callback(yellow_toggle.port, &yellow_cb_data) < 0 ||
            gpio_add_callback(green_toggle.port, &green_cb_data) < 0 ||
            gpio_add_callback(yblink_toggle.port, &yblink_cb_data) < 0) {
                printf("Error: Failed to add button callback(s)\n");
                return 0;
        }

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


void interrupt_enable(void)
{
        if (!interrupt_enabled && latest_push + DEBOUNCE_TIME_MS < k_uptime_get_32())
        {
                interrupt_enabled = true;
                gpio_pin_interrupt_configure_dt(&manual_button, GPIO_INT_EDGE_TO_ACTIVE);

                if (paused) {
                        // If we are in manual mode, we need to reconfigure the toggles
                        gpio_pin_interrupt_configure_dt(&red_toggle, GPIO_INT_EDGE_TO_ACTIVE);
                        gpio_pin_interrupt_configure_dt(&yellow_toggle, GPIO_INT_EDGE_TO_ACTIVE);
                        gpio_pin_interrupt_configure_dt(&green_toggle, GPIO_INT_EDGE_TO_ACTIVE);
                        gpio_pin_interrupt_configure_dt(&yblink_toggle, GPIO_INT_EDGE_TO_ACTIVE);
                }
        }
}

void interrupt_disable(void)
{
        interrupt_enabled = false;
        gpio_pin_interrupt_configure_dt(&manual_button, GPIO_INT_DISABLE);
        gpio_pin_interrupt_configure_dt(&red_toggle, GPIO_INT_DISABLE);
        gpio_pin_interrupt_configure_dt(&yellow_toggle, GPIO_INT_DISABLE);
        gpio_pin_interrupt_configure_dt(&green_toggle, GPIO_INT_DISABLE);
        gpio_pin_interrupt_configure_dt(&yblink_toggle, GPIO_INT_DISABLE);
}

void manual_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        paused = !paused;
        latest_push = k_uptime_get_32();

        // If we are pausing, save the current state and set state to MANUAL
        if (paused) {
                cont = state;
                state = Manual;
                // Pause color changing tasks
                k_thread_suspend(redth);
                k_thread_suspend(yellowth);
                k_thread_suspend(greenth);
                printf("Manual control\n");
        // If we are unpausing, restore the saved state
        } else {
                state = cont;
                // Resume color changing tasks
                k_thread_resume(redth);
                k_thread_resume(yellowth);
                k_thread_resume(greenth);
                printf("Automatic\n");
        }
}

void red_toggle_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        latest_push = k_uptime_get_32();

        // Only do something if we are paused
        if (paused) {
                if (state == Yblink) {
                        state = Manual;
                        printf("Toggling YELLOW BLINK OFF\n");
                }


                if (color == LRed) {
                        color = LOff;
                        set_off();
                } else {
                        color = LRed;
                        set_red();
                }

                printf("Toggling RED\n");
        }
}

void yellow_toggle_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        latest_push = k_uptime_get_32();

        if (paused) {
                if (state == Yblink) {
                        state = Manual;
                        printf("Toggling YELLOW BLINK OFF\n");
                }

                if (color == LYellow) {
                        color = LOff;
                        set_off();
                } else {
                        color = LYellow;
                        set_yellow();
                }
        }

}

void green_toggle_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        latest_push = k_uptime_get_32();

        if (paused) {
                if (state == Yblink) {
                        state = Manual;
                        printf("Toggling YELLOW BLINK OFF\n");
                }
                
                if (color == LGreen) {
                        color = LOff;
                        set_off();
                } else {
                        color = LGreen;
                        set_green();
                }

                printf("Toggling GREEN\n");
        }
}

void yblink_toggle_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        latest_push = k_uptime_get_32();

        if (paused) {
                // Toggle blinking yellow mode
                if (state != Yblink) {
                        state = Yblink;
                        printf("Toggling YELLOW BLINK ON\n");
                } else {
                        state = Manual;
                        printf("Toggling YELLOW BLINK OFF\n");
                }
        }
}