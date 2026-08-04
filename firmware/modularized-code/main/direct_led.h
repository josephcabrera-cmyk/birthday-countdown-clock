#pragma once

#include <stdint.h>
#include <stdbool.h>

// The ninth digit. The MAX6951 drives eight; this one hangs directly off
// eight GPIOs through discrete ballast resistors (R8-R15) and shows the
// ones place of the seconds.

void direct_led_init(void);

// number 0-9 lights that digit; 10 blanks it.
void direct_led_display(uint8_t number, bool show_dp);
