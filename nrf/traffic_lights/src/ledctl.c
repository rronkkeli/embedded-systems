#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/timing/timing.h>

#include "ledctl.h"
#include "mux.h"
#include "debug.h"

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
/*
    Add thread execution time on seq_time variable. If ltime_set becomes 7 during this operation, seq_time is scheduled in
    debugging.
*/
void add_or_send_exec(timing_t, int);

/*
    Each led task can set a bit in this atomic variable to signal othre led tasks that it has counted its execution time.

    Other ready tasks won't update the seq_time value until last one to update the value clears the flags.

    Value 7 means that all tasks have added their own values.
*/
atomic_t ltime_set = 0;
atomic_t seq_time = 0;


bool init_leds(void)
{
    // Check that on-board leds are ready
    if (!gpio_is_ready_dt(&red_led) || !gpio_is_ready_dt(&green_led)) {
        debug("Error: LED device(s) not ready");
        return false;
    }

    if (gpio_pin_configure_dt(&red_led, GPIO_OUTPUT_ACTIVE) < 0 ||
        gpio_pin_configure_dt(&green_led, GPIO_OUTPUT_ACTIVE) < 0) {
        debug("Error: Failed to configure LED pins");
        return false;
    }

    return true;
}

void red(enum State *state, enum Color *color, void *)
{
    timing_t start;
    k_sem_give(&threads_ready);

    // Loop forever
    while (1) {
        start = timing_counter_get();
        
        // Wait for a signal to switch led on and off
        debug("Waiting for lmux mutex..");
        k_mutex_lock(&lmux, K_FOREVER);

        debug("Done! Waiting for condition variable rsig..");

        if (k_condvar_wait(&rsig, &lmux, K_FOREVER) == 0) {
            debug("Done!");

            toggle_led(*state, color, Red);
        }

        add_or_send_exec(start, 0);

        k_mutex_unlock(&lmux);
        k_yield();
    }
}

void yellow(enum State *state, enum Color *color, void *)
{
    timing_t start;
    k_sem_give(&threads_ready);    
    // Loop forever
    while (1) {
        start = timing_counter_get();

        // Wait for a signal to switch led on and off
        debug("Waiting for lmux mutex..");
        k_mutex_lock(&lmux, K_FOREVER);

        debug("Done! Waiting for condition variable ysig..");

        if (k_condvar_wait(&ysig, &lmux, K_FOREVER) == 0) {
            debug("Done!");

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

        add_or_send_exec(start, 1);
        k_mutex_unlock(&lmux);
        k_yield();
    }
}

void green(enum State *state, enum Color *color, void *)
{
    timing_t start;
    k_sem_give(&threads_ready);
    // Loop forever
    while (1) {
        start = timing_counter_get();
        
        // Wait for a signal to switch led on and off
        debug("Waiting for lmux mutex..");
        k_mutex_lock(&lmux, K_FOREVER);
        
        debug("Done! Waiting for condition variable gsig..");
        
        if (k_condvar_wait(&gsig, &lmux, K_FOREVER) == 0) {
            debug("Done!");

            toggle_led(*state, color, Green);
        }

        add_or_send_exec(start, 2);
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
            debug("Thread was killed");
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

void add_or_send_exec(timing_t time, int bit) {
    if (!atomic_test_and_set_bit(&ltime_set, bit)) {
        atomic_val_t stop = (atomic_val_t)timing_cycles_to_ns(timing_counter_get() - time);
        atomic_add(&seq_time, stop);
        debug("Thread time: %d ns", stop);

        if (atomic_cas(&ltime_set, 7, 0)) {
            debug("Sequence execution time: %d ns", atomic_clear(&seq_time));
        }
    }
}
