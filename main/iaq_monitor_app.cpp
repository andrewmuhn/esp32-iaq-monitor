#include "iaq_monitor_app.hpp"

#include "driver/uart.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "epaper_canvas.hpp"
#include "iaq_assessment.hpp"

namespace {

    constexpr i2c_port_num_t I2C_PORT = I2C_NUM_0;
    constexpr gpio_num_t I2C_SDA_PIN = GPIO_NUM_21;
    constexpr gpio_num_t I2C_SCL_PIN = GPIO_NUM_22;

    constexpr char TAG[] = "IAQ_MONITOR_APP";

    // This display supports full-screen refreshes only. A refresh takes
    // roughly 21 seconds, so the display should not follow every sensor read.
    constexpr int64_t DISPLAY_REFRESH_INTERVAL_US =
        5LL * 60LL * 1000000LL;

    constexpr Pms5003Config PMS_CONFIG = {
        .uart_port = UART_NUM_2,
        .rx_pin = GPIO_NUM_16,
        .tx_pin = GPIO_NUM_17
    };

    constexpr EpaperDisplayConfig EPAPER_CONFIG = {
        .spi_host = SPI3_HOST,
        .mosi_pin = GPIO_NUM_23,
        .clock_pin = GPIO_NUM_18,
        .chip_select_pin = GPIO_NUM_13,
        .data_command_pin = GPIO_NUM_27,
        .reset_pin = GPIO_NUM_26,
        .busy_pin = GPIO_NUM_25,
        .power_pin = GPIO_NUM_33
    };

}

IAQMonitorApp::IAQMonitorApp() :
    pms5003_(PMS_CONFIG),
    epaper_display_(EPAPER_CONFIG) {}

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

    error = epaper_display_.initialize();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize e-paper interface: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "E-paper interface initialized");

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

    error = sht41_.initialize(i2c_bus_);

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to initialize SHT41: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "SHT41 registered on I2C bus");

    error = scd41_.initialize(i2c_bus_);

    if (error != ESP_OK) {
        ESP_LOGW(
            TAG,
            "SCD41 unavailable; continuing with SHT41 only: %s",
            esp_err_to_name(error)
        );
    } else {
        ESP_LOGI(TAG, "SCD41 registered on I2C bus");

        error = scd41_.start_periodic_measurement();

        if (error != ESP_OK) {
            ESP_LOGW(
                TAG,
                "SCD41 periodic measurement unavailable: %s",
                esp_err_to_name(error)
            );
        } else {
            scd41_available_ = true;
            ESP_LOGI(TAG, "SCD41 periodic measurement started");
        }
    }

    return ESP_OK;
}

esp_err_t IAQMonitorApp::display_current_readings(
    const AirQualityAssessment& assessment
) {
    EpaperCanvas canvas;

    dashboard_.render(
        canvas,
        latest_readings_,
        assessment
    );

    return epaper_display_.display(
        canvas.data(),
        canvas.size()
    );
}

void IAQMonitorApp::update_display_if_needed() {
    const bool have_complete_live_reading =
        latest_readings_.co2_valid &&
        latest_readings_.pm2_5_valid &&
        latest_readings_.pm10_valid &&
        latest_readings_.temperature_valid &&
        latest_readings_.humidity_valid;

    if (!have_complete_live_reading) {
        return;
    }

    const AirQualityAssessment assessment =
        assess_air_quality(latest_readings_);

    const bool first_refresh =
        !first_live_dashboard_rendered_;

    const int64_t now_us = esp_timer_get_time();

    const bool scheduled_refresh =
        first_live_dashboard_rendered_ &&
        now_us - last_display_refresh_us_ >=
            DISPLAY_REFRESH_INTERVAL_US;

    const bool overall_score_changed =
        first_live_dashboard_rendered_ &&
        assessment.overall_score != last_displayed_overall_score_;

    if (
        !first_refresh &&
        !scheduled_refresh &&
        !overall_score_changed
    ) {
        return;
    }

    const esp_err_t display_error =
        display_current_readings(assessment);

    if (display_error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to refresh dashboard: %s",
            esp_err_to_name(display_error)
        );
        return;
    }

    first_live_dashboard_rendered_ = true;
    last_displayed_overall_score_ = assessment.overall_score;
    last_display_refresh_us_ = esp_timer_get_time();

    ESP_LOGI(
        TAG,
        "Dashboard refreshed: %s",
        first_refresh
            ? "initial live data"
            : overall_score_changed
                ? "overall score change"
                : "five-minute interval"
    );

    if (scd41_available_) {
        const esp_err_t restart_error =
            scd41_.start_periodic_measurement();

        if (restart_error != ESP_OK) {
            ESP_LOGW(
                TAG,
                "SCD41 periodic measurement restart failed: %s",
                esp_err_to_name(restart_error)
            );
        }
    }
}

void IAQMonitorApp::run() {
    ParticulateReading pms_reading{};

    while (true) {
        if (scd41_available_) {
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
                Scd41Reading scd41_reading{};

                const esp_err_t read_error =
                    scd41_.read_measurement(scd41_reading);

                if (read_error != ESP_OK) {
                    ESP_LOGW(
                        TAG,
                        "Failed to read SCD41 measurement: %s",
                        esp_err_to_name(read_error)
                    );
                } else {
                    ESP_LOGI(
                        TAG,
                        "SCD41 measurement: CO2=%u ppm",
                        scd41_reading.co2_ppm
                    );

                    latest_readings_.co2_ppm =
                        scd41_reading.co2_ppm;

                    latest_readings_.co2_valid = true;
                }
            }
        }

        Sht41Reading sht41_reading{};

        const esp_err_t sht41_error =
            sht41_.read_measurement(sht41_reading);

        if (sht41_error != ESP_OK) {
            ESP_LOGW(
                TAG,
                "Failed to read SHT41 measurement: %s",
                esp_err_to_name(sht41_error)
            );
        } else {
            ESP_LOGI(
                TAG,
                "SHT41 measurement: temperature=%.2f °F | humidity=%.1f%% RH",
                sht41_reading.temperature_c * 9.0f / 5.0f + 32.0f,
                sht41_reading.relative_humidity_percent
            );

            latest_readings_.temperature_c =
                sht41_reading.temperature_c;

            latest_readings_.humidity_percent =
                sht41_reading.relative_humidity_percent;

            latest_readings_.temperature_valid = true;
            latest_readings_.humidity_valid = true;
        }

        const esp_err_t error = pms5003_.read(pms_reading);

        if (error == ESP_ERR_TIMEOUT) {
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
            pms_reading.pm1_0_ug_m3,
            pms_reading.pm2_5_ug_m3,
            pms_reading.pm10_ug_m3
        );

        latest_readings_.pm1_0_ug_m3 =
            pms_reading.pm1_0_ug_m3;

        latest_readings_.pm2_5_ug_m3 =
            pms_reading.pm2_5_ug_m3;

        latest_readings_.pm10_ug_m3 =
            pms_reading.pm10_ug_m3;

        latest_readings_.pm1_0_valid = true;
        latest_readings_.pm2_5_valid = true;
        latest_readings_.pm10_valid = true;
        update_display_if_needed();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
