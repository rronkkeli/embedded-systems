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

// Function prototypes for color tasks
void red(enum State *, enum Color *, bool *);
void yellow(enum State *, enum Color *, bool *);
void green(enum State *, enum Color *, bool *);
void yblink(enum State *, enum Color *, bool *);

void set_red(void);
void set_yellow(void);
void set_green(void);
void set_off(void);

#endif