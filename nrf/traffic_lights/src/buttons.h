#ifndef BUTTONS_H
#define BUTTONS_H

// Interrupt callback for the buttons
void manual_isr(void);
void red_toggle_isr(void);
void yellow_toggle_isr(void);
void green_toggle_isr(void);
void yblink_toggle_isr(void);

// Helper functions and variable to enable an disable interrupts for pause button
// and to detect if the interrupts are enabled or disabled
void interrupt_enable(void);
void interrupt_disable(void);

bool init_buttons(void);

#endif