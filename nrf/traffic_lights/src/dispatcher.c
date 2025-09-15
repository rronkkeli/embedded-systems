/* Uart task waits for data from the UART port and passes*/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "ledctl.h"
#include "dispatcher.h"

#define STACK_SIZE 512
#define PRIORITY 5
// Define command sequence size once to deflect possible human errors from not remembering to change every occurance
#define COMSIZ 20
#define UART_DEVICE DT_CHOSEN(zephyr_shell_uart)
const struct device *uart_dev = DEVICE_DT_GET(UART_DEVICE);

K_FIFO_DEFINE(dispatcher_fifo);

K_THREAD_DEFINE(uartth, STACK_SIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dispatchth, STACK_SIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY-1, 0, 0);

struct fifo_data {
    void *fifo_reserved;
    char command[COMSIZ];
    int len;
};

bool init_uart(void) {
    if (device_is_ready(uart_dev)) {
        return true;
    }
    printk("UART initialization failed\n");
    return false;
}

void uart_task(void *, void *, void *) {
    printk("Started uart task\n");
    // Holds the received single character
    char rechar = 0;
    // Holds the count of received characters from UART and also is the index
    int mix = 0;
    // Used to pass the commands to dispatcher
    char command[COMSIZ];
    // Initialize `command` and `single_command` with zeros
    memset(command, 0, COMSIZ);

    enum State prev_state = state;
    enum Color prev_color = color;

    // Infinitely wait for data to be read from UART
    while (true) {
        // printk("Anna mulle jottaiii ;) ");
        // Received a character through UART -> handle it
        if (uart_poll_in(uart_dev, &rechar) == 0) {
            // The message can have letters R, Y, G, O and optional time as an integer for milliseconds.
            // It may contain a sequence of colors and end with the time indicator, in which case the given time is
            // used for every color. If the time setting is given, it is applied to colors preceding it.
            //
            // Example:
            // `RYG500R200G500Y1000O` Changes colors Red, Yellow and Green in sequence, holding each on for 500ms.
            // Then it holds Red for 200ms, Green for 500ms, Yellow for 1s and finally turns leds off.

            printk("%c", rechar);
            state = Manual;

            // End reading and empty UART buffer if newline is received. Signal the dispatcher it can process the command.
            if (rechar == '\n' || rechar == '\r') {
                printk("Received: %s\n", command);
                // Allocate memory for fifo use
                struct fifo_data *data = k_malloc(sizeof(struct fifo_data));

                if (data == NULL) {
                    printk("Memory allocation error.\n");
                    return;
                }

                memcpy(data->command, command, COMSIZ);
                memcpy(&data->len, &mix, 4);
                printk("Passing data to dispatcher..\n");
                k_fifo_put(&dispatcher_fifo, data);
                rechar = 0;
                mix = 0;
                memset(command, 0, COMSIZ);
            }
            // Otherwise keep reading
            else {
                if (mix < COMSIZ-1) {
                    command[mix] = rechar;
                    mix++;
                } else {
                    printk("Received: %s\n", command);
                    // Dispatch sequence and clear message buffer, printing an error to UART console
                    printk("Command is over 20 bytes! Give shorter command to get what you expect. Sequence truncated to 20 bytes.\n");
                    // Allocate memory for fifo use
                    struct fifo_data *data = k_malloc(sizeof(struct fifo_data));

                    if (data == NULL) {
                        printk("Memory allocation failed.\n");
                        return;
                    }

                    memcpy(data->command, command, COMSIZ);
                    memcpy(&data->len, &mix, 4);
                    printk("Passing data to dispatcher..\n");
                    k_fifo_put(&dispatcher_fifo, data);
                    rechar = 0;
                    mix = 0;
                    memset(command, 0, COMSIZ);
                }
            }
        }

        k_yield();
    }
}

void dispatcher_task(void *, void *, void *) {
    // Used when updated from required step 1
    //uint32_t hold_times[COMSIZ] = 0;

    while (true) {
        printk("Waiting for fifo data\n");
        struct fifo_data *rec_data = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        printk("Working on data: %s with length: %d\n", rec_data->command, rec_data->len);

        char commands[COMSIZ];
        memcpy(commands, rec_data->command, COMSIZ);

        // Iterate over each command (end with ecountering 0)
        for (int i = 0; i < rec_data->len; i++) {
            switch (commands[i]) {
                case 'R':
                    set_red();
                    printk("Switched led to Red\n");
                    break;
                case 'Y':
                    set_yellow();
                    printk("Switched led to Yellow\n");
                    break;
                case 'G':
                    set_green();
                    printk("Switched led to Green\n");
                    break;
                default:
                    set_off();
                    printk("Switched led to Off\n");
                    break;
            }

            // Hold the led on for at least a predefined hold time
            k_msleep(500);
        }

        k_free(rec_data);
        k_yield();
    }
}