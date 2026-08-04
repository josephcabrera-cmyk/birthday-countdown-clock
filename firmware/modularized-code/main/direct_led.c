#include "direct_led.h"

#include "driver/gpio.h"

// Direct LED segment pin definitions
#define LED_SEG_A                  2     // Segment A
#define LED_SEG_B                  4     // Segment B
#define LED_SEG_C                  17    // Segment C
#define LED_SEG_D                  13    // Segment D
#define LED_SEG_E                  14    // Segment E
#define LED_SEG_F                  32    // Segment F
#define LED_SEG_G                  16    // Segment G
#define LED_SEG_DP                 19    // Decimal Point

// 7-segment display patterns for 0-9 (active high)
// Segments: DP G F E D C B A
static const uint8_t seven_seg_patterns[11] = {
    0b00111111,  // 0: ABCDEF
    0b00000110,  // 1: BC
    0b01011011,  // 2: ABDEG
    0b01001111,  // 3: ABCDG
    0b01100110,  // 4: BCFG
    0b01101101,  // 5: ACDFG
    0b01111101,  // 6: ACDEFG
    0b00000111,  // 7: ABC
    0b01111111,  // 8: ABCDEFG
    0b01101111,  // 9: ABCDFG
    0b00000000   // Blank: No segments
};

void direct_led_init(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = 0,
        .pull_up_en = 0,
        .pin_bit_mask = (1ULL << LED_SEG_A) |
                        (1ULL << LED_SEG_B) |
                        (1ULL << LED_SEG_C) |
                        (1ULL << LED_SEG_D) |
                        (1ULL << LED_SEG_E) |
                        (1ULL << LED_SEG_F) |
                        (1ULL << LED_SEG_G) |
                        (1ULL << LED_SEG_DP)
    };
    gpio_config(&io_conf);

    // Initialize all segments to OFF
    gpio_set_level(LED_SEG_A, 0);
    gpio_set_level(LED_SEG_B, 0);
    gpio_set_level(LED_SEG_C, 0);
    gpio_set_level(LED_SEG_D, 0);
    gpio_set_level(LED_SEG_E, 0);
    gpio_set_level(LED_SEG_F, 0);
    gpio_set_level(LED_SEG_G, 0);
    gpio_set_level(LED_SEG_DP, 0);
}

void direct_led_display(uint8_t number, bool show_dp) {
    if (number > 10) number = 0;

    uint8_t pattern = seven_seg_patterns[number];

    gpio_set_level(LED_SEG_A, (pattern >> 0) & 0x01);
    gpio_set_level(LED_SEG_B, (pattern >> 1) & 0x01);
    gpio_set_level(LED_SEG_C, (pattern >> 2) & 0x01);
    gpio_set_level(LED_SEG_D, (pattern >> 3) & 0x01);
    gpio_set_level(LED_SEG_E, (pattern >> 4) & 0x01);
    gpio_set_level(LED_SEG_F, (pattern >> 5) & 0x01);
    gpio_set_level(LED_SEG_G, (pattern >> 6) & 0x01);
    gpio_set_level(LED_SEG_DP, show_dp ? 1 : 0);
}
