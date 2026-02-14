#include "surface.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "font.hpp"

namespace LibXR
{
namespace MonoGL
{
namespace
{

Rect bounds_rect(Size size) noexcept
{
  return Rect{0, 0, size.w, size.h};
}

uint16_t default_stride(Size size) noexcept
{
  if (size.w == 0)
  {
    return 0;
  }
  return static_cast<uint16_t>((size.w + 7U) / 8U);
}

}  // namespace

void Surface::Bind(uint8_t* bits, Size size, uint16_t stride_bytes) noexcept
{
  bits_ = bits;
  size_ = size;
  stride_bytes_ = (stride_bytes == 0) ? default_stride(size) : stride_bytes;
  ResetClip();
  ClearDirtyRect();
}

Size Surface::GetSize() const noexcept
{
  return size_;
}

uint16_t Surface::GetStrideBytes() const noexcept
{
  return stride_bytes_;
}

void Surface::Clear(Color color) noexcept
{
  if (bits_ == nullptr || size_.w == 0 || size_.h == 0 || stride_bytes_ == 0)
  {
    return;
  }

  const std::size_t bytes = static_cast<std::size_t>(stride_bytes_) * static_cast<std::size_t>(size_.h);
  const uint8_t fill = (color == Color::WHITE) ? 0xFF : 0x00;
  std::memset(bits_, fill, bytes);
  MarkDirty(Bounds());
}

void Surface::SetClip(Rect rect) noexcept
{
  clip_ = intersect_rect(rect, Bounds());
}

Rect Surface::GetClip() const noexcept
{
  return clip_;
}

void Surface::ResetClip() noexcept
{
  clip_ = Bounds();
}

void Surface::DrawPixel(Point point, Color color, RasterOp raster_op) noexcept
{
  if (bits_ == nullptr || !InClip(point))
  {
    return;
  }

  PlotUnchecked(point.x, point.y, color, raster_op);
  MarkDirty(Rect{point.x, point.y, 1, 1});
}

void Surface::DrawHLine(Point point, int16_t length, Color color, RasterOp raster_op) noexcept
{
  if (bits_ == nullptr || length == 0)
  {
    return;
  }

  int32_t x = point.x;
  int32_t span = length;
  if (span < 0)
  {
    x += span;
    span = -span;
  }
  if (span <= 0)
  {
    return;
  }

  Rect rect{
      static_cast<int16_t>(x),
      point.y,
      static_cast<uint16_t>(span),
      1,
  };
  rect = intersect_rect(rect, clip_);
  if (rect_empty(rect))
  {
    return;
  }

  for (int32_t cx = rect.x; cx < rect.x + static_cast<int32_t>(rect.w); ++cx)
  {
    PlotUnchecked(static_cast<int16_t>(cx), rect.y, color, raster_op);
  }
  MarkDirty(rect);
}

void Surface::DrawVLine(Point point, int16_t length, Color color, RasterOp raster_op) noexcept
{
  if (bits_ == nullptr || length == 0)
  {
    return;
  }

  int32_t y = point.y;
  int32_t span = length;
  if (span < 0)
  {
    y += span;
    span = -span;
  }
  if (span <= 0)
  {
    return;
  }

  Rect rect{
      point.x,
      static_cast<int16_t>(y),
      1,
      static_cast<uint16_t>(span),
  };
  rect = intersect_rect(rect, clip_);
  if (rect_empty(rect))
  {
    return;
  }

  for (int32_t cy = rect.y; cy < rect.y + static_cast<int32_t>(rect.h); ++cy)
  {
    PlotUnchecked(rect.x, static_cast<int16_t>(cy), color, raster_op);
  }
  MarkDirty(rect);
}

void Surface::DrawLine(Point p0, Point p1, Color color, RasterOp raster_op) noexcept
{
  int32_t x0 = p0.x;
  int32_t y0 = p0.y;
  const int32_t x1 = p1.x;
  const int32_t y1 = p1.y;

  const int32_t dx = std::abs(x1 - x0);
  const int32_t sx = (x0 < x1) ? 1 : -1;
  const int32_t dy = -std::abs(y1 - y0);
  const int32_t sy = (y0 < y1) ? 1 : -1;
  int32_t err = dx + dy;

  while (true)
  {
    DrawPixel(Point{static_cast<int16_t>(x0), static_cast<int16_t>(y0)}, color, raster_op);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }
    const int32_t e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

void Surface::DrawRect(Rect rect, Color color, RasterOp raster_op) noexcept
{
  if (rect_empty(rect))
  {
    return;
  }

  DrawHLine(Point{rect.x, rect.y}, static_cast<int16_t>(rect.w), color, raster_op);
  if (rect.h > 1)
  {
    DrawHLine(Point{rect.x, static_cast<int16_t>(rect.y + static_cast<int32_t>(rect.h) - 1)},
              static_cast<int16_t>(rect.w),
              color,
              raster_op);
  }
  if (rect.h > 2)
  {
    DrawVLine(Point{rect.x, static_cast<int16_t>(rect.y + 1)},
              static_cast<int16_t>(rect.h - 2),
              color,
              raster_op);
    if (rect.w > 1)
    {
      DrawVLine(Point{static_cast<int16_t>(rect.x + static_cast<int32_t>(rect.w) - 1),
                      static_cast<int16_t>(rect.y + 1)},
                static_cast<int16_t>(rect.h - 2),
                color,
                raster_op);
    }
  }
}

void Surface::FillRect(Rect rect, Color color, RasterOp raster_op) noexcept
{
  rect = intersect_rect(rect, clip_);
  if (rect_empty(rect))
  {
    return;
  }

  for (int32_t y = rect.y; y < rect.y + static_cast<int32_t>(rect.h); ++y)
  {
    DrawHLine(Point{rect.x, static_cast<int16_t>(y)},
              static_cast<int16_t>(rect.w),
              color,
              raster_op);
  }
}

void Surface::DrawCircle(Point center, uint8_t radius, Color color, RasterOp raster_op) noexcept
{
  int32_t x = radius;
  int32_t y = 0;
  int32_t err = 1 - x;

  while (x >= y)
  {
    DrawPixel(Point{static_cast<int16_t>(center.x + x), static_cast<int16_t>(center.y + y)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x + y), static_cast<int16_t>(center.y + x)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x - y), static_cast<int16_t>(center.y + x)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x - x), static_cast<int16_t>(center.y + y)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x - x), static_cast<int16_t>(center.y - y)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x - y), static_cast<int16_t>(center.y - x)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x + y), static_cast<int16_t>(center.y - x)},
              color,
              raster_op);
    DrawPixel(Point{static_cast<int16_t>(center.x + x), static_cast<int16_t>(center.y - y)},
              color,
              raster_op);

    ++y;
    if (err < 0)
    {
      err += 2 * y + 1;
    }
    else
    {
      --x;
      err += 2 * (y - x + 1);
    }
  }
}

