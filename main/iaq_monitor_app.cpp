#include "iaq_monitor_app.hpp"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "epaper_canvas.hpp"
#include "font_5x7.hpp"

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

    constexpr int FRAME_X = 0;
    constexpr int FRAME_Y = 0;
    constexpr int FRAME_WIDTH = EpaperCanvas::WIDTH;
    constexpr int FRAME_HEIGHT = EpaperCanvas::HEIGHT;

    constexpr int HEADER_BOTTOM = 31;
    constexpr int MAIN_BOTTOM = 104;
    constexpr int MAIN_DIVIDER_X = 148;
    constexpr int MAIN_TEMP_DIVIDER_X = 222;
    constexpr int TILE_TOP = MAIN_BOTTOM + 2;
    constexpr int TILE_BOTTOM = FRAME_Y + FRAME_HEIGHT - 3;
    constexpr int TILE_VALUE_Y = 123;
            constexpr int TILE_LEFT[] = {
        2, 61, 120, 179, 238
    };

    constexpr int TILE_RIGHT[] = {
        58, 117, 176, 235, 293
    };

    constexpr int TILE_DIVIDER_X[] = {
        59, 118, 177, 236
    };

    void draw_outline(
        EpaperCanvas& canvas,
        int x,
        int y,
        int width,
        int height,
        EpaperColor color
    ) {
        constexpr int LINE_WIDTH = 2;

        canvas.fill_rectangle(x, y, width, LINE_WIDTH, color);
        canvas.fill_rectangle(
            x,
            y + height - LINE_WIDTH,
            width,
            LINE_WIDTH,
            color
        );
        canvas.fill_rectangle(x, y, LINE_WIDTH, height, color);
        canvas.fill_rectangle(
            x + width - LINE_WIDTH,
            y,
            LINE_WIDTH,
            height,
            color
        );
    }

    void draw_outer_frame(EpaperCanvas& canvas) {
        // A filled outer rectangle plus a smaller white rectangle gives us
        // a clean two-pixel frame while keeping the interior white.
        canvas.fill_rectangle(
            FRAME_X,
            FRAME_Y,
            FRAME_WIDTH,
            FRAME_HEIGHT,
            EpaperColor::Black
        );

        canvas.fill_rectangle(
            FRAME_X + 2,
            FRAME_Y + 2,
            FRAME_WIDTH - 4,
            FRAME_HEIGHT - 4,
            EpaperColor::White
        );
    }

    int text_width(const char* text, int scale) {
        if (text == nullptr || scale <= 0) {
            return 0;
        }

        int character_count = 0;

        while (text[character_count] != '\0') {
            ++character_count;
        }

        if (character_count == 0) {
            return 0;
        }

        // Six columns per character: five glyph columns plus one blank column.
        // Remove the final blank column from the visual width.
        return (
            character_count * (Font5x7::WIDTH + 1) * scale
        ) - scale;
    }

    void draw_centered_text(
        EpaperCanvas& canvas,
        int left,
        int right,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        const int width = text_width(text, scale);
        const int x = left + ((right - left + 1) - width) / 2;

        canvas.draw_text(
            x,
            y,
            text,
            color,
            scale
        );
    }
    void draw_left_aligned_text(
        EpaperCanvas& canvas,
        int left,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        canvas.draw_text(
            left,
            y,
            text,
            color,
            scale
        );
    }

    void draw_right_aligned_text(
        EpaperCanvas& canvas,
        int right,
        int y,
        const char* text,
        EpaperColor color,
        int scale
    ) {
        const int x =
            right - text_width(text, scale) + 1;

        canvas.draw_text(
            x,
            y,
            text,
            color,
            scale
        );
    }

    void fill_metric_box(
        EpaperCanvas& canvas,
        int left,
        int top,
        int right,
        int bottom,
        EpaperColor background
    ) {
        canvas.fill_rectangle(
            left,
            top,
            right - left + 1,
            bottom - top + 1,
            background
        );
    }

    EpaperColor text_color_for_background(
            EpaperColor background
    ) {
        if (
            background == EpaperColor::Black ||
            background == EpaperColor::Red
        ) {
            return EpaperColor::White;
        }

        return EpaperColor::Black;
    }

    void draw_metric_box(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int label_y,
        int label_scale,
        int value_y,
        int value_scale,
        int unit_y,
        int unit_scale,
        EpaperColor background
    ) {
        fill_metric_box(
            canvas,
            left,
            top,
            right,
            bottom,
            background
        );

        const EpaperColor foreground = text_color_for_background(background);

        draw_left_aligned_text(
            canvas,
            left + 4,
            label_y,
            label,
            foreground,
            label_scale
        );

        draw_centered_text(
            canvas,
            left,
            right,
            value_y,
            value,
            foreground,
            value_scale
        );

        draw_right_aligned_text(
            canvas,
            right - 3,
            unit_y,
            unit,
            foreground,
            unit_scale
        );
    }

    void draw_metric_tile(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int value_y,
        EpaperColor background
    ) {
        draw_metric_box(
            canvas,
            left,
            right,
            top,
            bottom,
            label,
            value,
            unit,
            top + 6,       // label y
            1,             // label scale
            value_y,
            3,             // value scale
            bottom - 9,    // unit y
            1,             // unit scale
            background
        );
    }

    void draw_metric_panel(
        EpaperCanvas& canvas,
        int left,
        int right,
        int top,
        int bottom,
        const char* label,
        const char* value,
        const char* unit,
        int value_y,
        EpaperColor background
    ) {
        draw_metric_box(
            canvas,
            left,
            right,
            top,
            bottom,
            label,
            value,
            unit,
            top + 5,       // label y
            2,             // label scale
            value_y,
            5,             // value scale
            bottom - 11,   // unit y
            1,             // unit scale
            background
        );
    }
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

