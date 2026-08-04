#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"

// SW1 = SET (IO25), SW2 = UP (IO26), SW3 = DOWN (IO27).
// Each switch is active low with a 10k pullup and a 1uF cap to ground,
// so there is a ~10 ms hardware RC underneath this software debounce.

typedef struct {
    uint8_t pin;
    TickType_t last_press_time;
    bool last_state;
} button_t;

extern button_t btn_up;
extern button_t btn_down;
extern button_t btn_set;

void init_buttons(void);

// Edge-triggered: returns true once on the press transition.
bool is_button_pressed(button_t *button);
