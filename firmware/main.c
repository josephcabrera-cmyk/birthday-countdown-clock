#include <stdio.h>
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include "esp_timer.h"
#include "esp_system.h"

// Pin definitions from schematic
#define I2C_MASTER_SCL_IO           22    // SCL pin
#define I2C_MASTER_SDA_IO           21    // SDA pin
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

#define SPI_MOSI_PIN               23     // DIN pin
#define SPI_SCLK_PIN               18     // CLK pin
#define SPI_CS_PIN                 5     // CS pin

// Updated direct LED segment pin definitions
#define LED_SEG_A                  2     // Segment A
#define LED_SEG_B                  4     // Segment B
#define LED_SEG_C                  17    // Segment C
#define LED_SEG_D                  13    // Segment D
#define LED_SEG_E                  14    // Segment E
#define LED_SEG_F                  32    // Segment F
#define LED_SEG_G                  16    // Segment G
#define LED_SEG_DP                 19    // Decimal Point

// Button definitions
#define BTN_UP_PIN      26
#define BTN_DOWN_PIN    27
#define BTN_SET_PIN     25

// Button debounce time in milliseconds
#define DEBOUNCE_TIME   50

// Add buzzer-related defines
#define BUZZER_GPIO 33 // From schematic
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL LEDC_CHANNEL_0

// Musical Notes Frequencies (in Hz)
#define NOTE_C4 262
#define NOTE_D4 294
#define NOTE_E4 330
#define NOTE_F4 349
#define NOTE_G4 392
#define NOTE_A4 440
#define NOTE_B4 494

// Note duration (in milliseconds)
#define WHOLE_NOTE 1000
#define HALF_NOTE 500
#define QUARTER_NOTE 250
#define EIGHTH_NOTE 125

// Seven-segment LED definitions
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

#define DIGIT_WRITE_DELAY_MS 1  // 1ms delay between digit writes

// DS3231 registers
#define DS3231_ADDR                0x68
#define DS3231_SECONDS_REG         0x00
#define DS3231_MINUTES_REG         0x01
#define DS3231_HOURS_REG           0x02
#define DS3231_DAY_REG             0x03
#define DS3231_DATE_REG            0x04
#define DS3231_MONTH_REG           0x05
#define DS3231_YEAR_REG            0x06

// MAX6951 commands
#define MAX6951_DECODE_MODE        0x01
#define MAX6951_INTENSITY          0x02
#define MAX6951_SCAN_LIMIT         0x03
#define MAX6951_CONFIG             0x04
#define MAX6951_DIGIT_0            0x20
#define MAX6951_SHUTDOWN           0x00

// Prototypes
static esp_err_t max6951_write(uint8_t reg, uint8_t data);
static void direct_led_display(uint8_t number, bool show_dp);
static esp_err_t ds3231_set_time(uint8_t seconds, uint8_t minutes, uint8_t hours, 
                                 uint8_t date, uint8_t month, uint8_t year);
static void display_birthday_prompt(void);
static void handle_setup_mode(void);
static void handle_birthday_setup_mode(void);
static esp_err_t i2c_master_init(void);
static esp_err_t spi_init(void);
static esp_err_t max6951_init(void);
static esp_err_t max6951_init1(void);
static int calculate_days_until_birthday(int current_date, int current_month, int current_year,
                                          int birthday_date, int birthday_month);
static esp_err_t ds3231_read_time(uint8_t *seconds, uint8_t *minutes, uint8_t *hours, uint8_t *date, 
                                  uint8_t *month, uint8_t *year);
static void configure_buzzer(void);
static void play_note(int frequency, int duration_ms);
static void play_birthday_song(void);

// Globals
static const char *TAG = "BIRTHDAY_COUNTDOWN";

// Melody structure
typedef struct {
    int frequency;
    int duration;
} Note;

// Define setup states
typedef enum {
    // Date/Time setup states
    SETUP_YEAR,
    SETUP_MONTH,
    SETUP_DAY,
    SETUP_HOUR,
    SETUP_MINUTE,
    SETUP_SECOND,
    // Birthday setup states
    SETUP_BIRTHDAY_MONTH,
    SETUP_BIRTHDAY_DAY,
    SETUP_BIRTHDAY_HOUR,
    SETUP_BIRTHDAY_MINUTE,
    SETUP_BIRTHDAY_SECOND,
    SETUP_DONE
} setup_state_t;

// Add new structure for birthday values
typedef struct {
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} birthday_values_t;

// Structure to hold setup values
typedef struct {
    // Current date/time values
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    // Birthday values
    uint8_t birthday_month;
    uint8_t birthday_day;
    uint8_t birthday_hour;
    uint8_t birthday_minute;
    uint8_t birthday_second;
} setup_values_t;


// Add global variables for birthday values
static uint8_t g_birthday_month;
static uint8_t g_birthday_day;
static uint8_t g_birthday_hour;
static uint8_t g_birthday_minute;
static uint8_t g_birthday_second;

// Button state structure
typedef struct {
    uint8_t pin;
    TickType_t last_press_time;
    bool last_state;
} button_t;

// Date/time limits structure
typedef struct {
    uint16_t min;
    uint16_t max;
} value_limits_t;

// Structure for preset messages
typedef struct {
    const char* name;           // Message name for reference
    const uint8_t* patterns;    // Array of 7-segment patterns
    uint8_t length;            // Length of the message
} display_message_t;

