#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class EpaperColor : uint8_t {
    Black = 0,
    White = 1,
    Yellow = 2,
    Red = 3
};

class EpaperCanvas {
    public:
        static constexpr int WIDTH = 296;
        static constexpr int HEIGHT = 160;

        EpaperCanvas();

        void clear(EpaperColor color);

        void set_pixel(
            int x,
            int y,
            EpaperColor color
        );

        void fill_rectangle(
            int x,
            int y,
            int width,
            int height,
            EpaperColor color
        );

        void draw_character(
            int x,
            int y,
            char character,
            EpaperColor color,
            int scale = 1
        );

        void draw_text(
            int x,
            int y,
            const char* text,
            EpaperColor color,
            int scale = 1
        );

        const uint8_t* data() const;
        size_t size() const;

    private:
        static constexpr int NATIVE_WIDTH = 160;
        static constexpr int NATIVE_HEIGHT = 296;
        static constexpr int PIXELS_PER_BYTE = 4;

        static constexpr size_t BUFFER_SIZE =
            NATIVE_WIDTH * NATIVE_HEIGHT / PIXELS_PER_BYTE;

        std::vector<uint8_t> buffer_;
};