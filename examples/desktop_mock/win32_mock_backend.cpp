#include "win32_mock_backend.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace LibXR
{
namespace MonoGL
{
namespace DesktopMock
{

struct Win32MockBackendState
{
  DisplayConfig config{};
  std::wstring window_title{L"MonoGLXR Desktop Mock"};
  int window_scale{6};
  HINSTANCE instance{nullptr};
  HWND hwnd{nullptr};
  BITMAPINFO bitmap_info{};
  std::vector<uint32_t> rgba_buffer{};
};

namespace
{

Win32MockBackendState* get_state(HWND hwnd) noexcept
{
  return reinterpret_cast<Win32MockBackendState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

LRESULT CALLBACK mock_window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) noexcept
{
  if (message == WM_NCCREATE)
  {
    const CREATESTRUCTW* CREATE_STRUCT = reinterpret_cast<const CREATESTRUCTW*>(lparam);
    auto* STATE = reinterpret_cast<Win32MockBackendState*>(CREATE_STRUCT->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(STATE));
    if (STATE != nullptr)
    {
      STATE->hwnd = hwnd;
    }
    return TRUE;
  }

  Win32MockBackendState* state = get_state(hwnd);
  switch (message)
  {
    case WM_ERASEBKGND:
      return 1;
    case WM_PAINT:
    {
      PAINTSTRUCT PAINT_STRUCT{};
      HDC device_context = BeginPaint(hwnd, &PAINT_STRUCT);
      if (state != nullptr && !state->rgba_buffer.empty())
      {
        RECT client_rect{};
        GetClientRect(hwnd, &client_rect);
        const int CLIENT_WIDTH = client_rect.right - client_rect.left;
        const int CLIENT_HEIGHT = client_rect.bottom - client_rect.top;
        StretchDIBits(device_context,
                      0,
                      0,
                      CLIENT_WIDTH,
                      CLIENT_HEIGHT,
                      0,
                      0,
                      state->config.width,
                      state->config.height,
                      state->rgba_buffer.data(),
                      &state->bitmap_info,
                      DIB_RGB_COLORS,
                      SRCCOPY);
      }
      EndPaint(hwnd, &PAINT_STRUCT);
      return 0;
    }
    case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool register_window_class(HINSTANCE instance, const wchar_t* class_name) noexcept
{
  static ATOM WINDOW_CLASS_ATOM = 0;
  if (WINDOW_CLASS_ATOM != 0)
  {
    return true;
  }

  WNDCLASSEXW WINDOW_CLASS{};
  WINDOW_CLASS.cbSize = sizeof(WNDCLASSEXW);
  WINDOW_CLASS.style = CS_HREDRAW | CS_VREDRAW;
  WINDOW_CLASS.lpfnWndProc = mock_window_proc;
  WINDOW_CLASS.hInstance = instance;
  WINDOW_CLASS.hCursor = LoadCursor(nullptr, IDC_ARROW);
  WINDOW_CLASS.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  WINDOW_CLASS.lpszClassName = class_name;

  WINDOW_CLASS_ATOM = RegisterClassExW(&WINDOW_CLASS);
  if (WINDOW_CLASS_ATOM == 0)
  {
    const DWORD ERROR_CODE = GetLastError();
    return ERROR_CODE == ERROR_CLASS_ALREADY_EXISTS;
  }
  return true;
}

}  // namespace

Win32MockBackend::Win32MockBackend()
    : Win32MockBackend(std::wstring(L"MonoGLXR Desktop Mock"), 6)
{
}

Win32MockBackend::Win32MockBackend(std::wstring window_title, int window_scale) noexcept
    : state_(std::make_shared<Win32MockBackendState>())
{
  if (window_title.empty())
  {
    state_->window_title = L"MonoGLXR Desktop Mock";
  }
  else
  {
    state_->window_title = std::move(window_title);
  }
  state_->window_scale = (window_scale <= 0) ? 1 : window_scale;
}

LibXR::ErrorCode Win32MockBackend::Init(const DisplayConfig& config) noexcept
{
  if (state_ == nullptr)
  {
    return LibXR::ErrorCode::INIT_ERR;
  }
  if (state_->hwnd != nullptr)
  {
    return LibXR::ErrorCode::STATE_ERR;
  }
  if (config.width == 0 || config.height == 0)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }

  state_->config = config;
  state_->instance = GetModuleHandleW(nullptr);
  if (state_->instance == nullptr)
  {
    return LibXR::ErrorCode::INIT_ERR;
  }
  if (!register_window_class(state_->instance, WINDOW_CLASS_NAME))
  {
    return LibXR::ErrorCode::INIT_ERR;
  }

  const int WINDOW_SCALE = (state_->window_scale <= 0) ? 1 : state_->window_scale;
  const int CLIENT_WIDTH = static_cast<int>(config.width) * WINDOW_SCALE;
  const int CLIENT_HEIGHT = static_cast<int>(config.height) * WINDOW_SCALE;
  const DWORD WINDOW_STYLE = WS_OVERLAPPEDWINDOW | WS_VISIBLE;

  RECT window_rect{0, 0, CLIENT_WIDTH, CLIENT_HEIGHT};
  if (!AdjustWindowRect(&window_rect, WINDOW_STYLE, FALSE))
  {
    return LibXR::ErrorCode::INIT_ERR;
  }

  state_->hwnd = CreateWindowExW(0,
                                 WINDOW_CLASS_NAME,
                                 state_->window_title.c_str(),
                                 WINDOW_STYLE,
                                 CW_USEDEFAULT,
                                 CW_USEDEFAULT,
                                 window_rect.right - window_rect.left,
                                 window_rect.bottom - window_rect.top,
                                 nullptr,
                                 nullptr,
                                 state_->instance,
                                 state_.get());
  if (state_->hwnd == nullptr)
  {
    return LibXR::ErrorCode::INIT_ERR;
  }

  state_->bitmap_info = {};
  state_->bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  state_->bitmap_info.bmiHeader.biWidth = static_cast<LONG>(config.width);
  state_->bitmap_info.bmiHeader.biHeight = -static_cast<LONG>(config.height);
  state_->bitmap_info.bmiHeader.biPlanes = 1;
  state_->bitmap_info.bmiHeader.biBitCount = 32;
  state_->bitmap_info.bmiHeader.biCompression = BI_RGB;

  const std::size_t PIXEL_COUNT =
      static_cast<std::size_t>(config.width) * static_cast<std::size_t>(config.height);
  state_->rgba_buffer.assign(PIXEL_COUNT, 0x00000000U);

  ShowWindow(state_->hwnd, SW_SHOWDEFAULT);
  UpdateWindow(state_->hwnd);
  return LibXR::ErrorCode::OK;
}

BackendCaps Win32MockBackend::Caps() const noexcept
{
  return BackendCaps{
      false,
      false,
      false,
      false,
  };
}

LibXR::ErrorCode Win32MockBackend::Present(const FrameView& frame, PresentMode mode) noexcept
{
  UNUSED(mode);
  if (state_ == nullptr || state_->hwnd == nullptr)
  {
    return LibXR::ErrorCode::INIT_ERR;
  }
  if (frame.bits == nullptr || frame.width == 0 || frame.height == 0)
  {
    return LibXR::ErrorCode::ARG_ERR;
  }
  if (frame.width != state_->config.width || frame.height != state_->config.height)
  {
    return LibXR::ErrorCode::SIZE_ERR;
  }

  const uint16_t MIN_STRIDE = static_cast<uint16_t>((frame.width + 7U) / 8U);
  const uint16_t STRIDE = (frame.stride_bytes == 0) ? MIN_STRIDE : frame.stride_bytes;
  if (STRIDE < MIN_STRIDE)
  {
    return LibXR::ErrorCode::SIZE_ERR;
  }

  const std::size_t PIXEL_COUNT =
      static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
  if (state_->rgba_buffer.size() != PIXEL_COUNT)
  {
    state_->rgba_buffer.assign(PIXEL_COUNT, 0x00000000U);
  }

  for (uint16_t y = 0; y < frame.height; ++y)
  {
    const uint8_t* row = frame.bits + static_cast<std::size_t>(y) * STRIDE;
    for (uint16_t x = 0; x < frame.width; ++x)
    {
      const uint8_t BYTE = row[x / 8U];
      const uint8_t BIT_MASK = static_cast<uint8_t>(0x80U >> (x & 0x7U));
      const bool PIXEL_ON = (BYTE & BIT_MASK) != 0U;
      state_->rgba_buffer[static_cast<std::size_t>(y) * frame.width + x] =
          PIXEL_ON ? 0x00FFFFFFU : 0x00000000U;
    }
  }

  InvalidateRect(state_->hwnd, nullptr, FALSE);
  UpdateWindow(state_->hwnd);
  return LibXR::ErrorCode::OK;
}

LibXR::ErrorCode Win32MockBackend::SetPowerSave(bool enable) noexcept
{
  UNUSED(enable);
  return LibXR::ErrorCode::NOT_SUPPORT;
}

LibXR::ErrorCode Win32MockBackend::SetContrast(uint8_t value) noexcept
{
  UNUSED(value);
  return LibXR::ErrorCode::NOT_SUPPORT;
}

}  // namespace DesktopMock
}  // namespace MonoGL
}  // namespace LibXR
