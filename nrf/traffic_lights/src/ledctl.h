#ifndef LEDCTL_H
#define LEDCTL_H

extern const uint32_t HOLD_TIME_MS;
extern volatile bool paused;

// This simplifies the state machine syntax (instead of using integers)
enum State { Auto, Manual, Blink };

// Holds the state of two leds
enum Color { Off, Red, Yellow, Green };

// Current state variables initialized to red
extern volatile enum State state;
extern volatile enum Color cont;
extern volatile enum Color color;

extern const k_tid_t redth;
extern const k_tid_t yellowth;
extern const k_tid_t greenth;

bool init_leds(void);

/*
    Set red on, if it is not on. Otherwise set it off.
*/
void red(enum State *state, enum Color *color, uint64_t *rtime);

/*
    Set yellow on, if it is not on. Otherwise set it off.
*/
void yellow(enum State *state, enum Color *color, uint64_t *ytime);

/*
    Set green on, if it is not on. Otherwise set it off.
*/
void green(enum State *state, enum Color *color, uint64_t *gtime);

#endif