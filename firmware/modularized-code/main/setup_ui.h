#pragma once

#include <stdint.h>

// Two-stage setup. First the current wall-clock time, which is written to
// the DS3231; then the target birthday, which is held in RAM only and so is
// lost on every power cycle.

extern uint8_t g_birthday_month;
extern uint8_t g_birthday_day;
extern uint8_t g_birthday_hour;
extern uint8_t g_birthday_minute;
extern uint8_t g_birthday_second;

// Runs at boot: current date/time, then the birthday.
void handle_setup_mode(void);

// Re-entered from the countdown by pressing SET.
// Known issue: the values cannot actually be changed once first set.
void handle_birthday_setup_mode(void);
