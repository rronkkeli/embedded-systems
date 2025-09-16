#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/random/random.h>

#include "ledctl.h"
#include "mux.h"

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

// Stack size and priority for task threads
#define STACKSIZE 512
#define PRIORITY 5

// TODO: Convert these into a one single thread that switches leds depending on what user wants.
// No need to repeat ourselves.
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
    int random;
    // Loop forever
    while (1) {
    // Check if we need to turn on red and green off
    if (*state == Red && *led_color != LRed) {
        *led_color = LRed;
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 0);

    } else if (*state == Manual) {
        // Check if signal has been given but do not block here in case automatic mode is put on
        if (k_condvar_wait(&rsig, &rmux, K_NO_WAIT) == 0) {
        struct holdtime_t *holdtime = k_fifo_get(&ht_fifo, K_NO_WAIT);
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 0);

        // Hold the given time if specified or default
        if (holdtime != NULL) {
            k_msleep(holdtime->time);
            k_free(holdtime);
        } else {
            k_msleep(HOLD_TIME_MS);
        }

        gpio_pin_set_dt(&red_led, 0);
        }
    }

    // Get random sleep value between 31 and 255 inclusive. This is to prevent task locks.
    random = (int)sys_rand32_get();
    random = (random & 0x000000ff) | 0x0000001f;
    k_msleep(random);
    }
}

void yellow(enum State *state, enum Color *led_color, bool *paused)
{
    int random;
    while (1) {
    if (*state == Yellow && *led_color != LYellow) {
        *led_color = LYellow;
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);

    } else if (*state == Manual) {
        // Check if signal has been given but do not block here in case automatic mode is put on
        if (k_condvar_wait(&ysig, &ymux, K_NO_WAIT) == 0) {
        struct holdtime_t *holdtime = k_fifo_get(&ht_fifo, K_NO_WAIT);
        gpio_pin_set_dt(&red_led, 1);
        gpio_pin_set_dt(&green_led, 1);

        // Hold the given time if specified or default
        if (holdtime != NULL) {
            k_msleep(holdtime->time);
            k_free(holdtime);
        } else {
            k_msleep(HOLD_TIME_MS);
        }

        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 0);
        }
    }

    random = (int)sys_rand32_get();
    random = (random & 0x000000ff) | 0x0000001f;
    k_msleep(random);
    }
}

void green(enum State *state, enum Color *led_color, bool *paused)
{
    int random;
    while (1) {
    if (*state == Green && *led_color != LGreen) {
        *led_color = LGreen;
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 1);

    } else if (*state == Manual) {
        // Check if signal has been given but do not block here in case automatic mode is put on
        if (k_condvar_wait(&gsig, &gmux, K_NO_WAIT) == 0) {
        struct holdtime_t *holdtime = k_fifo_get(&ht_fifo, K_NO_WAIT);
        gpio_pin_set_dt(&red_led, 0);
        gpio_pin_set_dt(&green_led, 1);

        // Hold the given time if specified or default
        if (holdtime != NULL) {
            k_msleep(holdtime->time);
            k_free(holdtime);
        } else {
            k_msleep(HOLD_TIME_MS);
        }

        gpio_pin_set_dt(&green_led, 0);
        }
    }

    // Get random sleep value between 31 and 255 inclusive. This is to prevent blocking other threads.
    random = (int)sys_rand32_get();
    random = (random & 0x000000ff) | 0x0000001f;
    k_msleep(random);
    }
}

void yblink(enum State *state, enum Color *led_color, bool *paused)
{
    uint32_t toggled = 0;
    int random;
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

        random = (int)sys_rand32_get();
        random = (random & 0x000000ff) | 0x0000001f;
        k_msleep(random);
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
