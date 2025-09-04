#include <zephyr/kernel.h>

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Red and Green are on board leds but yellow needs to be created by combining red and green.
#define RED DT_ALIAS(led0)
#define GREEN DT_ALIAS(led1)
// Pause button is switch 0
#define PAUSE DT_ALIAS(sw0)
// Prevent noise and constant interruptions from button presses
#define DEBOUNCE_TIME_MS 200

// Bind to leds
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN, gpios);

// Bind to button 0 and create a callback structure
static const struct gpio_dt_spec pause_button = GPIO_DT_SPEC_GET(PAUSE, gpios);
static struct gpio_callback pause_cb_data;

// Interrupt callback for the pause button
void pause_isr(void);

// Function prototypes for color tasks
void red(void *, void *, void *);
void yellow(void *, void *, void *);
void green(void *, void *, void *);

// This simplifies the state machine syntax (instead of using integers)
enum Sate { RED, YELLOW, GREEN, PAUSE } typedef State;

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

// Helper macross and variable to enable an disable interrupts for pause button
// and to detect if the interrupts are enabled or disabled
static bool interrupt_enabled = true;

#define INTERRUPT_ENABLE() {\
        if (!interrupt_enabled && latest_push + DEBOUNCE_TIME_MS < k_uptime_get_32()) \
        {\
                interrupt_enabled = true;\
                gpio_pin_interrupt_configure_dt(&pause_button, GPIO_INT_EDGE_TO_ACTIVE);\
        }\
}

#define INTERRUPT_DISABLE() {\
        interrupt_enabled = false;\
        gpio_pin_interrupt_configure_dt(&pause_button, GPIO_INT_DISABLE);\
}


int main(void)
{
        // Check that on-board leds are ready
        if (!gpio_is_ready_dt(&red_led) || !gpio_is_ready_dt(&green_led)) {
                printf("Error: LED device(s) not ready\n");
                return 0;
        }

        // Check that button is ready
        if (!gpio_is_ready_dt(&pause_button)) {
                printf("Error: Button device not ready\n");
                return;
        }

        // Set led pins as output
	if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&pause_button, GPIO_INPUT | GPIO_PULL_UP) < 0) {
                printf("Error: Failed to configure LED pin(s)\n");
		return 0;
	}

        // Finally configure the button to interrupt
        if (gpio_pin_interrupt_configure_dt(&pause_button, GPIO_INT_EDGE_TO_INACTIVE) < 0) {
                printf("Error: Failed to configure button interrupt\n");
                return 0;
        }

        // add the interrupt handler
        gpio_init_callback(&pause_cb_data, pause_isr, BIT(pause_button.pin));
        if (gpio_add_callback(pause_button.port, &pause_cb_data) < 0) {
                printf("Error: Failed to add button callback\n");
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
                case PAUSE:
                        printf("PAUSED\n");
                        break;
                }

                // Re-enable interrupts if they were disabled
                INTERRUPT_ENABLE();

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
                if (state == RED && !gpio_pin_get_dt(&red_led)) {
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 0);
                }

                k_yield();
        }
}

void yellow(void *, void *, void *)
{
        while (1) {
                if (state == YELLOW) {
                        if (!gpio_pin_get_dt(&red_led)) gpio_pin_set_dt(&red_led, 1);
                        if (!gpio_pin_get_dt(&green_led)) gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void green(void *, void *, void *)
{
        while (1) {
                if (state == GREEN && !gpio_pin_get_dt(&green_led)) {
                        gpio_pin_set_dt(&red_led, 0);
                        gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void pause_isr(void)
{
        // Disable interrupt and toggle state the rest is done by the (enabled in main)
        INTERRUPT_DISABLE();
        paused = !paused;
        latest_push = k_uptime_get_32();

        // If we are pausing, save the current state and set state to PAUSE
        if (paused) {
                cont = state;
                state = PAUSE;
                printf("Pausing\n");
                // If we are unpausing, restore the saved state
        } else {
                state = cont;
                printf("Resuming\n");
        }
}