#include "epaper_display.hpp"

#include <cstdint>
#include <array>

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

constexpr char TAG[] = "EPAPER_DISPLAY";

constexpr int DISPLAY_WIDTH = 160;
constexpr int DISPLAY_HEIGHT = 296;
constexpr int BITS_PER_PIXEL = 2;

constexpr int DISPLAY_BUFFER_SIZE =
    DISPLAY_WIDTH * DISPLAY_HEIGHT * BITS_PER_PIXEL / 8;

constexpr int SPI_CLOCK_HZ = 4'000'000;

uint64_t pin_mask(gpio_num_t pin) {
    return uint64_t{1} << static_cast<unsigned>(pin);
}

}

EpaperDisplay::EpaperDisplay(const EpaperDisplayConfig& config)
    : config_(config) {}

esp_err_t EpaperDisplay::initialize_gpio() {
    gpio_config_t output_config{};

    output_config.pin_bit_mask =
        pin_mask(config_.chip_select_pin) |
        pin_mask(config_.data_command_pin) |
        pin_mask(config_.reset_pin) |
        pin_mask(config_.power_pin);

    output_config.mode = GPIO_MODE_OUTPUT;
    output_config.pull_up_en = GPIO_PULLUP_DISABLE;
    output_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    output_config.intr_type = GPIO_INTR_DISABLE;

    esp_err_t error = gpio_config(&output_config);

    if (error != ESP_OK) {
        return error;
    }

    gpio_config_t input_config{};

    input_config.pin_bit_mask = pin_mask(config_.busy_pin);
    input_config.mode = GPIO_MODE_INPUT;
    input_config.pull_up_en = GPIO_PULLUP_DISABLE;
    input_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    input_config.intr_type = GPIO_INTR_DISABLE;

    error = gpio_config(&input_config);

    if (error != ESP_OK) {
        return error;
    }

    // Establish safe idle states before powering the display circuitry.
    gpio_set_level(config_.chip_select_pin, 1);
    gpio_set_level(config_.data_command_pin, 0);
    gpio_set_level(config_.reset_pin, 1);
    gpio_set_level(config_.power_pin, 0);

    return ESP_OK;
}

esp_err_t EpaperDisplay::initialize_spi() {
    spi_bus_config_t bus_config{};

    bus_config.mosi_io_num = config_.mosi_pin;
    bus_config.miso_io_num = GPIO_NUM_NC;
    bus_config.sclk_io_num = config_.clock_pin;
    bus_config.quadwp_io_num = GPIO_NUM_NC;
    bus_config.quadhd_io_num = GPIO_NUM_NC;
    bus_config.max_transfer_sz = DISPLAY_BUFFER_SIZE;

    esp_err_t error = spi_bus_initialize(
        config_.spi_host,
        &bus_config,
        SPI_DMA_CH_AUTO
    );

    if (error != ESP_OK) {
        return error;
    }

    spi_device_interface_config_t device_config{};

    device_config.clock_speed_hz = SPI_CLOCK_HZ;
    device_config.mode = 0;
    device_config.spics_io_num = config_.chip_select_pin;
    device_config.queue_size = 1;

    return spi_bus_add_device(
        config_.spi_host,
        &device_config,
        &spi_device_
    );
}

