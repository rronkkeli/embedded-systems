#ifndef MUX_H
#define MUX_H

extern struct k_mutex rmux;
extern struct k_mutex ymux;
extern struct k_mutex gmux;

extern struct k_condvar rsig;
extern struct k_condvar ysig;
extern struct k_condvar gsig;

extern struct k_fifo ht_fifo;

struct holdtime_t {
    void *fifo_reserved;
    int time;
};

#endif