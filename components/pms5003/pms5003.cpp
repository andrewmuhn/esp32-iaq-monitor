#include "pms5003.hpp"
#include "freertos/FreeRTOS.h"

Pms5003::Pms5003(const Pms5003Config& config) : config_(config) {}

esp_err_t Pms5003::synchronize(uint8_t* frame) {
    uint8_t byte = 0;

    while (true) {
        int bytes_read = uart_read_bytes(
            config_.uart_port,
            &byte,
            1,
            pdMS_TO_TICKS(2000)
        );

        if (bytes_read == 0) {
            return ESP_ERR_TIMEOUT;
        }

        if (bytes_read < 0) {
            return ESP_FAIL;
        }

        if (byte != 0x42) {
            continue;
        }

        frame[0] = byte;

        bytes_read = uart_read_bytes(
            config_.uart_port,
            &byte,
            1,
            pdMS_TO_TICKS(100)
        );

        if (bytes_read == 0) {
            return ESP_ERR_TIMEOUT;
        }

        if (bytes_read < 0) {
            return ESP_FAIL;
        }

        if (byte != 0x4D) {
            continue;
        }

        frame[1] = byte;
        return ESP_OK;
    }
}

esp_err_t Pms5003::read_frame(uint8_t* frame, std::size_t length) {
    if (frame == nullptr || length != FRAME_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t error = synchronize(frame);

    if (error != ESP_OK) {
        return error;
    }

    std::size_t offset = 2;
    std::size_t remaining = FRAME_SIZE - offset;

    while (remaining > 0) {
        const int bytes_read = uart_read_bytes(
            config_.uart_port,
            frame + offset,
            remaining,
            pdMS_TO_TICKS(100)
        );

        if (bytes_read == 0) {
            return ESP_ERR_TIMEOUT;
        }

        if (bytes_read < 0) {
            return ESP_FAIL;
        }

        offset += static_cast<std::size_t>(bytes_read);
        remaining -= static_cast<std::size_t>(bytes_read);
    }

    return ESP_OK;
}

bool Pms5003::checksum_is_valid(
    const uint8_t* frame,
    std::size_t length
) const {
    if (frame == nullptr || length != FRAME_SIZE) {
        return false;
    }

    uint16_t calculated_checksum = 0;

    for (std::size_t i = 0; i < 30; ++i) {
        calculated_checksum += frame[i];
    }

    const uint16_t expected_checksum =
        (static_cast<uint16_t>(frame[30]) << 8) |
        frame[31];

    return calculated_checksum == expected_checksum;
}

esp_err_t Pms5003::read(ParticulateReading& result) {
    uint8_t frame[FRAME_SIZE]{};

    esp_err_t error = read_frame(frame, sizeof(frame));

    if (error != ESP_OK) {
        return error;
    }

    if (frame[0] != 0x42 || frame[1] != 0x4D) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    const uint16_t frame_length = read_uint16_be(&frame[2]);

    if (frame_length != 28) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!checksum_is_valid(frame, sizeof(frame))) {
        return ESP_ERR_INVALID_CRC;
    }

    result.pm1_0_ug_m3 = read_uint16_be(&frame[10]);
    result.pm2_5_ug_m3 = read_uint16_be(&frame[12]);
    result.pm10_ug_m3 = read_uint16_be(&frame[14]);

    return ESP_OK;
}

uint16_t Pms5003::read_uint16_be(const uint8_t* data)
{
    return
        (static_cast<uint16_t>(data[0]) << 8) |
        data[1];
}

esp_err_t Pms5003::initialize() {
    const uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_DEFAULT,
        .flags = {}
    };

    esp_err_t error = uart_driver_install(
        config_.uart_port,
        1024,
        0,
        0,
        nullptr,
        0
    );

    if (error != ESP_OK) {
        return error;
    }

    error = uart_param_config(config_.uart_port, &uart_config);

    if (error != ESP_OK) {
        return error;
    }

    return uart_set_pin(
        config_.uart_port,
        config_.tx_pin,
        config_.rx_pin,
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    );

}