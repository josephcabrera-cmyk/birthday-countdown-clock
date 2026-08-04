#pragma once

#include <stdint.h>
#include "esp_err.h"

// MAX6951 command registers
#define MAX6951_DECODE_MODE        0x01
#define MAX6951_INTENSITY          0x02
#define MAX6951_SCAN_LIMIT         0x03
#define MAX6951_CONFIG             0x04
#define MAX6951_DIGIT_0            0x20
#define MAX6951_SHUTDOWN           0x00

#define DIGIT_WRITE_DELAY_MS 1  // 1ms delay between digit writes

// Brings up the SPI bus and attaches the MAX6951 as a device.
esp_err_t spi_init(void);

// Writes one register/data pair over SPI.
esp_err_t max6951_write(uint8_t reg, uint8_t data);

// No-decode configuration: digit registers are raw segment patterns,
// used for spelling out the setup prompts.
esp_err_t max6951_init(void);

// Hex-decode configuration: digit registers are numeric values,
// used for the countdown itself.
esp_err_t max6951_init1(void);

// Per-digit decode select; one bit per digit.
esp_err_t max6951_set_decode_mode(uint8_t decode_mask);
