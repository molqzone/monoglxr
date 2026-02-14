#pragma once

#include <cstdint>

namespace LibXR
{
namespace MonoGL
{

struct Font
{
  uint8_t glyph_width{0};
  uint8_t glyph_height{0};
  uint8_t first_char{32};
  uint8_t last_char{126};
  uint8_t ascent{0};
  uint8_t descent{0};
  const uint8_t* glyphs{nullptr};  // Packed by row, 1bpp.
};

}  // namespace MonoGL
}  // namespace LibXR
