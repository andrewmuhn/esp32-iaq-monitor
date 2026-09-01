#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

struct EpaperDisplayConfig {
    spi_host_device_t spi_host;
    gpio_num_t mosi_pin;
    gpio_num_t clock_pin;
    gpio_num_t chip_select_pin;
    gpio_num_t data_command_pin;
    gpio_num_t reset_pin;
    gpio_num_t busy_pin;
    gpio_num_t power_pin;
};

class EpaperDisplay {
    public:
        explicit EpaperDisplay(const EpaperDisplayConfig& config);

        esp_err_t initialize();
        esp_err_t show_test_pattern();
        
        esp_err_t display(
            const uint8_t* buffer,
            size_t size
        );

    private:
        esp_err_t initialize_gpio();
        esp_err_t initialize_spi();
        esp_err_t hardware_reset();
        esp_err_t wait_until_ready(uint32_t timeout_ms);

        esp_err_t initialize_controller();

        esp_err_t transmit(
            const uint8_t* data,
            size_t size
        );

        esp_err_t send_command(uint8_t command);

        esp_err_t send_data(
            const uint8_t* data,
            size_t size
        );

        esp_err_t write_register(
            uint8_t command,
            std::initializer_list<uint8_t> data
        );

        esp_err_t refresh();

        EpaperDisplayConfig config_;
        spi_device_handle_t spi_device_ = nullptr;
        bool initialized_ = false;
};