void IAQMonitorApp::draw_dashboard(EpaperCanvas& canvas) {
    canvas.clear(EpaperColor::White);
    draw_outer_frame(canvas);

    // Header: title, status badge, and five-step overall quality scale.
    draw_left_aligned_text(
        canvas,
        8,
        10,
        "AIR QUALITY",
        EpaperColor::Black,
        1
    );

    draw_outline(
        canvas,
        80,
        6,
        62,
        18,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        82,
        8,
        58,
        14,
        EpaperColor::Yellow
    );
    canvas.draw_text(
        87,
        11,
        "MODERATE",
        EpaperColor::Black,
        1
    );

    constexpr int SCALE_BOX_Y = 7;
    constexpr int SCALE_BOX_WIDTH = 22;
    constexpr int SCALE_BOX_HEIGHT = 10;
    constexpr int SCALE_STEP = 24;
    constexpr int SCALE_START_X = 145;

    for (int step = 0; step < 5; ++step) {
        const int x = SCALE_START_X + step * SCALE_STEP;

        draw_outline(
            canvas,
            x,
            SCALE_BOX_Y,
            SCALE_BOX_WIDTH,
            SCALE_BOX_HEIGHT,
            EpaperColor::Black
        );

        if (step == 2) {
            canvas.fill_rectangle(
                x + 2,
                SCALE_BOX_Y + 2,
                SCALE_BOX_WIDTH - 4,
                SCALE_BOX_HEIGHT - 4,
                EpaperColor::Yellow
            );
        }

        canvas.draw_text(
            x + 8,
            19,
            step == 0 ? "1" :
            step == 1 ? "2" :
            step == 2 ? "3" :
            step == 3 ? "4" : "5",
            EpaperColor::Black,
            1
        );
    }

    //place holder for top right corner for future use

    // Main-panel divider and bottom-row divider.
    canvas.fill_rectangle(
        FRAME_X + 1,
        HEADER_BOTTOM,
        FRAME_WIDTH - 2,
        2,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        MAIN_DIVIDER_X,
        HEADER_BOTTOM,
        2,
        MAIN_BOTTOM - HEADER_BOTTOM,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        MAIN_TEMP_DIVIDER_X,
        HEADER_BOTTOM,
        2,
        MAIN_BOTTOM - HEADER_BOTTOM,
        EpaperColor::Black
    );
    canvas.fill_rectangle(
        FRAME_X + 1,
        MAIN_BOTTOM,
        FRAME_WIDTH - 2,
        2,
        EpaperColor::Black
    );

    // Main CO2 panel.
    draw_metric_panel(
        canvas,
        2,
        146,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "CO2",
        "842",
        "PPM",
        55,
        EpaperColor::White
    );

    // Main temperature panel.
    draw_metric_panel(
        canvas,
        150,
        220,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "TEMP",
        "72",
        "F",
        55,
        EpaperColor::White
    );

    // Main PM2.5 panel.
    draw_metric_panel(
        canvas,
        224,
        293,
        HEADER_BOTTOM + 2,
        MAIN_BOTTOM - 1,
        "PM2.5",
        "12",
        "UG/M3",
        55,
        EpaperColor::White
);

    // Six bottom tiles. The order intentionally groups PM1/PM10 together
    // beneath PM2.5 and places VOC/NOX beside the CO2 panel.

    for (int divider = 0; divider < 4; ++divider) {
        canvas.fill_rectangle(
            TILE_DIVIDER_X[divider],
            MAIN_BOTTOM,
            2,
            TILE_BOTTOM - MAIN_BOTTOM + 1,
            EpaperColor::Black
        );
    }

    // draw_left_aligned_text(canvas, TILE_LEFT[0] + 4, 112, "VOC", EpaperColor::Black, 1);
    // draw_centered_text(canvas, TILE_LEFT[0], TILE_RIGHT[0], 123, "73", EpaperColor::Black, 3);
    // draw_right_aligned_text(canvas, TILE_RIGHT[0] - 3, 148, "PPB", EpaperColor::Black, 1);

    // draw_left_aligned_text(canvas, TILE_LEFT[1] + 4, 112, "NOX", EpaperColor::Black, 1);
    // draw_centered_text(canvas, TILE_LEFT[1], TILE_RIGHT[1], 123, "12", EpaperColor::Black, 3);
    // draw_right_aligned_text(canvas, TILE_RIGHT[1] - 3, 148, "PPB", EpaperColor::Black, 1);

    // draw_left_aligned_text(canvas, TILE_LEFT[2] + 4, 112, "RH", EpaperColor::Black, 1);
    // draw_centered_text(canvas, TILE_LEFT[2], TILE_RIGHT[2], 123, "45", EpaperColor::Black, 3);
    // draw_right_aligned_text(canvas, TILE_RIGHT[2] - 3, 148, "%", EpaperColor::Black, 1);

    // draw_left_aligned_text(canvas, TILE_LEFT[3] + 4, 112, "PM1", EpaperColor::Black, 1);
    // draw_centered_text(canvas, TILE_LEFT[3], TILE_RIGHT[3], 123, "8", EpaperColor::Black, 3);
    // draw_right_aligned_text(canvas, TILE_RIGHT[3] - 3, 148, "UG/M3", EpaperColor::Black, 1);

    // draw_left_aligned_text(canvas, TILE_LEFT[4] + 4, 112, "PM10", EpaperColor::Black, 1);
    // draw_centered_text(canvas, TILE_LEFT[4], TILE_RIGHT[4], 123, "18", EpaperColor::Black, 3);
    // draw_right_aligned_text(canvas, TILE_RIGHT[4] - 3, 148, "UG/M3", EpaperColor::Black, 1);

    draw_metric_tile(
        canvas,
        TILE_LEFT[0],
        TILE_RIGHT[0],
        TILE_TOP,
        TILE_BOTTOM,
        "VOC",
        "73",
        "PPB",
        TILE_VALUE_Y,
        EpaperColor::White
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[1],
        TILE_RIGHT[1],
        TILE_TOP,
        TILE_BOTTOM,
        "NOX",
        "12",
        "PPB",
        TILE_VALUE_Y,
        EpaperColor::Yellow
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[2],
        TILE_RIGHT[2],
        TILE_TOP,
        TILE_BOTTOM,
        "RH",
        "45",
        "%",
        TILE_VALUE_Y,
        EpaperColor::White
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[3],
        TILE_RIGHT[3],
        TILE_TOP,
        TILE_BOTTOM,
        "PM1",
        "8",
        "UG/M3",
        TILE_VALUE_Y,
        EpaperColor::Red
    );

    draw_metric_tile(
        canvas,
        TILE_LEFT[4],
        TILE_RIGHT[4],
        TILE_TOP,
        TILE_BOTTOM,
        "PM10",
        "18",
        "UG/M3",
        TILE_VALUE_Y,
        EpaperColor::White
    );
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

    EpaperCanvas canvas;
    draw_dashboard(canvas);

    error = epaper_display_.display(
        canvas.data(),
        canvas.size()
    );

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to display canvas test: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "Static dashboard displayed");

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

