#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"

struct Pms5003Config {
    uart_port_t uart_port;
    gpio_num_t rx_pin;
    gpio_num_t tx_pin;
};

struct ParticulateReading {
    uint16_t pm1_0_ug_m3;
    uint16_t pm2_5_ug_m3;
    uint16_t pm10_ug_m3;
};

class Pms5003 {
public:
    explicit Pms5003(const Pms5003Config& config);

    esp_err_t initialize();
    esp_err_t read(ParticulateReading& result);

private:
    static constexpr std::size_t FRAME_SIZE = 32;

    Pms5003Config config_;

    esp_err_t synchronize(uint8_t* frame);
    esp_err_t read_frame(uint8_t* frame, std::size_t length);
    bool checksum_is_valid(
        const uint8_t* frame,
        std::size_t length
    ) const;

    static uint16_t read_uint16_be(const uint8_t* data);
};