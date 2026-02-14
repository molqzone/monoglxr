#include "surface.hpp"

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

Rect bounds_rect(Size size) noexcept { return Rect{0, 0, size.w, size.h}; }

uint16_t default_stride(Size size) noexcept
{
  if (size.w == 0)
  {
    return 0;
  }
  return static_cast<uint16_t>((size.w + 7U) / 8U);
}

int32_t font_ascent(const Font& font) noexcept
{
  if (font.ascent == 0)
  {
    return static_cast<int32_t>(font.glyph_height);
  }
  return static_cast<int32_t>(font.ascent);
}

int32_t font_descent(const Font& font) noexcept
{
  return static_cast<int32_t>(font.descent);
}

int32_t font_line_height(const Font& font) noexcept
{
  const int32_t LINE_HEIGHT = font_ascent(font) + font_descent(font);
  if (LINE_HEIGHT <= 0)
  {
    return static_cast<int32_t>(font.glyph_height);
  }
  return LINE_HEIGHT;
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

Size Surface::GetSize() const noexcept { return size_; }

uint16_t Surface::GetStrideBytes() const noexcept { return stride_bytes_; }

void Surface::Clear(Color color) noexcept
{
  if (bits_ == nullptr || size_.w == 0 || size_.h == 0 || stride_bytes_ == 0)
  {
    return;
  }

  const std::size_t BYTES =
      static_cast<std::size_t>(stride_bytes_) * static_cast<std::size_t>(size_.h);
  const uint8_t FILL = (color == Color::WHITE) ? 0xFF : 0x00;
  std::memset(bits_, FILL, BYTES);
  MarkDirty(Bounds());
}

void Surface::SetClip(Rect rect) noexcept { clip_ = intersect_rect(rect, Bounds()); }

Rect Surface::GetClip() const noexcept { return clip_; }

void Surface::ResetClip() noexcept { clip_ = Bounds(); }

void Surface::DrawPixel(Point point, Color color, RasterOp raster_op) noexcept
{
  if (bits_ == nullptr || !InClip(point))
  {
    return;
  }

  PlotUnchecked(point.x, point.y, color, raster_op);
  MarkDirty(Rect{point.x, point.y, 1, 1});
}

void Surface::DrawHLine(Point point, int16_t length, Color color,
                        RasterOp raster_op) noexcept
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

void Surface::DrawVLine(Point point, int16_t length, Color color,
                        RasterOp raster_op) noexcept
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
  const int32_t X1 = p1.x;
  const int32_t Y1 = p1.y;

  const int32_t DX = std::abs(X1 - x0);
  const int32_t SX = (x0 < X1) ? 1 : -1;
  const int32_t DY = -std::abs(Y1 - y0);
  const int32_t SY = (y0 < Y1) ? 1 : -1;
  int32_t err = DX + DY;

  while (true)
  {
    DrawPixel(Point{static_cast<int16_t>(x0), static_cast<int16_t>(y0)}, color,
              raster_op);
    if (x0 == X1 && y0 == Y1)
    {
      break;
    }
    const int32_t E2 = 2 * err;
    if (E2 >= DY)
    {
      err += DY;
      x0 += SX;
    }
    if (E2 <= DX)
    {
      err += DX;
      y0 += SY;
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
    DrawHLine(
        Point{rect.x, static_cast<int16_t>(rect.y + static_cast<int32_t>(rect.h) - 1)},
        static_cast<int16_t>(rect.w), color, raster_op);
  }
  if (rect.h > 2)
  {
    DrawVLine(Point{rect.x, static_cast<int16_t>(rect.y + 1)},
              static_cast<int16_t>(rect.h - 2), color, raster_op);
    if (rect.w > 1)
    {
      DrawVLine(Point{static_cast<int16_t>(rect.x + static_cast<int32_t>(rect.w) - 1),
                      static_cast<int16_t>(rect.y + 1)},
                static_cast<int16_t>(rect.h - 2), color, raster_op);
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
    DrawHLine(Point{rect.x, static_cast<int16_t>(y)}, static_cast<int16_t>(rect.w), color,
              raster_op);
  }
}

void Surface::DrawCircle(Point center, uint8_t radius, Color color,
                         RasterOp raster_op) noexcept
{
  int32_t x = radius;
  int32_t y = 0;
  int32_t err = 1 - x;

  while (x >= y)
  {
    DrawPixel(
        Point{static_cast<int16_t>(center.x + x), static_cast<int16_t>(center.y + y)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x + y), static_cast<int16_t>(center.y + x)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x - y), static_cast<int16_t>(center.y + x)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x - x), static_cast<int16_t>(center.y + y)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x - x), static_cast<int16_t>(center.y - y)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x - y), static_cast<int16_t>(center.y - x)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x + y), static_cast<int16_t>(center.y - x)},
        color, raster_op);
    DrawPixel(
        Point{static_cast<int16_t>(center.x + x), static_cast<int16_t>(center.y - y)},
        color, raster_op);

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

void Surface::DrawBitmap(Point point, const uint8_t* bits, Size size, Color foreground,
                         RasterOp raster_op) noexcept
{
  if (bits == nullptr || size.w == 0 || size.h == 0)
  {
    return;
  }

  const uint16_t SRC_STRIDE = default_stride(size);
  for (uint16_t y = 0; y < size.h; ++y)
  {
    for (uint16_t x = 0; x < size.w; ++x)
    {
      const uint8_t BYTE = bits[static_cast<std::size_t>(y) * SRC_STRIDE +
                                static_cast<std::size_t>(x / 8)];
      const uint8_t MASK = static_cast<uint8_t>(0x80U >> (x & 0x7));
      if ((BYTE & MASK) != 0U)
      {
        DrawPixel(
            Point{static_cast<int16_t>(point.x + x), static_cast<int16_t>(point.y + y)},
            foreground, raster_op);
      }
    }
  }
}

