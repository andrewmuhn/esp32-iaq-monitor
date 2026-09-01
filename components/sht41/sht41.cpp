#include "sht41.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>
#include "esp_log.h"


namespace {
    constexpr char TAG[] = "SHT41";
    constexpr uint16_t DEVICE_ADDRESS = 0x44;
    constexpr uint32_t SCL_SPEED_HZ = 100000;
    constexpr int PROBE_TIMEOUT_MS = 100;
    constexpr int TRANSACTION_TIMEOUT_MS = 10;
    constexpr int MEASUREMENT_DURATION_MS = 20;
    constexpr uint8_t SOFT_RESET_COMMAND = 0x94;
    constexpr int SOFT_RESET_TIME_MS = 2;

    constexpr uint8_t READ_MEASUREMENT_COMMAND = 0xE0;
    constexpr std::size_t MEASUREMENT_RESPONSE_SIZE = 6;
    constexpr std::size_t BYTES_PER_RESPONSE_WORD = 3;
}

uint8_t Sht41::calculate_crc(
    const uint8_t* data,
    std::size_t length
) {
    uint8_t crc = 0xFF;

    for (std::size_t i = 0; i < length; ++i) {
        crc ^= data[i];

        for (uint8_t bit = 0; bit < 8; ++bit) {
            if ((crc & 0x80) != 0) {
                crc = static_cast<uint8_t>((crc << 1) ^ 0x31);
            } else {
                crc = static_cast<uint8_t>(crc << 1);
            }
        }
    }

    return crc;
}


uint16_t Sht41::read_uint16_be(const uint8_t* data) {
    return
        (static_cast<uint16_t>(data[0]) << 8) |
        data[1];
}

esp_err_t Sht41::initialize(i2c_master_bus_handle_t bus) {
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

    error = i2c_master_bus_add_device(
        bus,
        &device_config,
        &device_
    );

    if (error != ESP_OK) {
        return error;
    }

    error = i2c_master_transmit(
        device_,
        &SOFT_RESET_COMMAND,
        sizeof(SOFT_RESET_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(SOFT_RESET_TIME_MS) + 1);

    ESP_LOGI(TAG, "Soft reset completed");

    return ESP_OK;
}

esp_err_t Sht41::read_raw_measurement(
    Sht41::RawReading& reading
) {
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t cmd = READ_MEASUREMENT_COMMAND;

    ESP_LOGI(
        TAG,
        "Sending command: 0x%02X",
        cmd
    );

    esp_err_t error = i2c_master_transmit(
        device_,
        &cmd,
        sizeof(cmd),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Measurement command failed: %s",
            esp_err_to_name(error)
        );
            return error;
        }

    // vTaskDelay(pdMS_TO_TICKS(MEASUREMENT_DURATION_MS) + 1);
    esp_rom_delay_us(40000);

    uint8_t response[MEASUREMENT_RESPONSE_SIZE]{};

    error = i2c_master_receive(
        device_,
        response,
        sizeof(response),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        ESP_LOGW(
            TAG,
            "Measurement read failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(
        TAG,
        "Response: %02X %02X %02X %02X %02X %02X",
        response[0],
        response[1],
        response[2],
        response[3],
        response[4],
        response[5]
    );

    for (std::size_t offset = 0;
         offset < sizeof(response);
         offset += BYTES_PER_RESPONSE_WORD) {

        const uint8_t calculated_crc =
            calculate_crc(&response[offset], 2);

        const uint8_t received_crc = response[offset + 2];

        if (calculated_crc != received_crc) {
            return ESP_ERR_INVALID_CRC;
        }
    }

    reading.temperature_ticks = read_uint16_be(&response[0]);
    reading.humidity_ticks = read_uint16_be(&response[3]);

    return ESP_OK;
}

esp_err_t Sht41::read_measurement(Sht41Reading& reading) {
    RawReading raw;

    const esp_err_t error = read_raw_measurement(raw);

    if (error != ESP_OK) {
        return error;
    }

    constexpr float RAW_MAX = 65535.0f;

    reading.temperature_c =
        -45.0f +
        175.0f *
        static_cast<float>(raw.temperature_ticks) /
        RAW_MAX;

    reading.relative_humidity_percent =
        -6.0f +
        125.0f *
        static_cast<float>(raw.humidity_ticks) /
        RAW_MAX;

    return ESP_OK;
}