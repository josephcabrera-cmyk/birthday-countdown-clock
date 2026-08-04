#include "setup_ui.h"

#include "buttons.h"
#include "display.h"
#include "direct_led.h"
#include "ds3231.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BIRTHDAY_COUNTDOWN";

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

// Date/time limits structure
typedef struct {
    uint16_t min;
    uint16_t max;
} value_limits_t;

uint8_t g_birthday_month;
uint8_t g_birthday_day;
uint8_t g_birthday_hour;
uint8_t g_birthday_minute;
uint8_t g_birthday_second;

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
static uint16_t adjust_value(uint16_t current_value, bool increment, setup_state_t state,
                             uint16_t year, uint8_t month) {
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

// The value belonging to whichever field is currently being edited.
// Extracted verbatim from the three identical copies in handle_setup_mode.
static uint16_t current_field_value(setup_state_t state, const setup_values_t *v) {
    return (state == SETUP_YEAR)             ? v->year :
           (state == SETUP_MONTH)            ? v->month :
           (state == SETUP_DAY)              ? v->day :
           (state == SETUP_HOUR)             ? v->hour :
           (state == SETUP_MINUTE)           ? v->minute :
           (state == SETUP_BIRTHDAY_MONTH)   ? v->birthday_month :
           (state == SETUP_BIRTHDAY_DAY)     ? v->birthday_day :
           (state == SETUP_BIRTHDAY_HOUR)    ? v->birthday_hour :
           (state == SETUP_BIRTHDAY_MINUTE)  ? v->birthday_minute :
           (state == SETUP_BIRTHDAY_SECOND)  ? v->birthday_second :
                                               v->second;
}

// As above, but for the birthday-only re-entry path, which falls back to
// birthday_second rather than second.
static uint16_t birthday_field_value(setup_state_t state, const setup_values_t *v) {
    return (state == SETUP_BIRTHDAY_MONTH)   ? v->birthday_month :
           (state == SETUP_BIRTHDAY_DAY)     ? v->birthday_day :
           (state == SETUP_BIRTHDAY_HOUR)    ? v->birthday_hour :
           (state == SETUP_BIRTHDAY_MINUTE)  ? v->birthday_minute :
                                               v->birthday_second;
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

void handle_setup_mode(void) {
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
            display_setup_value(current_state, current_field_value(current_state, &values));
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
            display_setup_value(current_state, current_field_value(current_state, &values));
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
                    display_setup_value(current_state, current_field_value(current_state, &values));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Exiting setup mode - Starting countdown");
}

void handle_birthday_setup_mode(void) {
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
                    display_setup_value(current_state, birthday_field_value(current_state, &values));
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
