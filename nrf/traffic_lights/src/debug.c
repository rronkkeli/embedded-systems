/** Yeah, I may have gone a little overboard with this debugger. The reason behind that is that I wanted to
 *  learn how to actually pass variable amount of arguments to other tasks, so that other tasks don't have
 *  to format the debug messages themselves and can just immediately carry on as usual.
 * 
 *  This is how this is supposed to work: A normal working thread calls debug function with some debug message and
 *  formatting variables, like any other formatting print function. This debug function quickly allocates memory
 *  from memory slabs for the fifo messages, and for the arguments from a dedicated slab.
 */

#include <zephyr/kernel.h>
#include <zephyr/timing/timing.h>

#include "debug.h"
#include "mux.h"

// Argument amount that can be stored in one memory slab block.
#define ARGS_PER_BLOCK 4
#define MAX_ARGS 4
// Block amount in one memory slab.
#define MEM_SLAB_BLOCKS 32

void debug_task(void *, void*, void *);

/* 
    Holds a tight, non-fragmentable memory block for debug messages. Each message may only be 128 characters long.
    There is room for 16 simultaneous messages in total. Use `k_mem_slab_alloc(&debug_messages, (void **)&fifodata, some_wait_time)`
    to allocate a slab block for a debug message. 
*/
struct k_mem_slab debug_messages;

/*
    FIFO queue for debug messages. Expects `struct debug_fifo_t` type data.
*/
struct k_fifo debug_fifo;

/*
    Store one single argument and point to next four argument block, which is unnlikely. Usually 4 is quite enough for any debug message.
*/
struct debug_t {
    const char *fmt;
    size_t argc;
    uintptr_t args[MAX_ARGS];
};

/* 
    Each debug fifo element holds a string pointer to arbitrary sized string that needs to be formatted.
*/
struct debug_fifo_t {
    void *fifo_reserved;
    struct debug_t dbg;
};

// Initialize statistics
struct statistics statistics = {
    .btns = {
        .button_manual_toggle = 0,
        .button_red_toggle = 0,
        .button_yellow_toggle = 0,
        .button_green_toggle = 0,
        .button_yellow_blink_toggle = 0
    },

    .leds = {
        .red.toggled = 0,
        .blue.toggled = 0
    },

    .threads = {
        .main = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },

        .red = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },

        .yellow = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },

        .green = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },
        
        .uart = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },
        
        .dispatcher = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        },

        .debug = {
            .typical_signal_wait_time_ms = 0,
            .typical_runtime_ns = 0
        }
    }
};

volatile bool print_debug_messages = false;

K_FIFO_DEFINE(debug_fifo);
K_THREAD_DEFINE(debugth, 1024, debug_task, NULL, NULL, NULL, 10, 0, 0);
K_MEM_SLAB_DEFINE(debug_messages, sizeof(struct debug_t), MEM_SLAB_BLOCKS, 4);


void debug_task(void *, void *, void *) {
    struct debug_fifo_t *data;

    while (1) {
        // Check the fifo queue for available messages and parse them until none are left. Yield for a longer period after the queue has been
        // processed to give room for more important tasks.
        while ((data = k_fifo_get(&debug_fifo, K_NO_WAIT)) != NULL) {
            printk("DEBUG: ");
            switch (data->dbg.argc) {
                case 0:
                    printk(data->dbg.fmt);
                    break;
                case 1:
                    printk(data->dbg.fmt, data->dbg.args[0]);
                    break;
                case 2:
                    printk(data->dbg.fmt, data->dbg.args[0], data->dbg.args[1]);
                    break;
                case 3:
                    printk(data->dbg.fmt, data->dbg.args[0], data->dbg.args[1], data->dbg.args[2]);
                    break;
                case 4:
                    printk(data->dbg.fmt, data->dbg.args[0], data->dbg.args[1], data->dbg.args[2], data->dbg.args[3]);
                    break;
            }

            printk("\n");
            k_mem_slab_free(&debug_messages, data);
        }

        k_msleep(200);
    }
}

void schedule_printk(const char *fmt, size_t argc, uintptr_t *args) {
    struct debug_fifo_t *data;

    if (k_mem_slab_alloc(&debug_messages, (void **)&data, K_NO_WAIT) == 0) {
        data->dbg.fmt = fmt;
        data->dbg.argc = argc;
        memcpy(data->dbg.args, args, argc * sizeof(uintptr_t));
        k_fifo_put(&debug_fifo, data);
    } else {
        printk("I die :(\n");
    }
}