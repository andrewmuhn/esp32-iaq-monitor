#include "epaper_canvas.hpp"

#include <algorithm>

namespace {

uint8_t packed_color(EpaperColor color) {
    const uint8_t value = static_cast<uint8_t>(color);

    return
        (value << 6) |
        (value << 4) |
        (value << 2) |
        value;
}

constexpr int GLYPH_WIDTH = 5;
constexpr int GLYPH_HEIGHT = 7;

constexpr uint8_t GLYPH_A[GLYPH_HEIGHT] = {
    0b01110,
    0b10001,
    0b10001,
    0b11111,
    0b10001,
    0b10001,
    0b10001
};

}

EpaperCanvas::EpaperCanvas()
    : buffer_(BUFFER_SIZE) {
    clear(EpaperColor::White);
}

void EpaperCanvas::clear(EpaperColor color) {
    std::fill(
        buffer_.begin(),
        buffer_.end(),
        packed_color(color)
    );
}

void EpaperCanvas::set_pixel(
    int x,
    int y,
    EpaperColor color
) {
    if (
        x < 0 ||
        x >= WIDTH ||
        y < 0 ||
        y >= HEIGHT
    ) {
        return;
    }

    /*
     * Rotate landscape coordinates into the controller's
     * portrait-native coordinate system.
     */
    const int native_x = y;
    const int native_y = NATIVE_HEIGHT - 1 - x;

    const size_t bytes_per_native_row =
        NATIVE_WIDTH / PIXELS_PER_BYTE;

    const size_t byte_index =
        native_y * bytes_per_native_row +
        native_x / PIXELS_PER_BYTE;

    /*
     * Pixel positions within a byte:
     *
     * pixel 0 -> bits 7:6
     * pixel 1 -> bits 5:4
     * pixel 2 -> bits 3:2
     * pixel 3 -> bits 1:0
     */
    const int pixel_in_byte =
        native_x % PIXELS_PER_BYTE;

    const int shift =
        6 - pixel_in_byte * 2;

    const uint8_t mask =
        static_cast<uint8_t>(0b11 << shift);

    const uint8_t value =
        static_cast<uint8_t>(color) << shift;

    buffer_[byte_index] =
        static_cast<uint8_t>(
            (buffer_[byte_index] & ~mask) | value
        );
}

void EpaperCanvas::fill_rectangle(
    int x,
    int y,
    int width,
    int height,
    EpaperColor color
) {
    if (width <= 0 || height <= 0) {
        return;
    }

    for (int row = y; row < y + height; ++row) {
        for (int column = x;
             column < x + width;
             ++column) {
            set_pixel(column, row, color);
        }
    }
}

const uint8_t* EpaperCanvas::data() const {
    return buffer_.data();
}

size_t EpaperCanvas::size() const {
    return buffer_.size();
}

void EpaperCanvas::draw_character(
    int x,
    int y,
    char character,
    EpaperColor color
) {
    if (character != 'A') {
        return;
    }

    for (int row = 0; row < GLYPH_HEIGHT; ++row) {
        const uint8_t row_bits = GLYPH_A[row];

        for (int column = 0; column < GLYPH_WIDTH; ++column) {
            const uint8_t pixel_mask = static_cast<uint8_t>(
                1U << (GLYPH_WIDTH - 1 - column)
            );

            if ((row_bits & pixel_mask) != 0) {
                set_pixel(
                    x + column,
                    y + row,
                    color
                );
            }
        }
    }
}