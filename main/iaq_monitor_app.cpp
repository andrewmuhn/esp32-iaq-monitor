#include "iaq_monitor_app.hpp"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

namespace {

    constexpr i2c_port_num_t I2C_PORT = I2C_NUM_0;
    constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_21;
    constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_22;

    constexpr char TAG[] = "IAQ_MONITOR_APP";

    constexpr Pms5003Config PMS_CONFIG = {
        .uart_port = UART_NUM_2,
        .rx_pin = GPIO_NUM_16,
        .tx_pin = GPIO_NUM_17
    };
}

IAQMonitorApp::IAQMonitorApp() : pms5003_(PMS_CONFIG) {}

esp_err_t IAQMonitorApp::initialize_i2c_bus() {
    i2c_master_bus_config_t bus_config{};

    bus_config.i2c_port = I2C_PORT;
    bus_config.sda_io_num = I2C_SDA_PIN;
    bus_config.scl_io_num = I2C_SCL_PIN;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = false;

    return i2c_new_master_bus(&bus_config, &i2c_bus_);
}

esp_err_t IAQMonitorApp::initialize() {
    esp_err_t error = pms5003_.initialize();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize PMS5003: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "PMS5003 initialized");

    error = initialize_i2c_bus();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize I2C bus: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(
        TAG,
        "I2C bus initialized: SDA GPIO%d, SCL GPIO%d",
        I2C_SDA_PIN,
        I2C_SCL_PIN
    );

    error = scd41_.initialize(i2c_bus_);

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize SCD41: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "SCD41 registered on I2C bus");

    error = scd41_.start_periodic_measurement();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to start SCD41 periodic measurement: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "SCD41 periodic measurement started");

    return ESP_OK;
}

void IAQMonitorApp::run() {
    ParticulateReading reading{};

    while (true) {
        bool scd41_ready = false;

        const esp_err_t scd41_error =
            scd41_.is_data_ready(scd41_ready);

        if (scd41_error != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Failed to check SCD41 readiness: %s",
                esp_err_to_name(scd41_error)
            );
        } else if (scd41_ready) {
            Scd41Reading reading{};

            const esp_err_t read_error =
                scd41_.read_measurement(reading);

            if (read_error != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "Failed to read SCD41 measurement: %s",
                    esp_err_to_name(read_error)
                );
            } else {
                ESP_LOGI(
                    TAG,
                    "SCD41 measurement: CO2=%u ppm | temperature=%.2f °C | humidity=%.1f%% RH",
                    reading.co2_ppm,
                    reading.temperature_c,
                    reading.relative_humidity_percent
                );
            }
        }

        const esp_err_t error = pms5003_.read(reading);

        if (error == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Failed to read PMS5003 frame");
            continue;
        }

        if (
            error == ESP_ERR_INVALID_RESPONSE ||
            error == ESP_ERR_INVALID_CRC
        ) {
            ESP_LOGW(TAG, "Invalid PMS5003 frame");
            continue;
        }

        if (error != ESP_OK) {
            ESP_LOGE(
                TAG,
                "PMS5003 read error: %s",
                esp_err_to_name(error)
            );
            continue;
        }

        ESP_LOGI(
            TAG,
            "PM1.0: %u ug/m3 | PM2.5: %u ug/m3 | PM10: %u ug/m3",
            reading.pm1_0_ug_m3,
            reading.pm2_5_ug_m3,
            reading.pm10_ug_m3
        );
    }
}