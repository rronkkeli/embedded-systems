#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/random/random.h>

#include "ledctl.h"
#include "mux.h"

// Set transition time between colors
const uint32_t HOLD_TIME_MS = 1000;
volatile bool paused = false;

// Red and Green are on board leds but yellow needs to be created by combining red and green.
#define RED_L DT_ALIAS(led0)
#define GREEN_L DT_ALIAS(led1)

// Bind to leds and tract their state
static const struct gpio_dt_spec red_led = GPIO_DT_SPEC_GET(RED_L, gpios);
static const struct gpio_dt_spec green_led = GPIO_DT_SPEC_GET(GREEN_L, gpios);

// Current state variables initialized to red
volatile enum State state = Auto;
volatile enum Color cont = Red;
volatile enum Color color = Red;

// Stack size and priority for task threads
#define STACKSIZE 512
#define PRIORITY 2

// TODO: Led tasks should wait for a signal `xsig` locked with led mutex `lmux` and release the lock by giving signal
// to next color or to dispatcher, depending on if mode is automatic or manual. On automatic mode, each task calls the next:
// Red -> Yellow -> Green -> Red.. and on manual mode each task sends a release signal for dispatcher.
// Define tasks for each color
K_THREAD_DEFINE(redth, STACKSIZE,
		red, &state, &color, NULL,
		PRIORITY, 0, 0);

K_THREAD_DEFINE(yellowth, STACKSIZE,
        yellow, &state, &color, NULL,
        PRIORITY, 0, 0);

K_THREAD_DEFINE(greenth, STACKSIZE,
        green, &state, &color, NULL,
        PRIORITY, 0, 0);
        
// Helper functions for tasks
void toggle_led(enum State, enum Color *, enum Color);
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

void red(enum State *state, enum Color *color, void *)
{
    k_sem_give(&threads_ready);
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        k_mutex_lock(&lmux, K_FOREVER);
        if (k_condvar_wait(&rsig, &lmux, K_FOREVER) == 0) {
            toggle_led(*state, color, Red);
        }
        k_mutex_unlock(&lmux);
        k_yield();
    }
}

void yellow(enum State *state, enum Color *color, void *)
{
    k_sem_give(&threads_ready);    
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        k_mutex_lock(&lmux, K_FOREVER);
        if (k_condvar_wait(&ysig, &lmux, K_FOREVER) == 0) {

            if (*state == Blink) {
                while (*state == Blink) {
                    set_yellow();
                    k_msleep(HOLD_TIME_MS);
                    set_off();
                    k_msleep(HOLD_TIME_MS);
                }
            } else {
                toggle_led(*state, color, Yellow);
            }
        }
        k_mutex_unlock(&lmux);
        k_yield();
    }
}

void green(enum State *state, enum Color *color, void *)
{
    k_sem_give(&threads_ready);
    // Loop forever
    while (1) {
        // Wait for a signal to switch led on and off
        k_mutex_lock(&lmux, K_FOREVER);
        if (k_condvar_wait(&gsig, &lmux, K_FOREVER) == 0) {
            toggle_led(*state, color, Green);
        }
        k_mutex_unlock(&lmux);
        k_yield();
    }
}

void toggle_led(enum State state, enum Color *from_color, enum Color to_color) {
    // Store the color function to be called and the next led signal if not manual mode
    void (*set_color)(void);
    struct k_condvar *next_led_signal = NULL;

    switch (to_color) {
        case Red:
            set_color = &set_red;
            next_led_signal = &ysig;
            break;
        case Yellow:
            set_color = &set_yellow;
            next_led_signal = &gsig;
            break;
        case Green:
            set_color = &set_green;
            next_led_signal = &rsig;
            break;
        default:
            set_color = &set_off;
            printk("Thread was killed\n");
            k_oops();
    }

    if (state != Auto) {
        // Toggle off if already on
        if (*from_color == to_color) {
            set_off();
            *from_color = Off;
        } else {
            set_color();
            *from_color = to_color;
        }

        
        // Blink state is handled in the manual_isr in buttons.c
    } else {
        set_color();
        k_msleep(HOLD_TIME_MS);
        k_condvar_signal(next_led_signal);
    }

    k_condvar_signal(&sig_ok);
}

void set_red(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 0);
    color = Red;
}

void set_yellow(void)
{
    gpio_pin_set_dt(&red_led, 1);
    gpio_pin_set_dt(&green_led, 1);
    color = Yellow;
}

void set_green(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 1);
    color = Green;
}

void set_off(void)
{
    gpio_pin_set_dt(&red_led, 0);
    gpio_pin_set_dt(&green_led, 0);
    color = Off;
}
