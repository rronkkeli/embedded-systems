#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "ledctl.h"

// Set transition time between colors
const uint32_t HOLD_TIME_MS = 1000;
bool paused = false;

// Red and Green are on board leds but yellow needs to be created by combining red and green.
#define RED_L DT_ALIAS(led0)
#define GREEN_L DT_ALIAS(led1)

// Bind to leds and tract their state
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_L, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_L, gpios);

// Current state variables initialized to red
enum State state = Red;
enum State cont = Red;
enum Color color = LRed;

bool init_leds(void)
{
        // Check that on-board leds are ready
        if (!gpio_is_ready_dt(&red_led) || !gpio_is_ready_dt(&green_led)) {
                printf("Error: LED device(s) not ready\n");
                return false;
        }

        if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ||
            gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) < 0) {
                printf("Error: Failed to configure LED pins\n");
                return false;
        }

        return true;
}

void red(enum State *state, enum Color *led_color, bool *paused)
{
        // Loop forever
        while (1) {
                // Check if we need to turn on red and green off
                if (*state == Red && *led_color != LRed) {
                        *led_color = LRed;
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 0);
                }

                k_yield();
        }
}

void yellow(enum State *state, enum Color *led_color, bool *paused)
{
        while (1) {
                if (*state == Yellow && *led_color != LYellow) {
                        *led_color = LYellow;
                        gpio_pin_set_dt(&red_led, 1);
                        gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void green(enum State *state, enum Color *led_color, bool *paused)
{
        while (1) {
                if (*state == Green && *led_color != LGreen) {
                        *led_color = LGreen;
                        gpio_pin_set_dt(&red_led, 0);
                        gpio_pin_set_dt(&green_led, 1);
                }

                k_yield();
        }
}

void yblink(enum State *state, enum Color *led_color, bool *paused)
{
        uint32_t toggled = 0;

        while (1) {
                if (*state == Yblink && *paused && k_uptime_get_32() - HOLD_TIME_MS >= toggled) {
                        toggled = k_uptime_get_32();

                        // Toggle yellow by toggling red and green
                        if (*led_color == LYellow) {
                                *led_color = LOff;
                                gpio_pin_toggle_dt(&red_led);
                                gpio_pin_toggle_dt(&green_led);
                        } else {
                                *led_color = LYellow;
                                gpio_pin_set_dt(&red_led, 1);
                                gpio_pin_set_dt(&green_led, 1);
                        }
                }

                k_yield();
        }
}

void set_red(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 0);
}

void set_yellow(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 1);
}

void set_green(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 1);
}

void set_off(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
}
