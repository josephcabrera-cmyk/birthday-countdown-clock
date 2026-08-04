#include "ds3231.h"

#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"

// Pin definitions from schematic
#define I2C_MASTER_SCL_IO           22    // SCL pin
#define I2C_MASTER_SDA_IO           21    // SDA pin
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

esp_err_t i2c_master_init(void) {
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

esp_err_t ds3231_set_time(uint8_t seconds, uint8_t minutes, uint8_t hours,
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

esp_err_t ds3231_read_time(uint8_t *seconds, uint8_t *minutes,
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
