#pragma once

#include <stdint.h>

// Composition layer over the MAX6951 and the direct-driven ninth digit.
// The driver is flipped between no-decode (arbitrary segment patterns, used
// to spell the setup prompts) and hex decode (plain numbers) as needed.

typedef struct {
    const char* name;           // Message name for reference
    const uint8_t* patterns;    // Array of 7-segment patterns
    uint8_t length;             // Length of the message
} display_message_t;

// Index order: YEAR, MONTH, DAY, HOUR, MIN, SEC, BIRTHDAY
extern const display_message_t preset_messages[];

extern const uint8_t BIRTHDAY_pattern[8];

// Blanks all eight driver digits and the direct-driven ninth.
void clear_all_digits(void);

// Switches to no-decode and writes a segment pattern from the left.
void display_text_message(const uint8_t* pattern, uint8_t length);

// Switches the affected digits to decode and writes a number.
void display_number(uint16_t number, uint8_t num_digits, uint8_t start_position);
