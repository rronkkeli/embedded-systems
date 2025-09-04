#include <zephyr/kernel.h>

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// Red and Green are on board leds but yellow needs to be created by combining red and green.
#define RED DT_ALIAS(led0)
#define GREEN DT_ALIAS(led1)

// Bind to leds
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN, gpios);

void red(void *, void *, void *);
void yellow(void *, void *, void *);
void green(void *, void *, void *);

// This simplifies the state machine syntax (instead of using integers)
enum Sate { RED, YELLOW, GREEN } typedef State;

// Current state variable initialized to red
volatile static State state = RED;

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

int main(void)
{
        // Check that on-board leds are ready
        if (!gpio_is_ready_dt(&red_led) || !gpio_is_ready_dt(&green_led)) {
                printf("Error: LED device(s) not ready\n");
                return 0;
        }

        // Set led pins as output
	if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) < 0) {
                printf("Error: Failed to configure LED pin(s)\n");
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
                }

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