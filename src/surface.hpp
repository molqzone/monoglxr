#pragma once

#include <algorithm>
#include <cstdint>

namespace LibXR
{
namespace MonoGL
{

enum class Color : uint8_t
{
  BLACK = 0,
  WHITE = 1
};

enum class RasterOp : uint8_t
{
  COPY = 0,
  XOR = 1,
  AND = 2,
  OR = 3
};

struct Point
{
  int16_t x{0};
  int16_t y{0};
};

struct Size
{
  uint16_t w{0};
  uint16_t h{0};
};

struct Rect
{
  int16_t x{0};
  int16_t y{0};
  uint16_t w{0};
  uint16_t h{0};
};

inline bool rect_empty(Rect rect) noexcept { return rect.w == 0 || rect.h == 0; }

inline Rect intersect_rect(Rect a, Rect b) noexcept
{
  const int32_t LEFT = std::max<int32_t>(a.x, b.x);
  const int32_t TOP = std::max<int32_t>(a.y, b.y);
  const int32_t RIGHT =
      std::min<int32_t>(a.x + static_cast<int32_t>(a.w), b.x + static_cast<int32_t>(b.w));
  const int32_t BOTTOM =
      std::min<int32_t>(a.y + static_cast<int32_t>(a.h), b.y + static_cast<int32_t>(b.h));
  if (RIGHT <= LEFT || BOTTOM <= TOP)
  {
    return Rect{};
  }
  return Rect{
      static_cast<int16_t>(LEFT),
      static_cast<int16_t>(TOP),
      static_cast<uint16_t>(RIGHT - LEFT),
      static_cast<uint16_t>(BOTTOM - TOP),
  };
}

inline Rect union_rect(Rect a, Rect b) noexcept
{
  if (rect_empty(a))
  {
    return b;
  }
  if (rect_empty(b))
  {
    return a;
  }
  const int32_t LEFT = std::min<int32_t>(a.x, b.x);
  const int32_t TOP = std::min<int32_t>(a.y, b.y);
  const int32_t RIGHT =
      std::max<int32_t>(a.x + static_cast<int32_t>(a.w), b.x + static_cast<int32_t>(b.w));
  const int32_t BOTTOM =
      std::max<int32_t>(a.y + static_cast<int32_t>(a.h), b.y + static_cast<int32_t>(b.h));
  return Rect{
      static_cast<int16_t>(LEFT),
      static_cast<int16_t>(TOP),
      static_cast<uint16_t>(RIGHT - LEFT),
      static_cast<uint16_t>(BOTTOM - TOP),
  };
}

struct Font;  // Forward declaration.

struct TextStyle
{
  const Font* font{nullptr};
  Color color{Color::WHITE};
  RasterOp raster_op{RasterOp::COPY};
  uint8_t scale_x{1};
  uint8_t scale_y{1};
  int8_t letter_spacing{0};
};

class Surface
{
 public:
  Surface() = default;

  // stride_bytes = 0 means auto: ceil(width / 8).
  void Bind(uint8_t* bits, Size size, uint16_t stride_bytes = 0) noexcept;

  Size GetSize() const noexcept;
  uint16_t GetStrideBytes() const noexcept;
  void Clear(Color color = Color::BLACK) noexcept;

  void SetClip(Rect rect) noexcept;
  Rect GetClip() const noexcept;
  void ResetClip() noexcept;

  void DrawPixel(Point point, Color color = Color::WHITE,
                 RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawHLine(Point point, int16_t length, Color color = Color::WHITE,
                 RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawVLine(Point point, int16_t length, Color color = Color::WHITE,
                 RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawLine(Point p0, Point p1, Color color = Color::WHITE,
                RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawRect(Rect rect, Color color = Color::WHITE,
                RasterOp raster_op = RasterOp::COPY) noexcept;
  void FillRect(Rect rect, Color color = Color::WHITE,
                RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawCircle(Point center, uint8_t radius, Color color = Color::WHITE,
                  RasterOp raster_op = RasterOp::COPY) noexcept;

  void DrawBitmap(Point point, const uint8_t* bits, Size size,
                  Color foreground = Color::WHITE,
                  RasterOp raster_op = RasterOp::COPY) noexcept;
  void DrawText(Point baseline_left, const char* text, const TextStyle& style) noexcept;
  void DrawText(Point baseline_left, const char* text, const TextStyle& style,
                RasterOp raster_op) noexcept;
  void DrawTextTopLeft(Point top_left, const char* text, const TextStyle& style) noexcept;
  void DrawTextTopLeft(Point top_left, const char* text, const TextStyle& style,
                       RasterOp raster_op) noexcept;

  Rect GetDirtyRect() const noexcept;
  void ClearDirtyRect() noexcept;
  void AddDirtyRect(Rect rect) noexcept;

 private:
  Rect Bounds() const noexcept;
  bool InClip(Point point) const noexcept;
  void PlotUnchecked(int16_t x, int16_t y, Color color, RasterOp raster_op) noexcept;
  void MarkDirty(Rect rect) noexcept;

  uint8_t* bits_{nullptr};
  Size size_{};
  uint16_t stride_bytes_{0};
  Rect clip_{};
  Rect dirty_{};
};

}  // namespace MonoGL
}  // namespace LibXR
