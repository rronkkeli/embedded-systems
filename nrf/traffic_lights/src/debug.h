#ifndef DEBUG_H
#define DEBUG_H

extern volatile bool print_debug_messages;

/*
    This function behaves like printk, but instead of printing directly to serial port, it queues the debug string to be printed
    by the debug task. The formatted string must not exceed 128 characters when included the null character at the end.
*/
void schedule_printk(const char *fmt, size_t argc, uintptr_t *args);

// Schedule printk function to debug task.
#define debug(fmt, ...) \
    if (print_debug_messages) {\
        do {\
            uintptr_t args[] = { __VA_OPT__( (uintptr_t)(__VA_ARGS__) ) };\
            size_t argc = sizeof(args) / sizeof(uintptr_t);\
            schedule_printk(fmt, argc, args);\
        } while (0);\
    }\

struct led_stat_t {
    // Keep track of the toggle amount of this led.
    uint32_t toggled;
};

struct led_stats {
    // Statistics for red led.
    struct led_stat_t red;
    // Statistics for blue led.
    struct led_stat_t blue;
};

struct btn_stat_t {
    // Keep track on how many times a button has been pushed.
    uint32_t pushed;
    // Typical ripple count for this button, aka how many toggles does it make before settling.
    uint32_t typical_ripple;
};

struct btn_stats {
    // Button statistics for manual control button.
    struct btn_stat_t button_manual_toggle;
    // Button statistics for red color toggle button.
    struct btn_stat_t button_red_toggle;
    // Button statistics for yellow color toggle button.
    struct btn_stat_t button_yellow_toggle;
    // Button statistics for green color toggle button.
    struct btn_stat_t button_green_toggle;
    // Button statistics for yellow blink toggle button.
    struct btn_stat_t button_yellow_blink_toggle;    
};

struct thread_stat_t {
    // How long does the thread typically wait for a signal in automatic mode.
    uint64_t typical_signal_wait_time_ms;
    // How long does the thread typically take to finish the task, when it gets a signal.
    uint64_t typical_runtime_ns;
};

struct thread_stats {
    // Main thread statistics.
    struct thread_stat_t main;
    // Red led task thread statistics.
    struct thread_stat_t red;
    // Yellow led task thread statistics.
    struct thread_stat_t yellow;
    // Green led task thread statistics.
    struct thread_stat_t green;
    // UART task thread statistics.
    struct thread_stat_t uart;
    // Dispatcher task thread statistics.
    struct thread_stat_t dispatcher;
    // Debug task thread statistics.
    struct thread_stat_t debug;
};

struct statistics {
    // Statistics of leds.
    struct led_stats leds;
    // Statistics of buttons.
    struct btn_stats btns;
    // Statistics of threads.
    struct thread_stats threads;
};

extern struct statistics statistics;
#endif // DEBUG_H