void Surface::DrawBitmap(Point point,
                         const uint8_t* bits,
                         Size size,
                         Color foreground,
                         RasterOp raster_op) noexcept
{
  if (bits == nullptr || size.w == 0 || size.h == 0)
  {
    return;
  }

  const uint16_t src_stride = default_stride(size);
  for (uint16_t y = 0; y < size.h; ++y)
  {
    for (uint16_t x = 0; x < size.w; ++x)
    {
      const uint8_t byte = bits[static_cast<std::size_t>(y) * src_stride +
                                static_cast<std::size_t>(x / 8)];
      const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x7));
      if ((byte & mask) != 0U)
      {
        DrawPixel(Point{static_cast<int16_t>(point.x + x), static_cast<int16_t>(point.y + y)},
                  foreground,
                  raster_op);
      }
    }
  }
}

void Surface::DrawText(Point baseline_left, const char* text, const TextStyle& style) noexcept
{
  if (text == nullptr || style.font == nullptr)
  {
    return;
  }

  const Font& font = *style.font;
  if (font.glyphs == nullptr || font.glyph_width == 0 || font.glyph_height == 0 ||
      font.last_char < font.first_char)
  {
    return;
  }

  const uint8_t sx = (style.scale_x == 0) ? 1 : style.scale_x;
  const uint8_t sy = (style.scale_y == 0) ? 1 : style.scale_y;
  const uint16_t glyph_stride =
      static_cast<uint16_t>(((font.glyph_width + 7) / 8) * static_cast<uint16_t>(font.glyph_height));
  const int32_t advance = static_cast<int32_t>(font.glyph_width) * sx + style.letter_spacing;

  int32_t cursor_x = baseline_left.x;
  int32_t cursor_y = baseline_left.y;
  for (const char* p = text; *p != '\0'; ++p)
  {
    if (*p == '\n')
    {
      cursor_x = baseline_left.x;
      cursor_y += static_cast<int32_t>(font.glyph_height) * sy + 1;
      continue;
    }

    const uint8_t ch = static_cast<uint8_t>(*p);
    if (ch >= font.first_char && ch <= font.last_char)
    {
      const std::size_t glyph_index = static_cast<std::size_t>(ch - font.first_char);
      const uint8_t* glyph = font.glyphs + glyph_index * glyph_stride;

      for (uint8_t gy = 0; gy < font.glyph_height; ++gy)
      {
        for (uint8_t gx = 0; gx < font.glyph_width; ++gx)
        {
          const uint8_t byte =
              glyph[static_cast<std::size_t>(gy) * ((font.glyph_width + 7) / 8) + (gx / 8)];
          const uint8_t mask = static_cast<uint8_t>(0x80U >> (gx & 0x7));
          if ((byte & mask) != 0U)
          {
            FillRect(
                Rect{
                    static_cast<int16_t>(cursor_x + static_cast<int32_t>(gx) * sx),
                    static_cast<int16_t>(cursor_y + static_cast<int32_t>(gy) * sy),
                    sx,
                    sy,
                },
                style.color,
                style.raster_op);
          }
        }
      }
    }
    cursor_x += advance;
  }
}

