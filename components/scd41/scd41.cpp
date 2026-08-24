#include "scd41.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstdint>


namespace {
    constexpr uint16_t DEVICE_ADDRESS = 0x62;
    constexpr uint32_t SCL_SPEED_HZ = 100000;
    constexpr int PROBE_TIMEOUT_MS = 100;
    constexpr int TRANSACTION_TIMEOUT_MS = 100;
    constexpr int STOP_COMPLETION_TIME_MS = 500;

    constexpr uint8_t START_PERIODIC_MEASUREMENT_COMMAND[] = {
        0x21,
        0xB1
    };

    constexpr uint8_t STOP_PERIODIC_MEASUREMENT_COMMAND[] = {
        0x3F,
        0x86
    };

    constexpr uint8_t GET_DATA_READY_COMMAND[] = {
        0xE4,
        0xB8
    };

    constexpr std::size_t DATA_READY_RESPONSE_SIZE = 3;

    constexpr uint8_t READ_MEASUREMENT_COMMAND[] = {
        0xEC,
        0x05
    };

    constexpr std::size_t MEASUREMENT_RESPONSE_SIZE = 9;
    constexpr std::size_t BYTES_PER_RESPONSE_WORD = 3;
}

uint8_t Scd41::calculate_crc(
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


uint16_t Scd41::read_uint16_be(const uint8_t* data) {
    return
        (static_cast<uint16_t>(data[0]) << 8) |
        data[1];
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

esp_err_t Scd41::start_periodic_measurement() {
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = i2c_master_transmit(
        device_,
        START_PERIODIC_MEASUREMENT_COMMAND,
        sizeof(START_PERIODIC_MEASUREMENT_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );

    if (error == ESP_OK) {
        return ESP_OK;
    }

    if (error != ESP_ERR_INVALID_RESPONSE) {
        return error;
    }

    error = i2c_master_transmit(
        device_,
        STOP_PERIODIC_MEASUREMENT_COMMAND,
        sizeof(STOP_PERIODIC_MEASUREMENT_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(STOP_COMPLETION_TIME_MS));

    return i2c_master_transmit(
        device_,
        START_PERIODIC_MEASUREMENT_COMMAND,
        sizeof(START_PERIODIC_MEASUREMENT_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );
}

esp_err_t Scd41::is_data_ready(bool& ready) {
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = i2c_master_transmit(
        device_,
        GET_DATA_READY_COMMAND,
        sizeof(GET_DATA_READY_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(1) + 1);

    uint8_t response[DATA_READY_RESPONSE_SIZE]{};

    error = i2c_master_receive(
        device_,
        response,
        sizeof(response),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    const uint8_t calculated_crc = calculate_crc(response, 2);

    if (calculated_crc != response[2]) {
        return ESP_ERR_INVALID_CRC;
    }

    const uint16_t status =
        (static_cast<uint16_t>(response[0]) << 8) |
        response[1];

    ready = (status & 0x07FF) != 0;

    return ESP_OK;
}

esp_err_t Scd41::read_raw_measurement(
    Scd41::RawReading& reading
) {
    if (device_ == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t error = i2c_master_transmit(
        device_,
        READ_MEASUREMENT_COMMAND,
        sizeof(READ_MEASUREMENT_COMMAND),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(1) + 1);

    uint8_t response[MEASUREMENT_RESPONSE_SIZE]{};

    error = i2c_master_receive(
        device_,
        response,
        sizeof(response),
        TRANSACTION_TIMEOUT_MS
    );

    if (error != ESP_OK) {
        return error;
    }

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

    reading.co2_ppm = read_uint16_be(&response[0]);
    reading.temperature_ticks = read_uint16_be(&response[3]);
    reading.humidity_ticks = read_uint16_be(&response[6]);

    return ESP_OK;
}

esp_err_t Scd41::read_measurement(Scd41Reading& reading) {
    RawReading raw{};

    const esp_err_t error = read_raw_measurement(raw);

    if (error != ESP_OK) {
        return error;
    }

    constexpr float RAW_MAX = 65535.0f;

    reading.co2_ppm = raw.co2_ppm;

    reading.temperature_c =
        -45.0f +
        175.0f *
        static_cast<float>(raw.temperature_ticks) /
        RAW_MAX;

    reading.relative_humidity_percent =
        100.0f *
        static_cast<float>(raw.humidity_ticks) /
        RAW_MAX;

    return ESP_OK;
}