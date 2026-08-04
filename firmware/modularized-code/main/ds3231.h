#pragma once

#include <stdint.h>
#include "esp_err.h"

// DS3231 registers
#define DS3231_ADDR                0x68
#define DS3231_SECONDS_REG         0x00
#define DS3231_MINUTES_REG         0x01
#define DS3231_HOURS_REG           0x02
#define DS3231_DAY_REG             0x03
#define DS3231_DATE_REG            0x04
#define DS3231_MONTH_REG           0x05
#define DS3231_YEAR_REG            0x06

// Brings up the I2C bus the RTC sits on.
esp_err_t i2c_master_init(void);

// Values are plain decimal; BCD conversion happens inside.
esp_err_t ds3231_set_time(uint8_t seconds, uint8_t minutes, uint8_t hours,
                          uint8_t date, uint8_t month, uint8_t year);

esp_err_t ds3231_read_time(uint8_t *seconds, uint8_t *minutes, uint8_t *hours,
                           uint8_t *date, uint8_t *month, uint8_t *year);
