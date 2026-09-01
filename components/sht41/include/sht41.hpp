#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include <cstddef>
#include <cstdint>

struct Sht41Reading {
    float temperature_c;
    float relative_humidity_percent;
};

class Sht41 {
public:
    esp_err_t initialize(i2c_master_bus_handle_t bus);
    esp_err_t read_measurement(Sht41Reading& reading);

private:

    struct RawReading {
        uint16_t temperature_ticks;
        uint16_t humidity_ticks;
    };


    esp_err_t read_raw_measurement(RawReading& reading);

    static uint8_t calculate_crc(
        const uint8_t* data,
        std::size_t length
    );


    static uint16_t read_uint16_be(const uint8_t* data);

    i2c_master_dev_handle_t device_ = nullptr;
};