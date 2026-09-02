#pragma once

#include <array>
#include <cstdint>

namespace Font5x7 {

inline constexpr int WIDTH = 5;
inline constexpr int HEIGHT = 7;

using Glyph = std::array<uint8_t, HEIGHT>;

const Glyph* find_glyph(char character);

}