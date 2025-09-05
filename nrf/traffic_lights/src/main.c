/* My goal is to get full grades in Embedded Programming courses, because it will help me
 * to better understand embedded systems programming and Zephyr RTOS, and possibly
 * open up career opportunities in embedded systems development in the future. I see
 * this skill as a valuable asset in the tech industry.
*/

#include <zephyr/kernel.h>

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <core_cm33.h>
// #include <zephyr/sys/printk.h>

// Red and Green are on board leds but yellow needs to be created by combining red and green.
#define RED DT_ALIAS(led0)
#define GREEN DT_ALIAS(led1)
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

// Bind to leds and tract their state
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN, gpios);
bool red_state = true; // Red is on in the beginning
bool green_state = false;

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

// Function prototypes for color tasks
void red(void *, void *, void *);
void yellow(void *, void *, void *);
void green(void *, void *, void *);
void yblink(void *, void *, void *);

// This simplifies the state machine syntax (instead of using integers)
enum Sate { RED, YELLOW, GREEN, YBLINK, MANUAL } typedef State;

// Current state variables initialized to red
volatile static State state = RED;
volatile static State cont = RED;

// Is the state machine paused?
volatile static bool paused = false;

// Timestamp of the latest button push
volatile static uint32_t latest_push = 0;

// Set transition time between colors
#define HOLD_TIME_MS 1000

// Stack size and priority for task threads
#define STACKSIZE 512
#define PRIORITY 5

// Define tasks for each color
K_THREAD_DEFINE(redth, STACKSIZE,
		red, NULL, NULL, NULL,
		PRIORITY, 0, 0);

K_THREAD_DEFINE(yellowth, STACKSIZE,
                yellow, NULL, NULL, NULL,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(greenth, STACKSIZE,
                green, NULL, NULL, NULL,
                PRIORITY, 0, 0);

K_THREAD_DEFINE(yblinkth, STACKSIZE,
                yblink, NULL, NULL, NULL,
                PRIORITY, 0, 0);

// Helper functions and variable to enable an disable interrupts for pause button
// and to detect if the interrupts are enabled or disabled
static bool interrupt_enabled = true;
void interrupt_enable(void);
void interrupt_disable(void);

int main(void)
{
        // Check that on-board leds are ready
        if (!gpio_is_ready_dt(&red_led) || !gpio_is_ready_dt(&green_led)) {
                printf("Error: LED device(s) not ready\n");
                return 0;
        }
        // Check that buttons are ready
        if (!gpio_is_ready_dt(&manual_button) || !gpio_is_ready_dt(&red_toggle) ||
            !gpio_is_ready_dt(&yellow_toggle) || !gpio_is_ready_dt(&green_toggle) ||
            !gpio_is_ready_dt(&yblink_toggle)) {
                printf("Error: Button device not ready\n");
                return;
        }

        // Set led pins as output and buttons as input
	if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&manual_button, GPIO_INPUT | GPIO_PULL_UP) < 0
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
                case RED:
                        state = YELLOW;
                        printf("Switching to YELLOW\n");
                        break;
                case YELLOW:
                        state = GREEN;
                        printf("Switching to GREEN\n");
                        break;
                case GREEN:
                        state = RED;
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

void red(void *, void *, void *)
{
        // Loop forever
        while (1) {
                // Check if we need to turn on red and green off
                if (state == RED && (!red_state || green_state)) {
                        red_state = true;
                        green_state = false;
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 0);
                }

                k_yield();
        }
}

void yellow(void *, void *, void *)
{
        while (1) {
                if (state == YELLOW && (!red_state || !green_state)) {
                        red_state = true;
                        green_state = true;
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void green(void *, void *, void *)
{
        while (1) {
                if (state == GREEN && (red_state || !green_state)) {
                        red_state = false;
                        green_state = true;
                        gpio_pin_set_dt(&red_led, 0);
                        gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void yblink(void *, void *, void *)
{
        int toggled = 0;

        while (1) {
                if (state == YBLINK && paused && k_uptime_get_32() - HOLD_TIME_MS >= toggled) {
                        toggled = k_uptime_get_32();

                        // Toggle yellow by toggling red and green
                        if (red_state == green_state) {
                                red_state = !red_state;
                                green_state = !green_state;
                                gpio_pin_toggle_dt(&red_led);
                                gpio_pin_toggle_dt(&green_led);
                        } else {
                                red_state = true;
                                green_state = true;
                                gpio_pin_set_dt(&red_led, 1);
                                gpio_pin_set_dt(&green_led, 1);
                        }
                }

                k_yield();
        }
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
                state = MANUAL;
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
                // When toggling from other colours, red initially on
                if (state == YBLINK) {
                        state = MANUAL;
                        printf("Toggling YELLOW BLINK OFF\n");
                }

                if (green_state) {
                        green_state = false;
                        gpio_pin_set_dt(&green_led, 0);
                        if (!red_state) {
                                gpio_pin_set_dt(&red_led, 1);
                                red_state = true;
                        }
                } else {
                        red_state = !red_state;
                        gpio_pin_toggle_dt(&red_led);
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
                if (state == YBLINK) {
                        state = MANUAL;
                        printf("Toggling YELLOW BLINK OFF\n");
                }

                if (red_state == green_state) {
                        red_state = !red_state;
                        green_state = !green_state;
                        gpio_pin_toggle_dt(&red_led);
                        gpio_pin_toggle_dt(&green_led);
                        printf("Toggling YELLOW\n");
                } else {
                        red_state = true;
                        green_state = true;
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 1);
                        printf("Turning YELLOW ON\n");
                }
        }

}

void green_toggle_isr(void)
{
        // Disable interrupt (re-enabled in main)
        interrupt_disable();
        latest_push = k_uptime_get_32();

        if (paused) {
                if (state == YBLINK) {
                        state = MANUAL;
                        printf("Toggling YELLOW BLINK OFF\n");
                }
                
                if (red_state) {
                        red_state = false;
                        gpio_pin_set_dt(&red_led, 0);
                        if (!green_state) {
                                gpio_pin_set_dt(&green_led, 1);
                                green_state = true;
                        }
                } else {
                        green_state = !green_state;
                        gpio_pin_toggle_dt(&green_led);
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
                if (state != YBLINK) {
                        state = YBLINK;
                        printf("Toggling YELLOW BLINK ON\n");
                } else {
                        state = MANUAL;
                        printf("Toggling YELLOW BLINK OFF\n");
                }
        }
}