//     error = sht41_.initialize(i2c_bus_);

//     if (error != ESP_OK) {
//         ESP_LOGE(
//             TAG,
//             "Failed to initialize SHT41: %s",
//             esp_err_to_name(error)
//         );
//         return error;
//     }

//     ESP_LOGI(TAG, "SHT41 registered on I2C bus");

    return ESP_OK;
}

void IAQMonitorApp::run() {
    ParticulateReading pms_reading{};

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
                    "SCD41 measurement: CO2=%u ppm | temperature=%.2f °C | humidity=%.1f%% RH",
                    scd41_reading.co2_ppm,
                    scd41_reading.temperature_c,
                    scd41_reading.relative_humidity_percent
                );
            }
        }

        // Sht41Reading sht41_reading{};

        // const esp_err_t sht41_error =
        //     sht41_.read_measurement(sht41_reading);

        // if (sht41_error != ESP_OK) {
        //     ESP_LOGW(
        //         TAG,
        //         "Failed to read SHT41 measurement: %s",
        //         esp_err_to_name(sht41_error)
        //     );
        // } else {
        //     ESP_LOGI(
        //         TAG,
        //         "SHT41 measurement: temperature=%.2f °C | humidity=%.1f%% RH",
        //         sht41_reading.temperature_c,
        //         sht41_reading.relative_humidity_percent
        //     );
        // }

        const esp_err_t error = pms5003_.read(pms_reading);

        if (error == ESP_ERR_TIMEOUT) {
            // ESP_LOGW(TAG, "Failed to read PMS5003 frame");
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
    }
}
