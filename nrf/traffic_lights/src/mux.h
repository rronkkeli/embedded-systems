#ifndef MUX_H
#define MUX_H

// Led mutually exclusive lock
extern struct k_mutex lmux;

// Signal for red led
extern struct k_condvar rsig;
// SIgnal for yellow led
extern struct k_condvar ysig;
// Signal for green led
extern struct k_condvar gsig;
// Signal for release
extern struct k_condvar sig_ok;
// Fifo data for hold time
extern struct k_fifo ht_fifo;

// Semaphore inform main thread that led tasks are ready
extern struct k_sem threads_ready;

struct holdtime_t {
    void *fifo_reserved;
    uint16_t time;
};

#endif