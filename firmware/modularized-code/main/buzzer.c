#include "buzzer.h"

#include <stddef.h>
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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

// Melody structure
typedef struct {
    int frequency;
    int duration;
} Note;

// Happy Birthday Melody (simplified)
static const Note happy_birthday[] = {
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

void configure_buzzer(void) {
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

void play_birthday_song(void) {
    size_t num_notes = sizeof(happy_birthday) / sizeof(happy_birthday[0]);
    for (size_t i = 0; i < num_notes; i++) {
        play_note(happy_birthday[i].frequency, happy_birthday[i].duration);
        vTaskDelay(pdMS_TO_TICKS(50)); // Small pause between notes
    }
}
