#include "scd41.hpp"

#include <cstdint>

namespace {
    constexpr uint16_t DEVICE_ADDRESS = 0x62;
    constexpr uint32_t SCL_SPEED_HZ = 100000;
    constexpr int PROBE_TIMEOUT_MS = 100;
}

esp_err_t Scd41::initialize(i2c_master_bus_handle_t bus) {
    if (bus == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    if (device_ != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = i2c_master_probe(
        bus,
        DEVICE_ADDRESS,
        PROBE_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    i2c_device_config_t device_config{};

    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    device_config.device_address = DEVICE_ADDRESS;
    device_config.scl_speed_hz = SCL_SPEED_HZ;

    return i2c_master_bus_add_device(
        bus,
        &device_config,
        &device_
    );
}