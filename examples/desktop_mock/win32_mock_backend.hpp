#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "libxr_def.hpp"
#include "present_types.hpp"

namespace LibXR
{
namespace MonoGL
{
namespace DesktopMock
{

struct Win32MockBackendState;

class Win32MockBackend
{
 public:
  Win32MockBackend();
  explicit Win32MockBackend(std::wstring window_title, int window_scale = 6) noexcept;

  LibXR::ErrorCode Init(const DisplayConfig& config) noexcept;
  BackendCaps Caps() const noexcept;
  LibXR::ErrorCode Present(const FrameView& frame, PresentMode mode) noexcept;
  LibXR::ErrorCode SetPowerSave(bool enable) noexcept;
  LibXR::ErrorCode SetContrast(uint8_t value) noexcept;

 private:
  static constexpr const wchar_t* WINDOW_CLASS_NAME = L"MonoGLXRDesktopMockWindow";

  std::shared_ptr<Win32MockBackendState> state_;
};

}  // namespace DesktopMock
}  // namespace MonoGL
}  // namespace LibXR

