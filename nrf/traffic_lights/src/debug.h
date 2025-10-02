#ifndef DEBUG_H
#define DEBUG_H

extern volatile bool print_debug_messages;

/*
    This function behaves like printk, but instead of printing directly to serial port, it queues the debug string to be printed
    by the debug task. The formatted string must not exceed 128 characters when included the null character at the end.
*/
void schedule_printk(const char *fmt, size_t argc, uintptr_t *args);

// Schedule printk function to debug task
#define debug(fmt, ...) \
    if (print_debug_messages) {\
        do {\
            uintptr_t args[] = { __VA_OPT__( (uintptr_t)(__VA_ARGS__) ) };\
            size_t argc = sizeof(args) / sizeof(uintptr_t);\
            schedule_printk(fmt, argc, args);\
        } while (0);\
    }\

#endif // DEBUG_H