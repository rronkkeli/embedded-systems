/* Uart task waits for data from the UART port and passes*/

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "ledctl.h"
#include "dispatcher.h"
#include "mux.h"

#define STACK_SIZE 512
#define PRIORITY 5
// Define command sequence size once to deflect possible human errors from not remembering to change every occurance
#define COMSIZ 20
#define UART_DEVICE DT_CHOSEN(zephyr_shell_uart)
const struct device *uart_dev = DEVICE_DT_GET(UART_DEVICE);

K_FIFO_DEFINE(dispatcher_fifo);

K_THREAD_DEFINE(uartth, STACK_SIZE, uart_task, NULL, NULL, NULL, PRIORITY, 0, 0);
K_THREAD_DEFINE(dispatchth, STACK_SIZE, dispatcher_task, NULL, NULL, NULL, PRIORITY-1, 0, 0);

// Prints the information about usage to the UART shell in command line style
void print_help(void) {
    printk("Usage:\n\t[[R | Y | G | O]..INT]..\tSwitch light in given sequence\n\n");
};

struct led_control_t {
    int seq_len;
    char colors[COMSIZ];
    uint16_t hold_times[COMSIZ];
};

struct fifo_data_t {
    void *fifo_reserved;
    struct led_control_t ledctl;
};

bool init_uart(void) {
    if (device_is_ready(uart_dev)) {
        return true;
    }
    printk("UART initialization failed\n");
    return false;
}

// Helper function to push hold time buffer contents to hold time data array
void push_ht(uint16_t *ht_dat, uint16_t *ht, int *offset, int *len, bool *pd) {
    // Update each corresponding hold time of the sequence
    while (*len > 0) {
        ht_dat[*offset] = *ht;
        *offset += 1;; // One command handled -> increment the offset
        *len += 1; // ..and decrement length
    }

    *ht = 0;
    *pd = false;
}

void uart_task(void *, void *, void *) {
    printk("Started uart task\n");
    // Holds the received single character
    char rechar = 0;
    // Holds the count of received characters from UART and also is the index and seqnt counts the commands in sequence
    int cnt, seqnt = 0;
    bool parsing_digits = false;
    // Used to pass the commands and hold times to dispatcher
    char command_buf[COMSIZ];
    uint16_t ht_buf[COMSIZ];
    // Buffer the sequence's hold_time
    uint16_t seq_time_buf = 0;
    // Initialize `command` and `single_command` with zeros
    memset(command_buf, 0, COMSIZ);
    // Default to one second
    memset(ht_buf, 1000, sizeof(ht_buf));

    // Infinitely wait for data to be read from UART
    while (true) {
        // Received a character through UART -> handle it
        if (uart_poll_in(uart_dev, &rechar) == 0) {
            // To add more complexity for me, the message can have letters R, Y, G, O and optional time as an integer for milliseconds.
            // It may contain a sequence of colors and end with the time indicator, in which case the given time is
            // used for every color. If the time setting is given, it is applied to colors preceding it.
            //
            // Example:
            // `RYG500R200G500Y1000O` Changes colors Red, Yellow and Green in sequence, holding each on for 500ms.
            // Then it holds Red for 200ms, Green for 500ms, Yellow for 1s and finally turns leds off.
            //
            // I explicitly wanted to keep the state machine so the automatic mode could be used.
            
            printk("%c", rechar);
            if (!paused) paused = true;
            state = Manual;
            
            // Switch case would've been long long long so used if else instead.
            // Skip this loop cycle if carriage return is received.
            if (rechar == '\r') {
                continue;
            }

            // Parse newline aka command end
            else if (rechar == '\n' || cnt + seqnt == 20) {
                push_ht(ht_buf, &seq_time_buf, &cnt, &seqnt, &parsing_digits);

                printk("Received: %s\n", command_buf);
                // Allocate memory for fifo use
                struct fifo_data_t *data = k_malloc(sizeof(struct fifo_data_t));
                
                if (data == NULL) {
                    printk("Memory allocation error.\n");
                    return;
                }
                
                memcpy(data->ledctl.colors, command_buf, COMSIZ);
                memcpy(&data->ledctl.seq_len, &cnt, 4);
                memcpy(data->ledctl.hold_times, ht_buf, COMSIZ);
                printk("Passing data to dispatcher..\n");
                k_fifo_put(&dispatcher_fifo, data);
                rechar = 0;
                cnt = 0;
                seqnt = 0;
                parsing_digits = false;
                seq_time_buf = 0;
                memset(command_buf, 0, COMSIZ);
                memset(ht_buf, 1000, sizeof(ht_buf));
            }
            
            // Parse digits
            else if (rechar > 47 && rechar < 58) {
                if (!parsing_digits) parsing_digits = true;
                // Convert to binary integer
                seq_time_buf = seq_time_buf * 10 + (rechar - 48);
            }
            
            // Parse other characters
            else {
                // Commands
                if (rechar == 'R' || rechar == 'Y' || rechar == 'G' || rechar == 'O') {
                    // Write previous sequence time info if present
                    if (parsing_digits) {
                        push_ht(ht_buf, &seq_time_buf, &cnt, &seqnt, &parsing_digits);
                    }

                    seqnt++; // Increment sequence counter
                    command_buf[cnt + seqnt] = rechar;
                }
                
                // If no comprehensible command was given or the command was erroneous, print help
                else {
                    print_help();
                    char discard;
                    while (uart_poll_in(uart_dev, &discard) != -1);
                    rechar = 0;
                    cnt = 0;
                    seqnt = 0;
                    parsing_digits = false;
                    seq_time_buf = 0;
                    memset(command_buf, 0, COMSIZ);
                    memset(ht_buf, 1000, sizeof(ht_buf));
                }
            }
        }

        k_msleep(50);
    }
}
                
void dispatcher_task(void *, void *, void *) {
    while (true) {
        printk("Waiting for fifo data\n");
        struct fifo_data_t *rec_data = k_fifo_get(&dispatcher_fifo, K_FOREVER);
        printk("Working on data with length: %d\n", rec_data->ledctl.seq_len);

        // Iterate over each command (end with ecountering 0)
        for (int i = 0; i < rec_data->ledctl.seq_len; i++) {
            struct holdtime_t *ht = k_malloc(sizeof(struct holdtime_t));
            memcpy(&ht->time, &rec_data->ledctl.hold_times[i], 4);
            k_fifo_put(&ht_fifo, ht);
            struct k_condvar *release = NULL;
            struct k_mutex *lock = NULL;

            switch (rec_data->ledctl.colors[i]) {
                case 'R':
                    release = &rsig;
                    lock = &rmux;
                    printk("Switched led to Red\n");
                    break;
                case 'Y':
                    release = &ysig;
                    lock = &ymux;
                    printk("Switched led to Yellow\n");
                    break;
                case 'G':
                    release = &gsig;
                    lock = &gmux;
                    printk("Switched led to Green\n");
                    break;
                default:
                    set_off();
                    printk("Switched led to Off\n");
                    k_msleep(ht->time);
                    k_free(ht);
                    break;
            }

            if (release != NULL) k_condvar_wait(release, lock, K_FOREVER);
        }

        k_free(rec_data);
        k_yield();
    }
}