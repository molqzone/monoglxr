#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif

#include <cstddef>
#include <utility>

#include "present.hpp"
#include "fonts/u8g2_font_6x10_ascii.hpp"
#include "win32_mock_backend.hpp"

namespace LibXR
{
namespace MonoGL
{
namespace DesktopMock
{

constexpr uint16_t DISPLAY_WIDTH = 128U;
constexpr uint16_t DISPLAY_HEIGHT = 64U;
constexpr std::size_t FRAMEBUFFER_BYTES =
    static_cast<std::size_t>((DISPLAY_WIDTH + 7U) / 8U) * DISPLAY_HEIGHT;

}  // namespace DesktopMock
}  // namespace MonoGL
}  // namespace LibXR

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command)
{
  UNUSED(instance);
  UNUSED(previous);
  UNUSED(command_line);
  UNUSED(show_command);

  using namespace LibXR::MonoGL;
  using namespace LibXR::MonoGL::DesktopMock;

  DisplayConfig config{};
  config.width = DISPLAY_WIDTH;
  config.height = DISPLAY_HEIGHT;
  config.rotation = Rotation::R0;
  config.buffer_mode = BufferMode::FULL;
  config.enable_dirty_tracking = true;

  Win32MockBackend backend(std::wstring(L"MonoGLXR Desktop Mock"), 6);
  Present<Win32MockBackend, FRAMEBUFFER_BYTES> presenter(std::move(backend), config);

  TextStyle style{};
  style.font = &U8G2_FONT_6X10_ASCII;
  style.color = Color::WHITE;
  style.scale_x = 1;
  style.scale_y = 1;
  style.letter_spacing = 0;

  Surface& surface = presenter.GetSurface();
  surface.Clear(Color::BLACK);
  surface.DrawTextTopLeft(Point{8, 8}, "hello world", style);
  (void)presenter.PresentFrame(PresentMode::FULL);

  MSG message{};
  while (true)
  {
    const BOOL RESULT = GetMessageW(&message, nullptr, 0, 0);
    if (RESULT == -1)
    {
      return 1;
    }
    if (RESULT == 0)
    {
      break;
    }
    TranslateMessage(&message);
    DispatchMessageW(&message);
  }
  return static_cast<int>(message.wParam);
}
