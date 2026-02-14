#pragma once

#include <cstdint>

#include "surface.hpp"

namespace LibXR
{
namespace MonoGL
{

enum class Rotation : uint8_t
{
  R0,
  R90,
  R180,
  R270
};

enum class BufferMode : uint8_t
{
  FULL = 0,
  PAGE = 1
};

enum class PresentMode : uint8_t
{
  AUTO = 0,
  FULL = 1,
  DIRTY = 2
};

struct DisplayConfig
{
  uint16_t width{0};
  uint16_t height{0};
  Rotation rotation{Rotation::R0};
  BufferMode buffer_mode{BufferMode::FULL};
  uint8_t page_rows{8};  // Valid in Page mode.
  bool enable_dirty_tracking{true};
};

struct FrameView
{
  const uint8_t* bits{nullptr};  // 1bpp, row-major bit-packed.
  uint16_t width{0};
  uint16_t height{0};
  uint16_t stride_bytes{0};
  Rect dirty{};
};

struct BackendCaps
{
  bool partial_update{false};
  bool power_save{false};
  bool contrast{false};
  bool async_present{false};
};

}  // namespace MonoGL
}  // namespace LibXR