void Surface::DrawText(Point baseline_left, const char* text,
                       const TextStyle& style) noexcept
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

  const uint8_t SCALE_X = (style.scale_x == 0) ? 1 : style.scale_x;
  const uint8_t SCALE_Y = (style.scale_y == 0) ? 1 : style.scale_y;
  const uint16_t GLYPH_STRIDE = static_cast<uint16_t>(
      ((font.glyph_width + 7) / 8) * static_cast<uint16_t>(font.glyph_height));
  const int32_t ADVANCE =
      static_cast<int32_t>(font.glyph_width) * SCALE_X + style.letter_spacing;
  const int32_t ASCENT = font_ascent(font);
  const int32_t LINE_HEIGHT = font_line_height(font);

  int32_t cursor_x = baseline_left.x;
  int32_t baseline_y = baseline_left.y;
  for (const char* p = text; *p != '\0'; ++p)
  {
    if (*p == '\n')
    {
      cursor_x = baseline_left.x;
      baseline_y += LINE_HEIGHT * SCALE_Y + 1;
      continue;
    }

    const int32_t TOP_Y = baseline_y - ASCENT * SCALE_Y;
    const uint8_t CH = static_cast<uint8_t>(*p);
    if (CH >= font.first_char && CH <= font.last_char)
    {
      const std::size_t GLYPH_INDEX = static_cast<std::size_t>(CH - font.first_char);
      const uint8_t* glyph = font.glyphs + GLYPH_INDEX * GLYPH_STRIDE;

      for (uint8_t gy = 0; gy < font.glyph_height; ++gy)
      {
        for (uint8_t gx = 0; gx < font.glyph_width; ++gx)
        {
          const uint8_t BYTE =
              glyph[static_cast<std::size_t>(gy) * ((font.glyph_width + 7) / 8) +
                    (gx / 8)];
          const uint8_t MASK = static_cast<uint8_t>(0x80U >> (gx & 0x7));
          if ((BYTE & MASK) != 0U)
          {
            FillRect(
                Rect{
                    static_cast<int16_t>(cursor_x + static_cast<int32_t>(gx) * SCALE_X),
                    static_cast<int16_t>(TOP_Y + static_cast<int32_t>(gy) * SCALE_Y),
                    SCALE_X,
                    SCALE_Y,
                },
                style.color, style.raster_op);
          }
        }
      }
    }
    cursor_x += ADVANCE;
  }
}

void Surface::DrawText(Point baseline_left, const char* text, const TextStyle& style,
                       RasterOp raster_op) noexcept
{
  TextStyle override_style = style;
  override_style.raster_op = raster_op;
  DrawText(baseline_left, text, override_style);
}

void Surface::DrawTextTopLeft(Point top_left, const char* text,
                              const TextStyle& style) noexcept
{
  if (style.font == nullptr)
  {
    DrawText(top_left, text, style);
    return;
  }

  const uint8_t SCALE_Y = (style.scale_y == 0) ? 1 : style.scale_y;
  const int32_t ASCENT = font_ascent(*style.font);
  const Point BASELINE_LEFT{
      top_left.x,
      static_cast<int16_t>(top_left.y + ASCENT * SCALE_Y),
  };
  DrawText(BASELINE_LEFT, text, style);
}

void Surface::DrawTextTopLeft(Point top_left, const char* text, const TextStyle& style,
                              RasterOp raster_op) noexcept
{
  TextStyle override_style = style;
  override_style.raster_op = raster_op;
  DrawTextTopLeft(top_left, text, override_style);
}

Rect Surface::GetDirtyRect() const noexcept { return dirty_; }

void Surface::ClearDirtyRect() noexcept { dirty_ = Rect{}; }

void Surface::AddDirtyRect(Rect rect) noexcept { MarkDirty(rect); }

Rect Surface::Bounds() const noexcept { return bounds_rect(size_); }

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

void Surface::PlotUnchecked(int16_t x, int16_t y, Color color,
                            RasterOp raster_op) noexcept
{
  if (stride_bytes_ == 0)
  {
    return;
  }

  const std::size_t BYTE_INDEX =
      static_cast<std::size_t>(y) * stride_bytes_ + static_cast<std::size_t>(x / 8);
  const uint8_t MASK = static_cast<uint8_t>(0x80U >> (x & 0x7));
  const bool SRC_IS_SET = (color == Color::WHITE);
  uint8_t& dst = bits_[BYTE_INDEX];

  switch (raster_op)
  {
    case RasterOp::COPY:
      if (SRC_IS_SET)
      {
        dst |= MASK;
      }
      else
      {
        dst &= static_cast<uint8_t>(~MASK);
      }
      break;
    case RasterOp::XOR:
      if (SRC_IS_SET)
      {
        dst ^= MASK;
      }
      break;
    case RasterOp::AND:
      if (!SRC_IS_SET)
      {
        dst &= static_cast<uint8_t>(~MASK);
      }
      break;
    case RasterOp::OR:
      if (SRC_IS_SET)
      {
        dst |= MASK;
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
