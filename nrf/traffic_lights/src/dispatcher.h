#ifndef DISPATCHER_H
#define DISPATCHER_H

extern void uart_task(void *, void *, void *);
extern void dispatcher_task(enum Color *, void *, void *);

extern struct k_fifo command_fifo;

bool init_uart(void);

#endif