static const uint8_t BIRTHDAY_pattern[] = {
    PATTERN_B,
    PATTERN_I,
    PATTERN_R,
    PATTERN_T,
    PATTERN_H,
    PATTERN_D,
    PATTERN_A,
    PATTERN_Y
};

// Message patterns defined using the constants
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

// Modified display message structure
static const display_message_t preset_messages[] = {
    {"YEAR",     YEAR_pattern,     4},
    {"MONTH",    MONTH_pattern,    5},
    {"DAY",      DAY_pattern,      3},
    {"HOUR",     HOUR_pattern,     4},
    {"MIN",      MIN_pattern,      3},
    {"SEC",      SEC_pattern,      3},
    {"BIRTHDAY", BIRTHDAY_pattern, 8}
};

// Happy Birthday Melody (simplified)
const Note happy_birthday[] = {
    {NOTE_C4, QUARTER_NOTE}, {NOTE_C4, QUARTER_NOTE}, {NOTE_D4, HALF_NOTE}, 
    {NOTE_C4, HALF_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_E4, WHOLE_NOTE}, 
    {NOTE_C4, QUARTER_NOTE}, {NOTE_C4, QUARTER_NOTE}, {NOTE_D4, HALF_NOTE}, 
    {NOTE_C4, HALF_NOTE}, {NOTE_G4, HALF_NOTE}, {NOTE_F4, WHOLE_NOTE}, 
    {NOTE_C4, QUARTER_NOTE}, {NOTE_C4, QUARTER_NOTE}, {NOTE_C4, HALF_NOTE}, 
    {NOTE_A4, HALF_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_E4, HALF_NOTE}, 
    {NOTE_D4, WHOLE_NOTE}, {NOTE_B4, QUARTER_NOTE}, {NOTE_B4, QUARTER_NOTE}, 
    {NOTE_A4, HALF_NOTE}, {NOTE_F4, HALF_NOTE}, {NOTE_G4, HALF_NOTE}, 
    {NOTE_F4, WHOLE_NOTE}
};

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

static button_t btn_up = {BTN_UP_PIN, 0, true};
static button_t btn_down = {BTN_DOWN_PIN, 0, true};
static button_t btn_set = {BTN_SET_PIN, 0, true};

// Function to initialize buttons
static void init_buttons(void) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BTN_UP_PIN) |
                       (1ULL << BTN_DOWN_PIN) |
                       (1ULL << BTN_SET_PIN),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE
    };
    gpio_config(&io_conf);
}

// SPI handle
spi_device_handle_t spi;

// Function to handle button debouncing
static bool is_button_pressed(button_t* button) {
    bool current_state = !gpio_get_level(button->pin);  // Active low
    TickType_t current_time = xTaskGetTickCount();
    
    if (current_state != button->last_state) {
        if ((current_time - button->last_press_time) >= pdMS_TO_TICKS(DEBOUNCE_TIME)) {
            button->last_press_time = current_time;
            button->last_state = current_state;
            return current_state;
        }
    }
    return false;
}

// Function to clear all digits on MAX6951 and direct LED
static void clear_all_digits(void) {
    for(int i = 0; i <= 7; i++) {
        max6951_write(MAX6951_DIGIT_0 + i, 0x00);  // Clear MAX6951 digits
        vTaskDelay(pdMS_TO_TICKS(DIGIT_WRITE_DELAY_MS));
    }
    direct_led_display(10, false);  // Clear direct LED digit
}

// Function to set decode mode for specific digits
static esp_err_t max6951_set_decode_mode(uint8_t decode_mask) {
    return max6951_write(MAX6951_DECODE_MODE, decode_mask);
}

// Function to display text message
static void display_text_message(const uint8_t* pattern, uint8_t length) {
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

// Function to display number
static void display_number(uint16_t number, uint8_t num_digits, uint8_t start_position) {
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

// Function to display current setup state with value
static void display_setup_value(setup_state_t state, uint16_t value) {
    // First determine which message pattern to use
    const uint8_t* pattern;
    uint8_t length;
    
    switch(state) {
        case SETUP_YEAR:
            pattern = preset_messages[0].patterns;
            length = preset_messages[0].length;
            break;
        case SETUP_MONTH:
        case SETUP_BIRTHDAY_MONTH:
            pattern = preset_messages[1].patterns;
            length = preset_messages[1].length;
            break;
        case SETUP_DAY:
        case SETUP_BIRTHDAY_DAY:
            pattern = preset_messages[2].patterns;
            length = preset_messages[2].length;
            break;
        case SETUP_HOUR:
        case SETUP_BIRTHDAY_HOUR:
            pattern = preset_messages[3].patterns;
            length = preset_messages[3].length;
            break;
        case SETUP_MINUTE:
        case SETUP_BIRTHDAY_MINUTE:
            pattern = preset_messages[4].patterns;
            length = preset_messages[4].length;
            break;
        case SETUP_SECOND:
        case SETUP_BIRTHDAY_SECOND:
            pattern = preset_messages[5].patterns;
            length = preset_messages[5].length;
            break;
        default:
            return;
    }
    
    // Display the text message
    display_text_message(pattern, length);
    
  // Determine number of digits based on state
    uint8_t num_digits;
    switch(state) {
        case SETUP_YEAR:
            num_digits = 4;  // YYYY
            break;
        case SETUP_MONTH:
        case SETUP_DAY:
        case SETUP_HOUR:
        case SETUP_MINUTE:
        case SETUP_SECOND:
        case SETUP_BIRTHDAY_MONTH:
        case SETUP_BIRTHDAY_DAY:
        case SETUP_BIRTHDAY_HOUR:
        case SETUP_BIRTHDAY_MINUTE:
        case SETUP_BIRTHDAY_SECOND:
            num_digits = 2;  // MM, DD, HH, MM, SS
            break;
        default:
            return;
    }
    
// Add one space after text based on pattern length and type
uint8_t start_pos;
if (state == SETUP_YEAR) {
	direct_led_display(value % 10, false);
    start_pos = 5;
} else {
    // Adjust start position based on pattern length
    switch(state) {
        case SETUP_MONTH:
        case SETUP_BIRTHDAY_MONTH:
            start_pos = 6;  // MONTH is 5 chars, need more space
            break;
        case SETUP_HOUR:
        case SETUP_BIRTHDAY_HOUR:
            start_pos = 5;  // HOUR is 4 chars, need more space
            break;
        default:
            start_pos = 4;  // For DAY, MIN, SEC (3 chars each)
            break;
    }
}

// Display the number
display_number(value, num_digits, start_pos);
}

// Helper function to get maximum days in a month
static uint8_t get_max_days_in_month(uint8_t month, uint16_t year) {
    static const uint8_t days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    
    // Handle February in leap years
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            return 29;
        }
    }
    
    return days_in_month[month - 1];
}