void Surface::DrawText(Point baseline_left,
                       const char* text,
                       const TextStyle& style,
                       RasterOp raster_op) noexcept
{
  TextStyle override_style = style;
  override_style.raster_op = raster_op;
  DrawText(baseline_left, text, override_style);
}

Rect Surface::GetDirtyRect() const noexcept
{
  return dirty_;
}

void Surface::ClearDirtyRect() noexcept
{
  dirty_ = Rect{};
}

void Surface::AddDirtyRect(Rect rect) noexcept
{
  MarkDirty(rect);
}

Rect Surface::Bounds() const noexcept
{
  return bounds_rect(size_);
}

bool Surface::InClip(Point point) const noexcept
{
  if (point.x < clip_.x || point.y < clip_.y)
  {
    return false;
  }
  if (point.x >= clip_.x + static_cast<int32_t>(clip_.w) ||
      point.y >= clip_.y + static_cast<int32_t>(clip_.h))
  {
    return false;
  }
  return true;
}

void Surface::PlotUnchecked(int16_t x, int16_t y, Color color, RasterOp raster_op) noexcept
{
  if (stride_bytes_ == 0)
  {
    return;
  }

  const std::size_t byte_index =
      static_cast<std::size_t>(y) * stride_bytes_ + static_cast<std::size_t>(x / 8);
  const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 0x7));
  const bool src_is_set = (color == Color::WHITE);
  uint8_t& dst = bits_[byte_index];

  switch (raster_op)
  {
    case RasterOp::COPY:
      if (src_is_set)
      {
        dst |= mask;
      }
      else
      {
        dst &= static_cast<uint8_t>(~mask);
      }
      break;
    case RasterOp::XOR:
      if (src_is_set)
      {
        dst ^= mask;
      }
      break;
    case RasterOp::AND:
      if (!src_is_set)
      {
        dst &= static_cast<uint8_t>(~mask);
      }
      break;
    case RasterOp::OR:
      if (src_is_set)
      {
        dst |= mask;
      }
      break;
  }
}

void Surface::MarkDirty(Rect rect) noexcept
{
  Rect clipped = intersect_rect(rect, Bounds());
  if (rect_empty(clipped))
  {
    return;
  }

  if (rect_empty(dirty_))
  {
    dirty_ = clipped;
    return;
  }
  dirty_ = union_rect(dirty_, clipped);
}

}  // namespace MonoGL
}  // namespace LibXR