esp_err_t EpaperDisplay::initialize() {
    if (initialized_) {
        return ESP_OK;
    }

    esp_err_t error = initialize_gpio();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "GPIO initialization failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = initialize_spi();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "SPI initialization failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = gpio_set_level(config_.power_pin, 1);

    if (error != ESP_OK) {
        return error;
    }

    // Give the HAT's power circuitry time to stabilize.
    vTaskDelay(pdMS_TO_TICKS(50));

    error = hardware_reset();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Hardware reset failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = wait_until_ready(5'000);

    if (error != ESP_OK) {
        return error;
    }

    error = initialize_controller();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Controller initialization failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    initialized_ = true;

    ESP_LOGI(
        TAG,
        "Interface and controller initialized: "
        "SPI host %d, BUSY=%d",
        static_cast<int>(config_.spi_host),
        gpio_get_level(config_.busy_pin)
    );

    return ESP_OK;
}

esp_err_t EpaperDisplay::hardware_reset() {
    esp_err_t error = gpio_set_level(config_.reset_pin, 1);

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    error = gpio_set_level(config_.reset_pin, 0);

    if (error != ESP_OK) {
        return error;
    }

    // The reference pulse is only 2 ms. A FreeRTOS tick can be longer
    // than that, so use the ROM microsecond delay for this short pulse.
    esp_rom_delay_us(2'000);

    error = gpio_set_level(config_.reset_pin, 1);

    if (error != ESP_OK) {
        return error;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    return ESP_OK;
}

esp_err_t EpaperDisplay::wait_until_ready(uint32_t timeout_ms) {
    const TickType_t start = xTaskGetTickCount();
    const TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

    while (gpio_get_level(config_.busy_pin) == 0) {
        if ((xTaskGetTickCount() - start) >= timeout) {
            ESP_LOGE(
                TAG,
                "Timed out waiting for display controller; BUSY stayed low"
            );
            return ESP_ERR_TIMEOUT;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }

    return ESP_OK;
}

esp_err_t EpaperDisplay::transmit(
    const uint8_t* data,
    size_t size
) {
    if (data == nullptr || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    spi_transaction_t transaction{};

    transaction.length = size * 8;
    transaction.tx_buffer = data;

    return spi_device_transmit(
        spi_device_,
        &transaction
    );
}

esp_err_t EpaperDisplay::send_command(uint8_t command) {
    esp_err_t error = gpio_set_level(
        config_.data_command_pin,
        0
    );

    if (error != ESP_OK) {
        return error;
    }

    return transmit(&command, 1);
}

esp_err_t EpaperDisplay::send_data(
    const uint8_t* data,
    size_t size
) {
    esp_err_t error = gpio_set_level(
        config_.data_command_pin,
        1
    );

    if (error != ESP_OK) {
        return error;
    }

    return transmit(data, size);
}

esp_err_t EpaperDisplay::write_register(
    uint8_t command,
    std::initializer_list<uint8_t> data
) {
    esp_err_t error = send_command(command);

    if (error != ESP_OK) {
        return error;
    }

    if (data.size() == 0) {
        return ESP_OK;
    }

    return send_data(data.begin(), data.size());
}

esp_err_t EpaperDisplay::initialize_controller() {
    esp_err_t error = write_register(0x4D, {0x78});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x00, {0x0F, 0x29});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x01, {0x07, 0x00});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x03, {0x10, 0x54, 0x44});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(
        0x06,
        {0x0F, 0x0A, 0x2F, 0x25, 0x22, 0x2E, 0x21}
    );

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x30, {0x02});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x41, {0x00});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x50, {0x37});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0x60, {0x02, 0x02});

    if (error != ESP_OK) {
        return error;
    }

    // Native controller dimensions: 160 × 296.
    error = write_register(
        0x61,
        {
            0x00, 0xA0,  // 160
            0x01, 0x28   // 296
        }
    );

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(
        0x65,
        {0x00, 0x00, 0x00, 0x00}
    );

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xE7, {0x1C});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xE3, {0x22});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xE0, {0x00});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xB4, {0xD0});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xB5, {0x03});

    if (error != ESP_OK) {
        return error;
    }

    error = write_register(0xE9, {0x01});

    if (error != ESP_OK) {
        return error;
    }

    // Power on the panel's internal high-voltage circuitry.
    error = send_command(0x04);

    if (error != ESP_OK) {
        return error;
    }

    // Give BUSY time to react before testing its level.
    vTaskDelay(pdMS_TO_TICKS(10));

    return wait_until_ready(5'000);
}

esp_err_t EpaperDisplay::refresh() {
    esp_err_t error = write_register(0x12, {0x00});

    if (error != ESP_OK) {
        return error;
    }

    // Let the controller assert BUSY before we begin polling it.
    vTaskDelay(pdMS_TO_TICKS(10));

    return wait_until_ready(45'000);
}

esp_err_t EpaperDisplay::show_test_pattern() {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    constexpr size_t PIXELS_PER_BYTE = 4;
    constexpr size_t ROW_SIZE =
        DISPLAY_WIDTH / PIXELS_PER_BYTE;
    constexpr int ROWS_PER_COLOR =
        DISPLAY_HEIGHT / 4;

    static_assert(DISPLAY_WIDTH % PIXELS_PER_BYTE == 0);
    static_assert(DISPLAY_HEIGHT % 4 == 0);

    constexpr std::array<uint8_t, 4> PACKED_COLORS = {
        0x00, // Black:  00 00 00 00
        0x55, // White:  01 01 01 01
        0xAA, // Yellow: 10 10 10 10
        0xFF  // Red:    11 11 11 11
    };

    esp_err_t error = send_command(0x10);

    if (error != ESP_OK) {
        return error;
    }

    std::array<uint8_t, ROW_SIZE> row{};

    for (const uint8_t packed_color : PACKED_COLORS) {
        row.fill(packed_color);

        for (int row_number = 0;
             row_number < ROWS_PER_COLOR;
             ++row_number) {
            error = send_data(row.data(), row.size());

            if (error != ESP_OK) {
                return error;
            }
        }
    }

    ESP_LOGI(TAG, "Test pattern transferred; refreshing display");

    error = refresh();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Display refresh failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "Test pattern refresh completed");

    return ESP_OK;
}

esp_err_t EpaperDisplay::display(
    const uint8_t* buffer,
    size_t size
) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }

    if (
        buffer == nullptr ||
        size != DISPLAY_BUFFER_SIZE
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t error = send_command(0x10);

    if (error != ESP_OK) {
        return error;
    }

    error = send_data(buffer, size);

    if (error != ESP_OK) {
        return error;
    }

    ESP_LOGI(
        TAG,
        "Framebuffer transferred; refreshing display"
    );

    error = refresh();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Framebuffer refresh failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    ESP_LOGI(TAG, "Framebuffer refresh completed");

    return ESP_OK;
}