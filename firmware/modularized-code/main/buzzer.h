#pragma once

// PUI Audio SMTB-0927-TW-R, driven through R7 (47 ohm) from a GPIO
// using LEDC hardware PWM.

void configure_buzzer(void);
void play_note(int frequency, int duration_ms);
void play_birthday_song(void);
