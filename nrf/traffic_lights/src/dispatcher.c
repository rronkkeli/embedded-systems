/* Uart task waits for data from the UART port and passes*/

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/timing/timing.h>

#include "ledctl.h"
#include "dispatcher.h"
#include "mux.h"
#include "debug.h"
#include "timeparser.h"

#define STACK_SIZE 512
// Define command sequence size once to deflect possible human errors from not remembering to change every occurance
#define COMSIZ 20
#define UART_DEVICE DT_CHOSEN(zephyr_shell_uart)
const struct device *uart_dev = DEVICE_DT_GET(UART_DEVICE);

K_FIFO_DEFINE(dispatcher_fifo);

K_THREAD_DEFINE(uartth, STACK_SIZE, uart_task, NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(dispatchth, STACK_SIZE, dispatcher_task, &color, NULL, NULL, 3, 0, 0);

volatile bool robomode = false;

// Prints the information about usage to the UART shell in command line style
void print_help(void) {
    printk("\n\nUsage:\n\t[[R | Y | G | O]..INT]..[[T]INT]\tSwitch light in given sequence and loop T times\n\n\tUse D to toggle debug on or off (does not echo)\n\n");
};

struct led_control_t {
    int seq_len;
    char colors[COMSIZ];
    uint16_t hold_times[COMSIZ];
    uint16_t loop;
};

struct fifo_data_t {
    void *fifo_reserved;
    struct led_control_t ledctl;
};

bool init_uart(void) {
    if (device_is_ready(uart_dev)) {
        return true;
    }
    debug("UART initialization failed");
    return false;
}

// Helper function to push hold time buffer contents to hold time data array
void push_ht(uint16_t *ht_dat, uint16_t *ht, int *offset, int *len, bool *pd) {
    // Update each corresponding hold time of the sequence
    uint16_t holdfor = *ht;


    while (*len > 0) {
        int ix = *offset;
        ht_dat[ix] = holdfor;
        *offset += 1; // One command handled -> increment the offset
        *len -= 1; // ..and decrement length
    }

    *ht = 1000;
    *pd = false;
}

// Switch to blink state
void simple_task() {
    printk("Timer expired\n");

    state = Blink;
    k_condvar_signal(&ysig);
}

K_TIMER_DEFINE(schedule_timer, simple_task, NULL);

void uart_task(void *, void *, void *) {
    debug("Started uart task");
    // Holds the received single character
    char rechar = 0;
    // Holds the count of received characters from UART and also is the index and seqnt counts the commands in sequence
    int cnt = 0;
    int seqnt = 0;
    bool parsing_digits = false;
    bool uart_print = true;
    // Loop through the given sequence n amount of times
    uint16_t loop = 1;
    bool expect_loop = false;
    // Used to pass the commands and hold times to dispatcher
    char command_buf[COMSIZ];
    uint16_t ht_buf[COMSIZ];
    // Buffer the sequence's hold_time
    // Default to one second
    uint16_t seq_time_buf = 1000;
    // Initialize `command` and `single_command` with zeros
    memset(command_buf, 0, COMSIZ);
    memset(ht_buf, (int)&seq_time_buf, sizeof(ht_buf));

    // Infinitely wait for data to be read from UART
    // ----- Commented out to test time parser uart
    // while (true) {
    //     if (uart_print) {
    //         uart_print = false;
    //         debug("Waiting for UART");
    //     }
    //     // Received a character through UART -> handle it
    //     if (uart_poll_in(uart_dev, &rechar) == 0) {
    //         // To add more complexity for me, the message can have letters R, Y, G, O and optional time as an integer for milliseconds.
    //         // It may contain a sequence of colors and end with the time indicator, in which case the given time is
    //         // used for every color. If the time setting is given, it is applied to colors preceding it.
    //         //
    //         // Example:
    //         // `RYG500R200G500Y1000O` Changes colors Red, Yellow and Green in sequence, holding each on for 500ms.
    //         // Then it holds Red for 200ms, Green for 500ms, Yellow for 1s and finally turns leds off.

    //         // Toggle debug messages on and off but do not print anything.
    //         if (rechar == 'D') {
    //             print_debug_messages = !print_debug_messages;
    //             continue;
    //         }
            
    //         printk("%c", rechar);
    //         if (!paused && state != Manual) {
    //             paused = true;
    //             state = Manual;
    //             cont = color;
    //         }

    //         if (rechar == '\r') {
    //             rechar = '\n';
    //         }
            
    //         // Parse newline aka command end
    //         if (rechar == '\n') {
    //             printk("\n");
    //             push_ht(ht_buf, &seq_time_buf, &cnt, &seqnt, &parsing_digits);
                
    //             debug("Received: %s", command_buf);
    //             // Allocate memory for fifo use
    //             struct fifo_data_t *data = k_malloc(sizeof(struct fifo_data_t));
                
    //             if (data == NULL) {
    //                 debug("Memory allocation error.");
    //                 return;
    //             }
                
    //             memcpy(data->ledctl.colors, command_buf, COMSIZ);
    //             memcpy(data->ledctl.hold_times, ht_buf, sizeof(ht_buf));
    //             data->ledctl.seq_len = cnt;
    //             data->ledctl.loop = loop;
    //             k_fifo_put(&dispatcher_fifo, data);
    //             rechar = 0;
    //             cnt = 0;
    //             seqnt = 0;
    //             memset(command_buf, 0, COMSIZ);
    //             memset(ht_buf, 1000, sizeof(ht_buf));
    //             uart_print = true;
    //             expect_loop = false;
    //             loop = 1;
    //         }
            
    //         // Parse digits
    //         else if (rechar > 47 && rechar < 58) {
    //             if (!parsing_digits) {
    //                 parsing_digits = true;
    //                 seq_time_buf = 0;
    //             }

    //             // Convert to binary integer
    //             if (expect_loop) {
    //                 loop = loop * 10 + (rechar - 48);
    //             } else {
    //                 seq_time_buf = seq_time_buf * 10 + (rechar - 48);
    //             }
    //         }
            
    //         // Parse other characters
    //         else {
    //             // Commands
    //             if ((rechar == 'R' || rechar == 'Y' || rechar == 'G' || rechar == 'O') && !expect_loop) {
    //                 // Write previous sequence time info if present
    //                 if (parsing_digits) {
    //                     push_ht(ht_buf, &seq_time_buf, &cnt, &seqnt, &parsing_digits);
    //                 }
                    
    //                 int index = cnt + seqnt;
    //                 command_buf[index] = rechar;
    //                 seqnt++; // Increment sequence counter
    //                 expect_loop = false;
    //             }

    //             // Parse loop command
    //             else if (rechar == 'T') {
    //                 if (parsing_digits) {
    //                     push_ht(ht_buf, &seq_time_buf, &cnt, &seqnt, &parsing_digits);
    //                 }

    //                 if (expect_loop) {
    //                     debug("\nYou cannot define T multiple times!");
    //                 } else {
    //                     expect_loop = true;
    //                     loop = 0;
    //                 }
    //             }
                
    //             // If no comprehensible command was given or the command was erroneous, print help
    //             else if (rechar != '\r') {
    //                 print_help();
    //                 char discard;
    //                 while (uart_poll_in(uart_dev, &discard) != -1);
    //                 rechar = 0;
    //                 cnt = 0;
    //                 seqnt = 0;
    //                 parsing_digits = false;
    //                 expect_loop = false;
    //                 loop = 1;
    //                 seq_time_buf = 0;
    //                 memset(command_buf, 0, COMSIZ);
    //                 memset(ht_buf, 1000, sizeof(ht_buf));
    //             }
    //         }
    //     }

    //     k_msleep(100);
    // }
    // ----- End comment out

    // This is meant to be a simple implementation to show it has been implemented. I intend to make this implementation better later.
    while (true) {
        if (uart_print) {
            uart_print = false;
            debug("Waiting for UART");
        }
        // Received a character through UART -> handle it
        if (uart_poll_in(uart_dev, &rechar) == 0) {            
            
            if (rechar == (char)0) {
                robomode = !robomode;
                printk("%i\n", robomode);
                continue;
            } else {
                // Do not echo characters when on robo mode
                if (!robomode) printk("%c", rechar);
                
                if (!paused && state != Manual) {
                    paused = true;
                    state = Manual;
                    cont = color;
                }
    
                if (rechar == '\r') {
                    rechar = '\n';
                }
                
                // Parse newline aka command end
                if (rechar == '\n') {                        
                    int timeout = time_parse(command_buf);
                    
                    // Print data to robot
                    if (robomode) {
                        printk("%i\n", timeout);
                    }
                    
                    // Otherwise schedule task normally
                    else {
                        printk("\n");
                        if (timeout > -1) {
                            printk("Setting up timer with %i seconds\n", timeout);
                            k_timer_start(&schedule_timer, K_SECONDS(timeout), K_NO_WAIT);
                        } else {
                            printk("Invalid input data. Got error %08x\n", timeout);
                        }
                    }
                    
                    cnt = 0;
                    memset(command_buf, 0, COMSIZ);
    
                } else {
                    command_buf[cnt] = rechar;
                    cnt++;
                }
            }

        }
        // Causes problems with robot so uncommented it
        // k_msleep(100);
    }
}
                
void dispatcher_task(enum Color *curcol, void *, void *) {
    while (true) {
        debug("Waiting for fifo data");
        struct fifo_data_t *rec_data = k_fifo_get(&dispatcher_fifo, K_FOREVER);

        // Begin counting
        timing_t start = timing_counter_get();

        // Loop through the sequence
        for (int l = 0; l < rec_data->ledctl.loop; l++) {
            
            // Iterate over each command (end with ecountering 0)
            for (int i = 0; i < rec_data->ledctl.seq_len; i++) {
                struct holdtime_t *ht = k_malloc(sizeof(struct holdtime_t));
                memcpy(&ht->time, &rec_data->ledctl.hold_times[i], 2);
                struct k_condvar *ledsig = NULL;
    
                switch (rec_data->ledctl.colors[i]) {
                    case 'R':
                        ledsig = &rsig;
                        debug("Switched led to Red");
                        break;
                    case 'Y':
                        ledsig = &ysig;
                        debug("Switched led to Yellow");
                        break;
                    case 'G':
                        ledsig = &gsig;
                        debug("Switched led to Green");
                        break;
                    case 'O':
                        switch (*curcol) {
                            case Red:
                                ledsig = &rsig;
                                break;
                            case Yellow:
                                ledsig = &ysig;
                                break;
                            case Green:
                                ledsig = &gsig;
                                break;
                            default:
                                ledsig = NULL;
                                debug("Can't turn led off if it is already off.");
                                break;
                        }
    
                        debug("Switched led to Off");
                        break;
                    default:
                        k_oops();
                }
                    
                    // Send the signal if it was set and wait for a generous amount of time for a answer.
                    if (ledsig != NULL) {
                        debug("Waiting for lock to release");
                        if (k_mutex_lock(&lmux, K_MSEC(5000)) == 0) {
                             k_condvar_signal(ledsig);
                            // Wait for minimum hold time to pass before continuing
                            if (k_condvar_wait(&sig_ok, &lmux, K_MSEC(ht->time + HOLD_TIME_MS)) == 0) {
                                // No reason to wait here if we only toggle one color because it holds
                                if (rec_data->ledctl.seq_len > 1) k_msleep(ht->time);
                            } else {
                                debug("Waiting time expired!");
                            }
    
                        } else {
                            debug("Mutex timed out");
                        }
    
                        k_mutex_unlock(&lmux);
                    }
    
                    k_free(ht);
            }
        }

        debug("Dispatcher done! Execution time: %llu ns", timing_cycles_to_ns(timing_counter_get() - start));

        k_free(rec_data);
    }
}