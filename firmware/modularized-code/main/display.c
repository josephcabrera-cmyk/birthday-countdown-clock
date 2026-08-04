#include "display.h"

#include "max6951.h"
#include "direct_led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Seven-segment letter definitions.
// Note: PATTERN_G and PATTERN_D are deliberately left as they were written
// originally -- they hold the same value, so 'G' renders as 'D'.
#define PATTERN_A 0b01110111
#define PATTERN_B 0b00011111
#define PATTERN_C 0b01001110
#define PATTERN_D 0b00111101
#define PATTERN_E 0b01001111
#define PATTERN_F 0b01110001
#define PATTERN_G 0b00111101
#define PATTERN_H 0b00010111
#define PATTERN_I 0b00110000
#define PATTERN_M 0b01010101
#define PATTERN_N 0b00010101
#define PATTERN_O 0b00011101
#define PATTERN_R 0b00000101
#define PATTERN_S 0b01011011
#define PATTERN_T 0b00001111
#define PATTERN_U 0b00011100
#define PATTERN_Y 0b00111011

const uint8_t BIRTHDAY_pattern[8] = {
    PATTERN_B,
    PATTERN_I,
    PATTERN_R,
    PATTERN_T,
    PATTERN_H,
    PATTERN_D,
    PATTERN_A,
    PATTERN_Y
};

static const uint8_t YEAR_pattern[] = {
    PATTERN_Y,
    PATTERN_E,
    PATTERN_A,
    PATTERN_R
};

static const uint8_t MONTH_pattern[] = {
    PATTERN_M,
    PATTERN_O,
    PATTERN_N,
    PATTERN_T,
    PATTERN_H
};

static const uint8_t DAY_pattern[] = {
    PATTERN_D,
    PATTERN_A,
    PATTERN_Y
};

static const uint8_t HOUR_pattern[] = {
    PATTERN_H,
    PATTERN_O,
    PATTERN_U,
    PATTERN_R
};

static const uint8_t MIN_pattern[] = {
    PATTERN_M,
    PATTERN_I,
    PATTERN_N
};

static const uint8_t SEC_pattern[] = {
    PATTERN_S,
    PATTERN_E,
    PATTERN_C
};

const display_message_t preset_messages[] = {
    {"YEAR",     YEAR_pattern,     4},
    {"MONTH",    MONTH_pattern,    5},
    {"DAY",      DAY_pattern,      3},
    {"HOUR",     HOUR_pattern,     4},
    {"MIN",      MIN_pattern,      3},
    {"SEC",      SEC_pattern,      3},
    {"BIRTHDAY", BIRTHDAY_pattern, 8}
};

void clear_all_digits(void) {
    for(int i = 0; i <= 7; i++) {
        max6951_write(MAX6951_DIGIT_0 + i, 0x00);  // Clear MAX6951 digits
        vTaskDelay(pdMS_TO_TICKS(DIGIT_WRITE_DELAY_MS));
    }
    direct_led_display(10, false);  // Clear direct LED digit
}

void display_text_message(const uint8_t* pattern, uint8_t length) {
    // First clear all digits
    clear_all_digits();

    // Set no-decode mode for text digits
    max6951_set_decode_mode(0x00);  // No decode for any digit

    // Display the pattern starting from leftmost digit
    for(uint8_t i = 0; i < length && i < 8; i++) {
        max6951_write(MAX6951_DIGIT_0 + i, pattern[i]);
        vTaskDelay(pdMS_TO_TICKS(DIGIT_WRITE_DELAY_MS));
    }
}

void display_number(uint16_t number, uint8_t num_digits, uint8_t start_position) {
    // Set decode mode for number digits
    uint8_t decode_mask = 0;
    for(uint8_t i = 0; i < num_digits; i++) {
        decode_mask |= (1 << (start_position + i));
    }
    max6951_set_decode_mode(decode_mask);

    // Convert number to individual digits
    uint8_t digits[4] = {0};

    for(int i = num_digits - 1; i >= 0; i--) {
        digits[i] = number % 10;
        number /= 10;
    }

    // Display digits starting at specified position
    for(uint8_t i = 0; i < num_digits; i++) {
        max6951_write(MAX6951_DIGIT_0 + start_position + i, digits[i]);
        vTaskDelay(pdMS_TO_TICKS(DIGIT_WRITE_DELAY_MS));
    }
}
