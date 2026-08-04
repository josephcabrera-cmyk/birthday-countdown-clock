#include "buttons.h"

#include "driver/gpio.h"
#include "freertos/task.h"

// Button definitions
#define BTN_UP_PIN      26
#define BTN_DOWN_PIN    27
#define BTN_SET_PIN     25

// Button debounce time in milliseconds
#define DEBOUNCE_TIME   50

button_t btn_up = {BTN_UP_PIN, 0, true};
button_t btn_down = {BTN_DOWN_PIN, 0, true};
button_t btn_set = {BTN_SET_PIN, 0, true};

void init_buttons(void) {
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

bool is_button_pressed(button_t *button) {
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
