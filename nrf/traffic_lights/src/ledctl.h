#ifndef LEDCTL_H
#define LEDCTL_H

extern const uint32_t HOLD_TIME_MS;
extern bool paused;

// This simplifies the state machine syntax (instead of using integers)
enum State { Red, Yellow, Green, Yblink, Manual };

// Holds the state of two leds
enum Color { LOff, LRed, LGreen, LYellow };

int init_leds(void);

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