#ifndef LEDCTL_H
#define LEDCTL_H

extern const uint32_t HOLD_TIME_MS;
extern bool paused;

// This simplifies the state machine syntax (instead of using integers)
enum State { Red, Yellow, Green, Yblink, Manual };

// Holds the state of two leds
enum Color { LOff, LRed, LGreen, LYellow };

// Current state variables initialized to red
extern enum State state;
extern enum State cont;
extern enum Color color;

extern const k_tid_t redth;
extern const k_tid_t yellowth;
extern const k_tid_t greenth;

extern volatile bool red_ignore;
extern volatile bool yellow_ignore;
extern volatile bool green_ignore;

bool init_leds(void);

/*
    Hold the red led on for a specified time, or hold it for a default time. If ht_fifo time
    most significant bit is set, led is turned off instead of on.

    Hold time, if given in manual mode, determines the minimum time for the led on or off state.
*/
void red(enum State *state, void  *, void *);

/*
    Hold the yellow led on for a specified time, or hold it for a default time. If ht_fifo time
    most significant bit is set, led is turned off instead of on.

    Hold time, if given in manual mode, determines the minimum time for the led on or off state.
*/
void yellow(enum State *state, void  *, void *);

/*
    Hold the green led on for a specified time, or hold it for a default time. If ht_fifo time
    most significant bit is set, led is turned off instead of on.

    Hold time, if given in manual mode, determines the minimum time for the led on or off state.
*/
void green(enum State *state, void  *, void *);

#endif