// Function to adjust value with wrapping
static uint16_t adjust_value(uint16_t current_value, bool increment, setup_state_t state, uint16_t year, uint8_t month) {
    value_limits_t limits = {0};
    
    // Get limits based on state
    switch(state) {
        case SETUP_YEAR:
            limits = (value_limits_t){2024, 2099};
            break;
        case SETUP_MONTH:
            limits = (value_limits_t){1, 12};
            break;
        case SETUP_DAY:
            limits = (value_limits_t){1, get_max_days_in_month(month, year)};
            break;
        case SETUP_HOUR:
            limits = (value_limits_t){0, 23};
            break;
        case SETUP_MINUTE:
        case SETUP_SECOND:
            limits = (value_limits_t){0, 59};
            break;
        default:
            return current_value;
    }
    
    // Adjust value with wrapping
    if (increment) {
        current_value = (current_value >= limits.max) ? limits.min : current_value + 1;
    } else {
        current_value = (current_value <= limits.min) ? limits.max : current_value - 1;
    }
    
    return current_value;
}

// Function to handle the setup process
static void handle_setup_mode(void) {
    ESP_LOGI(TAG, "Entering setup mode");
    
   setup_values_t values = {
        .year = 2024,
        .month = 1,
        .day = 1,
        .hour = 0,
        .minute = 0,
        .second = 0,
        // Initialize birthday values
        .birthday_month = 1,
        .birthday_day = 1,
        .birthday_hour = 0,
        .birthday_minute = 0,
        .birthday_second = 0
    };
    
    setup_state_t current_state = SETUP_YEAR;
    bool setup_complete = false;
    
    // Display initial prompt
    display_setup_value(current_state, values.year);
    ESP_LOGI(TAG, "Initial setup display - YEAR: %d", values.year);
    
    while (!setup_complete) {
        // Handle Up button
        if (is_button_pressed(&btn_up)) {
            
            switch (current_state) {
                // Date setup cases
                case SETUP_YEAR:
                    values.year = adjust_value(values.year, true, current_state, values.year, values.month);
                    direct_led_display(values.year % 10, false);
                    break;
                case SETUP_MONTH:
                    values.month = adjust_value(values.month, true, current_state, values.year, values.month);
                    break;
                case SETUP_DAY:
                    values.day = adjust_value(values.day, true, current_state, values.year, values.month);
                    break;
                case SETUP_HOUR:
                    values.hour = adjust_value(values.hour, true, current_state, values.year, values.month);
                    break;
                case SETUP_MINUTE:
                    values.minute = adjust_value(values.minute, true, current_state, values.year, values.month);
                    break;
                case SETUP_SECOND:
                    values.second = adjust_value(values.second, true, current_state, values.year, values.month);
                    break;
                    
                // Birthday setup cases
                case SETUP_BIRTHDAY_MONTH:
                    values.birthday_month = adjust_value(values.birthday_month, true, SETUP_MONTH, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_DAY:
                    values.birthday_day = adjust_value(values.birthday_day, true, SETUP_DAY, 2024, values.birthday_month);
                    break;
                case SETUP_BIRTHDAY_HOUR:
                    values.birthday_hour = adjust_value(values.birthday_hour, true, SETUP_HOUR, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_MINUTE:
                    values.birthday_minute = adjust_value(values.birthday_minute, true, SETUP_MINUTE, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_SECOND:
                    values.birthday_second = adjust_value(values.birthday_second, true, SETUP_SECOND, 2024, 0);
                    break;
                case SETUP_DONE:
                    break;
            }
            
            // Update display with new value
            uint16_t display_value = (current_state == SETUP_YEAR) ? values.year :
                                   (current_state == SETUP_MONTH) ? values.month :
                                   (current_state == SETUP_DAY) ? values.day :
                                   (current_state == SETUP_HOUR) ? values.hour :
                                   (current_state == SETUP_MINUTE) ? values.minute :
                                   (current_state == SETUP_BIRTHDAY_MONTH) ? values.birthday_month :
                                   (current_state == SETUP_BIRTHDAY_DAY) ? values.birthday_day :
                                   (current_state == SETUP_BIRTHDAY_HOUR) ? values.birthday_hour :
                                   (current_state == SETUP_BIRTHDAY_MINUTE) ? values.birthday_minute :
                                   (current_state == SETUP_BIRTHDAY_SECOND) ? values.birthday_second :
                                   values.second;
            
            display_setup_value(current_state, display_value);
        }
        // Handle Down button
        if (is_button_pressed(&btn_down)) {
            
            switch (current_state) {
                // Date setup cases
                case SETUP_YEAR:
                    values.year = adjust_value(values.year, false, current_state, values.year, values.month);
                    direct_led_display(values.year % 10, false);
                    break;
                case SETUP_MONTH:
                    values.month = adjust_value(values.month, false, current_state, values.year, values.month);
                    break;
                case SETUP_DAY:
                    values.day = adjust_value(values.day, false, current_state, values.year, values.month);
                    break;
                case SETUP_HOUR:
                    values.hour = adjust_value(values.hour, false, current_state, values.year, values.month);
                    break;
                case SETUP_MINUTE:
                    values.minute = adjust_value(values.minute, false, current_state, values.year, values.month);
                    break;
                case SETUP_SECOND:
                    values.second = adjust_value(values.second, false, current_state, values.year, values.month);
                    break;
                    
                // Birthday setup cases
                case SETUP_BIRTHDAY_MONTH:
                    values.birthday_month = adjust_value(values.birthday_month, false, SETUP_MONTH, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_DAY:
                    values.birthday_day = adjust_value(values.birthday_day, false, SETUP_DAY, 2024, values.birthday_month);
                    break;
                case SETUP_BIRTHDAY_HOUR:
                    values.birthday_hour = adjust_value(values.birthday_hour, false, SETUP_HOUR, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_MINUTE:
                    values.birthday_minute = adjust_value(values.birthday_minute, false, SETUP_MINUTE, 2024, 0);
                    break;
                case SETUP_BIRTHDAY_SECOND:
                    values.birthday_second = adjust_value(values.birthday_second, false, SETUP_SECOND, 2024, 0);
                    break;
                case SETUP_DONE:
                    break;
            }
            
            // Update display with new value
            uint16_t display_value = (current_state == SETUP_YEAR) ? values.year :
                                   (current_state == SETUP_MONTH) ? values.month :
                                   (current_state == SETUP_DAY) ? values.day :
                                   (current_state == SETUP_HOUR) ? values.hour :
                                   (current_state == SETUP_MINUTE) ? values.minute :
                                   (current_state == SETUP_BIRTHDAY_MONTH) ? values.birthday_month :
                                   (current_state == SETUP_BIRTHDAY_DAY) ? values.birthday_day :
                                   (current_state == SETUP_BIRTHDAY_HOUR) ? values.birthday_hour :
                                   (current_state == SETUP_BIRTHDAY_MINUTE) ? values.birthday_minute :
                                   (current_state == SETUP_BIRTHDAY_SECOND) ? values.birthday_second :
                                   values.second;
            
            display_setup_value(current_state, display_value);
        }

// Handle Set button
if (is_button_pressed(&btn_set)) {
    if (current_state < SETUP_DONE) {
        // If we're at the last date setup state
        if (current_state == SETUP_SECOND) {
            // Save RTC values first
            ds3231_set_time(values.second, values.minute, values.hour,
                          values.day, values.month, values.year % 100);
            
            // Show birthday prompt
            display_birthday_prompt();
            
            // Move to first birthday setup state
            current_state = SETUP_BIRTHDAY_MONTH;
        }
        // If we're at the last birthday setup state
        else if (current_state == SETUP_BIRTHDAY_SECOND) {
            // Save birthday values to global variables
            g_birthday_month = values.birthday_month;
            g_birthday_day = values.birthday_day;
            g_birthday_hour = values.birthday_hour;
            g_birthday_minute = values.birthday_minute;
            g_birthday_second = values.birthday_second;
            
            ESP_LOGI(TAG, "Setup complete! Birthday values - Month: %d, Day: %d, Time: %02d:%02d:%02d",
                    g_birthday_month, g_birthday_day, g_birthday_hour, 
                    g_birthday_minute, g_birthday_second);
            setup_complete = true;
        }
        // For all other states, just move to the next state
        else {
            current_state++;
        }

        // If we're not done yet, display the next value
        if (!setup_complete) {
            uint16_t display_value = (current_state == SETUP_YEAR) ? values.year :
                                   (current_state == SETUP_MONTH) ? values.month :
                                   (current_state == SETUP_DAY) ? values.day :
                                   (current_state == SETUP_HOUR) ? values.hour :
                                   (current_state == SETUP_MINUTE) ? values.minute :
                                   (current_state == SETUP_BIRTHDAY_MONTH) ? values.birthday_month :
                                   (current_state == SETUP_BIRTHDAY_DAY) ? values.birthday_day :
                                   (current_state == SETUP_BIRTHDAY_HOUR) ? values.birthday_hour :
                                   (current_state == SETUP_BIRTHDAY_MINUTE) ? values.birthday_minute :
                                   (current_state == SETUP_BIRTHDAY_SECOND) ? values.birthday_second :
                                   values.second;
            
            display_setup_value(current_state, display_value);
        }
    }
}
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "Exiting setup mode - Starting countdown");
}

// Function to initialize direct LED GPIO pins
static void direct_led_init(void) {
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
    
// Function to display a number on the direct LED
static void direct_led_display(uint8_t number, bool show_dp) {
    if (number > 10) number = 0;
    
    uint8_t pattern = seven_seg_patterns[number];
    
 //   ESP_LOGI(TAG, "Displaying number: %d, Pattern: 0b%07b", number, pattern);
    
    gpio_set_level(LED_SEG_A, (pattern >> 0) & 0x01);
    gpio_set_level(LED_SEG_B, (pattern >> 1) & 0x01);
    gpio_set_level(LED_SEG_C, (pattern >> 2) & 0x01);
    gpio_set_level(LED_SEG_D, (pattern >> 3) & 0x01);
    gpio_set_level(LED_SEG_E, (pattern >> 4) & 0x01);
    gpio_set_level(LED_SEG_F, (pattern >> 5) & 0x01);
    gpio_set_level(LED_SEG_G, (pattern >> 6) & 0x01);
    gpio_set_level(LED_SEG_DP, show_dp ? 1 : 0);
}

// Add this function to set the time on the DS3231
static esp_err_t ds3231_set_time(uint8_t seconds, uint8_t minutes, uint8_t hours, 
                                 uint8_t date, uint8_t month, uint8_t year) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Convert decimal to BCD
    uint8_t bcd_seconds = ((seconds / 10) << 4) | (seconds % 10);
    uint8_t bcd_minutes = ((minutes / 10) << 4) | (minutes % 10);
    uint8_t bcd_hours = ((hours / 10) << 4) | (hours % 10);
    uint8_t bcd_date = ((date / 10) << 4) | (date % 10);
    uint8_t bcd_month = ((month / 10) << 4) | (month % 10);
    uint8_t bcd_year = ((year / 10) << 4) | (year % 10);
    
    // Start I2C write transaction
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    
    // Write to starting register (0x00 - Seconds)
    i2c_master_write_byte(cmd, 0x00, true);
    
    // Write time data
    i2c_master_write_byte(cmd, bcd_seconds, true);   // Seconds
    i2c_master_write_byte(cmd, bcd_minutes, true);   // Minutes
    i2c_master_write_byte(cmd, bcd_hours, true);     // Hours
    i2c_master_write_byte(cmd, 0x04, true);          // Day of week (not used)
    i2c_master_write_byte(cmd, bcd_date, true);      // Date
    i2c_master_write_byte(cmd, bcd_month, true);     // Month
    i2c_master_write_byte(cmd, bcd_year, true);      // Year
    
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    return err;
}


// Function to initialize I2C as master
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf);
    if (err != ESP_OK) return err;
    
    return i2c_driver_install(I2C_NUM_0, conf.mode, 
                            I2C_MASTER_RX_BUF_DISABLE,
                            I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Function to initialize SPI
static esp_err_t spi_init(void) {
    spi_bus_config_t buscfg = {
        .mosi_io_num = SPI_MOSI_PIN,
        .miso_io_num = -1,  // Not used
        .sclk_io_num = SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,  // Small transfers only
    };

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 1000000,
        .mode = 0,
        .spics_io_num = SPI_CS_PIN,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY, // MAX6951 doesn't need dummy bits
    };

    esp_err_t err = spi_bus_initialize(HSPI_HOST, &buscfg, 0);
    if (err != ESP_OK) return err;

    return spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
}

static void display_birthday_prompt(void) {
    // Display "BIRTHDAY" message and wait for button press
    display_text_message(BIRTHDAY_pattern, 8);
    
    // Wait for SET button press while keeping decimal point off
    direct_led_display(10, false);  // Blank the direct LED
    
    while (1) {
        if (is_button_pressed(&btn_set)) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void handle_birthday_setup_mode(void) {
    ESP_LOGI(TAG, "Entering birthday setup mode from countdown");
    
    // Display transition message
    display_birthday_prompt();
    
    setup_values_t values = {
        .birthday_month = g_birthday_month,
        .birthday_day = g_birthday_day,
        .birthday_hour = g_birthday_hour,
        .birthday_minute = g_birthday_minute,
        .birthday_second = g_birthday_second
    };
    
    setup_state_t current_state = SETUP_BIRTHDAY_MONTH;
    bool setup_complete = false;
    
    // Show initial setup field
    display_setup_value(current_state, values.birthday_month);
    ESP_LOGI(TAG, "Initial birthday setup display - MONTH: %d", values.birthday_month);
    
    while (!setup_complete) {
        // Handle Up button
        if (is_button_pressed(&btn_up)) {
            uint16_t old_value;
            
            switch (current_state) {
                case SETUP_BIRTHDAY_MONTH:
                    old_value = values.birthday_month;
                    values.birthday_month = adjust_value(values.birthday_month, true, current_state, 2024, 0);  // Year not used for birthday
                    ESP_LOGI(TAG, "Birthday Month adjusted: %d -> %d", old_value, values.birthday_month);
                    display_setup_value(current_state, values.birthday_month);
                    break;
                    
                case SETUP_BIRTHDAY_DAY:
                    old_value = values.birthday_day;
                    values.birthday_day = adjust_value(values.birthday_day, true, current_state, 2024, values.birthday_month);
                    ESP_LOGI(TAG, "Birthday Day adjusted: %d -> %d", old_value, values.birthday_day);
                    display_setup_value(current_state, values.birthday_day);
                    break;
               	case SETUP_BIRTHDAY_HOUR:
                    old_value = values.birthday_hour;
                    values.birthday_hour = adjust_value(values.birthday_hour, true, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Hour adjusted: %d -> %d", old_value, values.birthday_hour);
                    display_setup_value(current_state, values.birthday_hour);
                    break;

                case SETUP_BIRTHDAY_MINUTE:
                    old_value = values.birthday_minute;
                    values.birthday_minute = adjust_value(values.birthday_minute, true, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Minute adjusted: %d -> %d", old_value, values.birthday_minute);
                    display_setup_value(current_state, values.birthday_minute);
                    break;

                case SETUP_BIRTHDAY_SECOND:
                    old_value = values.birthday_second;
                    values.birthday_second = adjust_value(values.birthday_second, true, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Second adjusted: %d -> %d", old_value, values.birthday_second);
                    display_setup_value(current_state, values.birthday_second);
                    break;
                    
                default:
                    break;
            }
        }

        // Handle Down button
        if (is_button_pressed(&btn_down)) {
            uint16_t old_value;
            
            switch (current_state) {
                case SETUP_BIRTHDAY_MONTH:
                    old_value = values.birthday_month;
                    values.birthday_month = adjust_value(values.birthday_month, false, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Month adjusted: %d -> %d", old_value, values.birthday_month);
                    display_setup_value(current_state, values.birthday_month);
                    break;
                                    case SETUP_BIRTHDAY_DAY:
                    old_value = values.birthday_day;
                    values.birthday_day = adjust_value(values.birthday_day, false, current_state, 2024, values.birthday_month);
                    ESP_LOGI(TAG, "Birthday Day adjusted: %d -> %d", old_value, values.birthday_day);
                    display_setup_value(current_state, values.birthday_day);
                    break;

                case SETUP_BIRTHDAY_HOUR:
                    old_value = values.birthday_hour;
                    values.birthday_hour = adjust_value(values.birthday_hour, false, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Hour adjusted: %d -> %d", old_value, values.birthday_hour);
                    display_setup_value(current_state, values.birthday_hour);
                    break;

                case SETUP_BIRTHDAY_MINUTE:
                    old_value = values.birthday_minute;
                    values.birthday_minute = adjust_value(values.birthday_minute, false, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Minute adjusted: %d -> %d", old_value, values.birthday_minute);
                    display_setup_value(current_state, values.birthday_minute);
                    break;

                case SETUP_BIRTHDAY_SECOND:
                    old_value = values.birthday_second;
                    values.birthday_second = adjust_value(values.birthday_second, false, current_state, 2024, 0);
                    ESP_LOGI(TAG, "Birthday Second adjusted: %d -> %d", old_value, values.birthday_second);
                    display_setup_value(current_state, values.birthday_second);
                    break;

                default:
                    break;
            }
        }

        // Handle Set button
        if (is_button_pressed(&btn_set)) {
            if (current_state < SETUP_DONE) {
                current_state++;
                if (current_state == SETUP_DONE) {
                    // Save birthday values to global variables
                    g_birthday_month = values.birthday_month;
                    g_birthday_day = values.birthday_day;
                    g_birthday_hour = values.birthday_hour;
                    g_birthday_minute = values.birthday_minute;
                    g_birthday_second = values.birthday_second;
                    
                    ESP_LOGI(TAG, "Birthday setup complete! Final values - Month: %d, Day: %d, Time: %02d:%02d:%02d",
                            g_birthday_month, g_birthday_day, g_birthday_hour, g_birthday_minute, g_birthday_second);
                    setup_complete = true;
                } else {
                    // Show next setup field
                    uint16_t current_value = (current_state == SETUP_BIRTHDAY_MONTH) ? values.birthday_month :
                                           (current_state == SETUP_BIRTHDAY_DAY) ? values.birthday_day :
                                           (current_state == SETUP_BIRTHDAY_HOUR) ? values.birthday_hour :
                                           (current_state == SETUP_BIRTHDAY_MINUTE) ? values.birthday_minute :
                                           values.birthday_second;
                    
                    display_setup_value(current_state, current_value);
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Function to read from RTC
static esp_err_t ds3231_read_time(uint8_t *seconds, uint8_t *minutes, 
                                 uint8_t *hours, uint8_t *date, 
                                 uint8_t *month, uint8_t *year) {
    uint8_t data[7];
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    
    // Set register pointer to 0x00
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, 0x00, true);
    
    // Read registers
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 7, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_0, cmd, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (err == ESP_OK) {
        // Convert BCD to decimal
        *seconds = ((data[0] >> 4) * 10) + (data[0] & 0x0F);
        *minutes = ((data[1] >> 4) * 10) + (data[1] & 0x0F);
        *hours = ((data[2] >> 4) * 10) + (data[2] & 0x0F);
        *date = ((data[4] >> 4) * 10) + (data[4] & 0x0F);
        *month = ((data[5] >> 4) * 10) + (data[5] & 0x0F);
        *year = ((data[6] >> 4) * 10) + (data[6] & 0x0F);
    }
    
    return err;
}

// Function to write to MAX6951
static esp_err_t max6951_write(uint8_t reg, uint8_t data) {
    uint8_t tx_data[2] = {reg, data};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = NULL
    };
    return spi_device_transmit(spi, &t);
}
static esp_err_t max6951_init(void) {
    esp_err_t err;
    
    // Start with no decode for all digits
    err = max6951_write(MAX6951_DECODE_MODE, 0x00);
    if (err != ESP_OK) return err;
    
    // Set intensity (50%)
    err = max6951_write(MAX6951_INTENSITY, 0x08);
    if (err != ESP_OK) return err;
    
    // Enable all digits
    err = max6951_write(MAX6951_SCAN_LIMIT, 0x07);
    if (err != ESP_OK) return err;
    
    // Normal operation
    err = max6951_write(MAX6951_CONFIG, 0x01);
    return err;
}

static esp_err_t max6951_init1(void) {
    esp_err_t err;
    
    // Start with no decode for all digits
    err = max6951_write(MAX6951_DECODE_MODE, 0xFF);
    if (err != ESP_OK) return err;
    
    // Set intensity (50%)
    err = max6951_write(MAX6951_INTENSITY, 0x08);
    if (err != ESP_OK) return err;
    
    // Enable all digits
    err = max6951_write(MAX6951_SCAN_LIMIT, 0x07);
    if (err != ESP_OK) return err;
    
    // Normal operation
    err = max6951_write(MAX6951_CONFIG, 0x01);
    return err;
}
// Function to calculate days until birthday
int calculate_days_until_birthday(int current_date, int current_month, int current_year,
                                  int birthday_date, int birthday_month) {
    struct tm current_time = {0}, birthday_time = {0};
    time_t now, birthday_epoch;

    // Set current time structure
    current_time.tm_mday = current_date;
    current_time.tm_mon = current_month - 1;  // tm_mon is 0-indexed
    current_time.tm_year = current_year - 1900;  // tm_year starts from 1900

    // Set birthday time structure for this year
    birthday_time.tm_mday = birthday_date;
    birthday_time.tm_mon = birthday_month - 1;
    birthday_time.tm_year = current_year - 1900;

    // Get epoch times
    now = mktime(&current_time);
    birthday_epoch = mktime(&birthday_time);

    // If birthday has passed this year, calculate for next year
    if (mktime(&birthday_time) <= mktime(&current_time)) {
        birthday_time.tm_year += 1;  // Next year's birthday
        birthday_epoch = mktime(&birthday_time);
    }

    // Calculate the difference in seconds and convert to days
    double seconds_until_birthday = difftime(birthday_epoch, now);
    int days_until_birthday = (int)(seconds_until_birthday / (60 * 60 * 24));

    // If we hit exactly 0 days (birthday), return 365 to start next year's countdown
    if (days_until_birthday <= 0 && 
        current_date == birthday_date && 
        current_month == birthday_month) {
        return 365;  // Start counting down to next year
    }

    return days_until_birthday;
}

// Buzzer-related functions
void configure_buzzer() {
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 5000,
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = LEDC_CHANNEL,
        .duty = 0,
        .gpio_num = BUZZER_GPIO,
        .speed_mode = LEDC_MODE,
        .timer_sel = LEDC_TIMER
    };
    ledc_channel_config(&ledc_channel);
}

void play_note(int frequency, int duration_ms) {
    if (frequency > 0) {
        ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 512);
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
    }
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    // Stop the sound
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void play_birthday_song() {
    size_t num_notes = sizeof(happy_birthday) / sizeof(happy_birthday[0]);
    for (size_t i = 0; i < num_notes; i++) {
        play_note(happy_birthday[i].frequency, happy_birthday[i].duration);
        vTaskDelay(pdMS_TO_TICKS(50)); // Small pause between notes
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "Initializing birthday countdown system");
    
    // Initialize buzzer
    configure_buzzer();
    ESP_LOGI(TAG, "Buzzer initialized");
    
    // Initialize I2C
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized");
    
    // Initialize SPI and MAX6951
    ESP_ERROR_CHECK(spi_init());
    ESP_ERROR_CHECK(max6951_init());
    ESP_LOGI(TAG, "SPI and MAX6951 initialized");
    
    // Initialize direct LED GPIO
    direct_led_init();
    ESP_LOGI(TAG, "Direct LED initialized");
    
    // Initialize buttons
    init_buttons();
    ESP_LOGI(TAG, "Buttons initialized");
    
    // Startup delay to ensure hardware initialization
    ESP_LOGI(TAG, "Starting initialization delay");
    vTaskDelay(pdMS_TO_TICKS(500));  // 500ms delay
    ESP_LOGI(TAG, "Initialization delay complete");
    
    // Run initial setup
    ESP_LOGI(TAG, "Starting initial time setup");
    handle_setup_mode();
    ESP_ERROR_CHECK(max6951_init1());	// switch back to decode mode
    ESP_LOGI(TAG, "Time setup complete - Beginning countdown operation");
    
    // Variable to track if birthday celebration has occurred
    bool birthday_celebrated = false;
    
    // Variable for decimal point blinking
    bool dp_state = false;
    
    ESP_LOGI(TAG, "Entering main countdown loop");
    ESP_LOGI(TAG, "Birthday target set to: Month: %d, Day: %d, Time: %02d:%02d",
             g_birthday_month, g_birthday_day, g_birthday_hour, g_birthday_minute);
    
    while (1) {
        uint8_t seconds, minutes, hours, date, month, year;
        
// Inside while(1) loop in app_main()
if (ds3231_read_time(&seconds, &minutes, &hours, 
                     &date, &month, &year) == ESP_OK) {
						       // Add long-press detection here
        if (is_button_pressed(&btn_set)) {
                ESP_LOGI(TAG, "Long press detected - Entering birthday setup mode");
                
                // Clear display before entering birthday setup
                clear_all_digits();
                
                // Enter birthday setup mode
                handle_birthday_setup_mode();
                
                // Reinitialize MAX6951 for countdown display
                ESP_ERROR_CHECK(max6951_init1());
                
                // Skip the rest of this loop iteration
                continue;
        }
            
    int days = calculate_days_until_birthday(date, month, year,
                                           g_birthday_day, g_birthday_month);
                                           
            ESP_LOGI(TAG, "Current time: %02d/%02d/%02d %02d:%02d:%02d", 
                     date, month, year, hours, minutes, seconds);
            ESP_LOGI(TAG, "Days until birthday: %d", days);
    
    // Check if it's birthday time BEFORE we modify the days value
    bool is_birthday_time = (
        date == g_birthday_day &&
        month == g_birthday_month &&
        hours == g_birthday_hour &&
        minutes == g_birthday_minute &&
        seconds == 0
    );

    // Trigger birthday celebration only once when countdown reaches zero
    if (is_birthday_time && !birthday_celebrated) {
        ESP_LOGI(TAG, "HAPPY BIRTHDAY! Celebrating at %02d:%02d", 
                 g_birthday_hour, g_birthday_minute);
                    // Update MAX6951 display with countdown values
    max6951_write(MAX6951_DIGIT_0,0);          // Hundreds
    max6951_write(MAX6951_DIGIT_0 + 1, 0); // Tens
    max6951_write(MAX6951_DIGIT_0 + 2, 0);        // Ones
    max6951_write(MAX6951_DIGIT_0 + 3,0);     // Hours tens
    max6951_write(MAX6951_DIGIT_0 + 4,0);     // Hours ones
    max6951_write(MAX6951_DIGIT_0 + 5,0);   // Minutes tens
    max6951_write(MAX6951_DIGIT_0 + 6,0);   // Minutes ones
    max6951_write(MAX6951_DIGIT_0 + 7,0);   // Seconds tens
    
    // Update direct LED with seconds ones digit and blink decimal point
    direct_led_display(0, dp_state);
        play_birthday_song();
        birthday_celebrated = true;
        // After celebration, set days to 365 to start next year's countdown
        days = 365;
    }

    // Calculate countdown components correctly
    int countdown_hours;
    int countdown_minutes;
    int countdown_seconds;
    
    if (seconds == 0 && minutes == 0 && hours == 0) {
        // Start of a new day at midnight - exactly 12 hours until noon
        countdown_seconds = 0;
        countdown_minutes = 0;
        countdown_hours = g_birthday_hour;
    } else {
        // Initial calculations
        countdown_seconds = (seconds == 0) ? 0 : 60 - seconds;
        countdown_minutes = (minutes == 0) ? 0 : 59 - minutes;
        
        // Calculate hours remaining until BIRTHDAY_HOUR
        if (hours <= g_birthday_hour) {
            countdown_hours = g_birthday_hour - hours;
            if (minutes > 0 || seconds > 0) {
                countdown_hours--;
                if (countdown_hours < 0) {
                    countdown_hours = 23;
                    days--;
                }
            }
        } else {
            countdown_hours = (24 - hours) + g_birthday_hour - 1;
            days--;
        }
        
        // Handle rollovers for non-midnight times
        if (seconds > 0 || minutes > 0) {
            countdown_minutes = 59 - minutes;
        }
    }

    // Reset celebration flag at midnight
    if (hours == 0 && minutes == 0 && seconds == 0) {
        birthday_celebrated = false;
    }
    
    // Enhanced logging for countdown
    ESP_LOGI(TAG, "Countdown to Birthday: %03d:%02d:%02d:%02d", 
             days, countdown_hours, countdown_minutes, countdown_seconds);
             
    // Update MAX6951 display with countdown values
    max6951_write(MAX6951_DIGIT_0, days / 100);          // Hundreds
    max6951_write(MAX6951_DIGIT_0 + 1, (days / 10) % 10); // Tens
    max6951_write(MAX6951_DIGIT_0 + 2, days % 10);        // Ones
    max6951_write(MAX6951_DIGIT_0 + 3, countdown_hours / 10);     // Hours tens
    max6951_write(MAX6951_DIGIT_0 + 4, countdown_hours % 10);     // Hours ones
    max6951_write(MAX6951_DIGIT_0 + 5, countdown_minutes / 10);   // Minutes tens
    max6951_write(MAX6951_DIGIT_0 + 6, countdown_minutes % 10);   // Minutes ones
    max6951_write(MAX6951_DIGIT_0 + 7, countdown_seconds / 10);   // Seconds tens
    
    // Update direct LED with seconds ones digit and blink decimal point
    direct_led_display(countdown_seconds % 10, dp_state);
    dp_state = !dp_state;  // Toggle decimal point each second
        } else {
            ESP_LOGE(TAG, "Error reading from RTC");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}