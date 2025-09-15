#ifndef DISPATCHER_H
#define DISPATCHER_H

extern void uart_task(void *, void *, void *);
extern void dispatcher_task(void *, void *, void *);

extern struct k_fifo command_fifo;
// extern struct fifo_data data;

bool init_uart(void);

#endif