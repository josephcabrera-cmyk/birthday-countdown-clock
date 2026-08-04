#include <stdio.h>
#include <stdbool.h>

#include "buttons.h"
#include "buzzer.h"
#include "countdown.h"
#include "direct_led.h"
#include "display.h"
#include "ds3231.h"
#include "max6951.h"
#include "setup_ui.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "BIRTHDAY_COUNTDOWN";

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
    ESP_ERROR_CHECK(max6951_init1());   // switch back to decode mode
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

        if (ds3231_read_time(&seconds, &minutes, &hours,
                             &date, &month, &year) == ESP_OK) {
            // Re-enter birthday setup on a SET press
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

                // Zero the display for the celebration
                max6951_write(MAX6951_DIGIT_0,     0);  // Hundreds
                max6951_write(MAX6951_DIGIT_0 + 1, 0);  // Tens
                max6951_write(MAX6951_DIGIT_0 + 2, 0);  // Ones
                max6951_write(MAX6951_DIGIT_0 + 3, 0);  // Hours tens
                max6951_write(MAX6951_DIGIT_0 + 4, 0);  // Hours ones
                max6951_write(MAX6951_DIGIT_0 + 5, 0);  // Minutes tens
                max6951_write(MAX6951_DIGIT_0 + 6, 0);  // Minutes ones
                max6951_write(MAX6951_DIGIT_0 + 7, 0);  // Seconds tens

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
            max6951_write(MAX6951_DIGIT_0,     days / 100);              // Hundreds
            max6951_write(MAX6951_DIGIT_0 + 1, (days / 10) % 10);        // Tens
            max6951_write(MAX6951_DIGIT_0 + 2, days % 10);               // Ones
            max6951_write(MAX6951_DIGIT_0 + 3, countdown_hours / 10);    // Hours tens
            max6951_write(MAX6951_DIGIT_0 + 4, countdown_hours % 10);    // Hours ones
            max6951_write(MAX6951_DIGIT_0 + 5, countdown_minutes / 10);  // Minutes tens
            max6951_write(MAX6951_DIGIT_0 + 6, countdown_minutes % 10);  // Minutes ones
            max6951_write(MAX6951_DIGIT_0 + 7, countdown_seconds / 10);  // Seconds tens

            // Update direct LED with seconds ones digit and blink decimal point
            direct_led_display(countdown_seconds % 10, dp_state);
            dp_state = !dp_state;  // Toggle decimal point each second
        } else {
            ESP_LOGE(TAG, "Error reading from RTC");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));  // Update every second
    }
}
