#ifndef DEBUG_H
#define DEBUG_H


// Running time variables. Protected with mutex `lmux`
extern uint64_t rtime;
extern uint64_t ytime;
extern uint64_t gtime;

void debug_task(uint64_t *r, uint64_t *y, uint64_t *g);

/*
    Use CONFIG_DEBUG=y in prj.conf to enable this custom debug printing.
*/
#ifdef CONFIG_DEBUG
    // Print debug messages to serial port when CONFIG_DEBUG=y. Expands to printk(...).
    #define DBG(...) printk(__VA_ARGS__)
#else
    // Print debug messages to serial port when CONFIG_DEBUG=y. Expands to void.
    #define DBG(...)
#endif

#endif // DEBUG_H