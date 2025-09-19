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

volatile bool red_ignore = false;
volatile bool yellow_ignore = false;
volatile bool green_ignore = false;

// Stack size and priority for task threads
#define STACKSIZE 512
#define PRIORITY 5

// TODO: Led tasks should wait for a signal `xsig` locked with led mutex `lmux` and release the lock by giving signal
// to next color or to dispatcher, depending on if mode is automatic or manual. On automatic mode, each task calls the next:
// Red -> Yellow -> Green -> Red.. and on manual mode each task sends a release signal for dispatcher.
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

// Helper functions for tasks
void hold(enum State, enum Color);
void set_red(void);
void set_yellow(void);
void set_green(void);
void set_off(void);

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

void red(enum State *state, void  *, void *)
{
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        if (k_condvar_wait(&rsig, &lmux, K_FOREVER) == 0) {
            hold(*state, LRed);
        }
    }
}

void yellow(enum State *state, void  *, void *)
{
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        if (k_condvar_wait(&ysig, &lmux, K_FOREVER) == 0) {
            hold(*state, LYellow);
        }
    }
}

void green(enum State *state, void  *, void *)
{
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        if (k_condvar_wait(&gsig, &lmux, K_FOREVER) == 0) {
            hold(*state, LGreen);
        }
    }
}

void hold(enum State state, enum Color color) {
    // Store the color function to be called and the next led signal if not manual mode
    void (*set_color)(void);
    struct k_condvar *next_led_signal;

    switch (color) {
        case LRed:
            set_color = set_red;
            next_led_signal = &rsig;
            break;
            case LYellow:
            set_color = set_yellow;
            next_led_signal = &ysig;
            break;
            // Default to green
            default:
            set_color = set_green;
            next_led_signal = &ysig;
            break;
    }

    if (state == Manual) {
        struct holdtime_t *ht = k_fifo_get(&ht_fifo, K_MSEC(HOLD_TIME_MS));

        if  (ht != NULL) {
            if (ht->time > 0x7fff) {
                set_off();
                k_msleep(ht->time & 0x7fff);
            } else {
                set_color();
                k_msleep(ht->time);
            }
        } else {
            set_color();
            printk("Did not receive fifo data in timeout time\n");
        }
        
        k_free(ht);
        k_condvar_signal(&sig_ok);
        
    } else {
        set_color();
        k_msleep(HOLD_TIME_MS);
        k_condvar_signal(next_led_signal);
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
