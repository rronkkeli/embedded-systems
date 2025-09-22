#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "buttons.h"
#include "ledctl.h"
#include "mux.h"

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

K_TIMER_DEFINE(button_db, interrupt_enable, NULL);

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

static bool interrupt_enabled = true;

bool init_buttons(void)
{
    // Check that buttons are ready
    if (!gpio_is_ready_dt(&manual_button) || !gpio_is_ready_dt(&red_toggle) ||
    !gpio_is_ready_dt(&yellow_toggle) || !gpio_is_ready_dt(&green_toggle) ||
    !gpio_is_ready_dt(&yblink_toggle)) {
        printf("Error: Button device not ready\n");
        return false;
    }

    // Set led pins as output and buttons as input
	if (gpio_pin_configure_dt(&manual_button, GPIO_INPUT | GPIO_PULL_UP) < 0
        || gpio_pin_configure_dt(&red_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
        || gpio_pin_configure_dt(&yellow_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
        || gpio_pin_configure_dt(&green_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0
        || gpio_pin_configure_dt(&yblink_toggle, GPIO_INPUT | GPIO_PULL_UP) < 0) {
        printf("Error: Failed to configure IO\n");
		return false;
	}

    // Finally configure the button to interrupt and..
    if (gpio_pin_interrupt_configure_dt(&manual_button, GPIO_INT_EDGE_TO_INACTIVE) < 0 ||
    gpio_pin_interrupt_configure_dt(&red_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
    gpio_pin_interrupt_configure_dt(&yellow_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
    gpio_pin_interrupt_configure_dt(&green_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0 ||
    gpio_pin_interrupt_configure_dt(&yblink_toggle, GPIO_INT_EDGE_TO_ACTIVE) < 0) {
        printf("Error: Failed to configure button interrupt\n");
        return false;
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
        return false;
    }

    return true;
}

void interrupt_enable(void)
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

    printk("Interrupts enabled\n");
}

void interrupt_disable(void)
{
    interrupt_enabled = false;
    gpio_pin_interrupt_configure_dt(&manual_button, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&red_toggle, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&yellow_toggle, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&green_toggle, GPIO_INT_DISABLE);
    gpio_pin_interrupt_configure_dt(&yblink_toggle, GPIO_INT_DISABLE);
    k_timer_start(&button_db, K_MSEC(DEBOUNCE_TIME_MS), K_NO_WAIT);
}

void manual_isr(void)
{
    interrupt_disable();
    paused = !paused;

    // If we are pausing, save the current state and set state to MANUAL
    if (paused) {
        // Save the current color
        cont = color;
        state = Manual;
        printk("Manual control\n");
    // If we are unpausing, restore the saved state
    } else {
        if (k_mutex_lock(&lmux, K_NO_WAIT) == 0) {
            state = Auto;
            color = cont;

            switch (color) {
                case Red:
                    k_condvar_signal(&rsig);
                    break;
                case Yellow:
                    k_condvar_signal(&ysig);
                    break;
                default:
                    k_condvar_signal(&gsig);
                    break;
            }
        } else {
            printk("Try again. Mutex is busy\n");
        }

        k_mutex_unlock(&lmux);
    }
}

void red_toggle_isr(void)
{
    interrupt_disable();

    // Only do something if we are paused
    if (paused) {
        if (state == Blink) {
            state = Manual;
            printf("Toggling YELLOW BLINK OFF\n");
        }

        // Send signal to red task
        k_condvar_signal(&rsig);
        printf("Toggling RED\n");
    }
}

void yellow_toggle_isr(void)
{
    interrupt_disable();

    if (paused) {
        if (state == Blink) {
            state = Manual;
            printf("Toggling YELLOW BLINK OFF\n");
        }

        // Send signal to yellow task
        k_condvar_signal(&ysig);
        printf("Toggling YELLOW\n");
    }

}

void green_toggle_isr(void)
{
    interrupt_disable();

    if (paused) {
        if (state == Blink) {
            state = Manual;
            printf("Toggling YELLOW BLINK OFF\n");
        }
        
        // Send signal to green task
        k_condvar_signal(&gsig);
        printf("Toggling GREEN\n");
    }
}

void yblink_toggle_isr(void)
{
    interrupt_disable();

    if (paused) {
        // Toggle blinking yellow mode
        if (state != Blink) {
            state = Blink;
            printf("Toggling YELLOW BLINK ON\n");
        } else {
            state = Manual;
            printf("Toggling YELLOW BLINK OFF\n");
        }
    }

    // Send signal to yellow task
    k_condvar_signal(&ysig);
}