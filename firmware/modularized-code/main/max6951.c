#include "max6951.h"

#include "driver/spi_master.h"

// Pin definitions from schematic
#define SPI_MOSI_PIN               23     // DIN pin
#define SPI_SCLK_PIN               18     // CLK pin
#define SPI_CS_PIN                 5      // CS pin

// SPI handle
static spi_device_handle_t spi;

esp_err_t spi_init(void) {
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

esp_err_t max6951_write(uint8_t reg, uint8_t data) {
    uint8_t tx_data[2] = {reg, data};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = NULL
    };
    return spi_device_transmit(spi, &t);
}

esp_err_t max6951_init(void) {
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

esp_err_t max6951_init1(void) {
    esp_err_t err;

    // Hex decode for all digits
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

esp_err_t max6951_set_decode_mode(uint8_t decode_mask) {
    return max6951_write(MAX6951_DECODE_MODE, decode_mask);
}
