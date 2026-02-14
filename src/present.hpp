#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

#include "libxr_def.hpp"
#include "present_types.hpp"

namespace LibXR
{
namespace MonoGL
{

template <typename Backend, std::size_t kFramebufferBytes>
class Present
{
 public:
  static_assert(kFramebufferBytes > 0, "kFramebufferBytes must be greater than 0.");

  // Backend is owned by value. Use a handle-type backend or a correctly movable backend.
  Present(Backend backend, DisplayConfig config)
      : cfg_(config), backend_(std::move(backend))
  {
    if (cfg_.width == 0 || cfg_.height == 0)
    {
      ASSERT(false);
      initialized_ = false;
      return;
    }
    if (cfg_.buffer_mode == BufferMode::PAGE && cfg_.page_rows == 0)
    {
      ASSERT(false);
      initialized_ = false;
      return;
    }

    const std::size_t REQUIRED_BYTES = FramebufferBytes(cfg_);
    if (REQUIRED_BYTES > kFramebufferBytes)
    {
      ASSERT(false);
      initialized_ = false;
      return;
    }

    const LibXR::ErrorCode INIT_STATUS = backend_.Init(cfg_);
    ASSERT(INIT_STATUS == LibXR::ErrorCode::OK);
    if (INIT_STATUS != LibXR::ErrorCode::OK)
    {
      initialized_ = false;
      return;
    }

    caps_ = backend_.Caps();
    draw_buffer_index_ = 0;
    transfer_in_progress_.store(false, std::memory_order_relaxed);
    BindDrawSurface();
    ClearAllBuffers();
    surface_.AddDirtyRect(FullRect(cfg_));
    initialized_ = true;
    in_frame_ = false;
  }

  Present(const Present&) = delete;
  Present& operator=(const Present&) = delete;
  Present(Present&&) = default;
  Present& operator=(Present&&) = default;

  Surface& GetSurface() noexcept { return surface_; }

  const Surface& GetSurface() const noexcept { return surface_; }

  Backend& GetBackend() noexcept { return backend_; }

  const Backend& GetBackend() const noexcept { return backend_; }

  bool IsTransferInProgress() const noexcept
  {
    return transfer_in_progress_.load(std::memory_order_acquire);
  }

  // Call this from DMA/SPI transfer-complete ISR when caps.async_present == true.
  LibXR::ErrorCode OnTransferDone() noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    if (!caps_.async_present)
    {
      return LibXR::ErrorCode::NOT_SUPPORT;
    }

    bool expected = true;
    if (!transfer_in_progress_.compare_exchange_strong(
            expected, false, std::memory_order_acq_rel, std::memory_order_acquire))
    {
      return LibXR::ErrorCode::STATE_ERR;
    }
    return LibXR::ErrorCode::OK;
  }

  LibXR::ErrorCode BeginFrame() noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    if (in_frame_)
    {
      return LibXR::ErrorCode::BUSY;
    }
    in_frame_ = true;
    return LibXR::ErrorCode::OK;
  }

  LibXR::ErrorCode EndFrame() noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    if (!in_frame_)
    {
      return LibXR::ErrorCode::ARG_ERR;
    }
    in_frame_ = false;
    return LibXR::ErrorCode::OK;
  }

  LibXR::ErrorCode PresentFrame(PresentMode mode = PresentMode::AUTO) noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }

    PresentMode resolved = mode;
    if (!caps_.partial_update &&
        (resolved == PresentMode::AUTO || resolved == PresentMode::DIRTY))
    {
      resolved = PresentMode::FULL;
    }

    Rect region{};
    if (resolved == PresentMode::FULL ||
        (resolved == PresentMode::AUTO && !cfg_.enable_dirty_tracking))
    {
      resolved = PresentMode::FULL;
      region = FullRect(cfg_);
    }
    else
    {
      region = ClipToFrame(surface_.GetDirtyRect(), cfg_);
      if (rect_empty(region))
      {
        return LibXR::ErrorCode::OK;
      }
      resolved = PresentMode::DIRTY;
    }

    return SubmitFrame(region, resolved);
  }

  LibXR::ErrorCode PresentFrame(Rect dirty_rect) noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }

    Rect clipped_dirty = ClipToFrame(dirty_rect, cfg_);
    if (rect_empty(clipped_dirty))
    {
      return LibXR::ErrorCode::ARG_ERR;
    }

    PresentMode mode = caps_.partial_update ? PresentMode::DIRTY : PresentMode::FULL;
    if (mode == PresentMode::FULL)
    {
      clipped_dirty = FullRect(cfg_);
    }
    return SubmitFrame(clipped_dirty, mode);
  }

  LibXR::ErrorCode SetRotation(Rotation rotation) noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    cfg_.rotation = rotation;
    surface_.AddDirtyRect(FullRect(cfg_));
    return LibXR::ErrorCode::OK;
  }

  LibXR::ErrorCode SetPowerSave(bool enable) noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    if (!caps_.power_save)
    {
      return LibXR::ErrorCode::NOT_SUPPORT;
    }
    return backend_.SetPowerSave(enable);
  }

  LibXR::ErrorCode SetContrast(uint8_t value) noexcept
  {
    if (!initialized_)
    {
      return LibXR::ErrorCode::INIT_ERR;
    }
    if (!caps_.contrast)
    {
      return LibXR::ErrorCode::NOT_SUPPORT;
    }
    return backend_.SetContrast(value);
  }

 private:
  static Rect FullRect(const DisplayConfig& cfg) noexcept
  {
    return Rect{
        0,
        0,
        cfg.width,
        cfg.height,
    };
  }

  static Rect ClipToFrame(Rect rect, const DisplayConfig& cfg) noexcept
  {
    return intersect_rect(rect, FullRect(cfg));
  }

  static uint16_t StrideBytes(const DisplayConfig& cfg) noexcept
  {
    return static_cast<uint16_t>((cfg.width + 7U) / 8U);
  }

  static std::size_t FramebufferBytes(const DisplayConfig& cfg) noexcept
  {
    return static_cast<std::size_t>(StrideBytes(cfg)) *
           static_cast<std::size_t>(cfg.height);
  }

  void BindDrawSurface() noexcept
  {
    surface_.Bind(framebuffers_[draw_buffer_index_].data(), Size{cfg_.width, cfg_.height},
                  StrideBytes(cfg_));
  }

  void ClearAllBuffers() noexcept
  {
    for (auto& buffer : framebuffers_)
    {
      std::fill(buffer.begin(), buffer.end(), 0U);
    }
    surface_.ClearDirtyRect();
  }

  void CopyRegionBetweenBuffers(uint8_t src_index, uint8_t dst_index,
                                Rect region) noexcept
  {
    const Rect CLIPPED = ClipToFrame(region, cfg_);
    if (rect_empty(CLIPPED))
    {
      return;
    }

    const uint16_t STRIDE = StrideBytes(cfg_);
    const int32_t X_BYTE_START = static_cast<int32_t>(CLIPPED.x) / 8;
    const int32_t X_BYTE_END =
        (static_cast<int32_t>(CLIPPED.x) + static_cast<int32_t>(CLIPPED.w) + 7) / 8;
    const std::size_t COPY_BYTES = static_cast<std::size_t>(X_BYTE_END - X_BYTE_START);
    if (COPY_BYTES == 0U)
    {
      return;
    }

    const int32_t Y_START = static_cast<int32_t>(CLIPPED.y);
    const int32_t Y_END =
        static_cast<int32_t>(CLIPPED.y) + static_cast<int32_t>(CLIPPED.h);
    for (int32_t y = Y_START; y < Y_END; ++y)
    {
      const std::size_t ROW_OFFSET =
          static_cast<std::size_t>(y) * STRIDE + static_cast<std::size_t>(X_BYTE_START);
      std::copy_n(framebuffers_[src_index].begin() + ROW_OFFSET, COPY_BYTES,
                  framebuffers_[dst_index].begin() + ROW_OFFSET);
    }
  }

  void SwapToNextDrawBuffer(Rect sync_region) noexcept
  {
    const uint8_t SUBMITTED = draw_buffer_index_;
    const uint8_t NEXT = static_cast<uint8_t>(SUBMITTED ^ 1U);
    CopyRegionBetweenBuffers(SUBMITTED, NEXT, sync_region);
    draw_buffer_index_ = NEXT;
    BindDrawSurface();
    surface_.ClearDirtyRect();
  }

  LibXR::ErrorCode SubmitFrame(Rect region, PresentMode mode) noexcept
  {
    FrameView frame{
        framebuffers_[draw_buffer_index_].data(),
        cfg_.width,
        cfg_.height,
        StrideBytes(cfg_),
        region,
    };

    if (!caps_.async_present)
    {
      const LibXR::ErrorCode STATUS = backend_.Present(frame, mode);
      if (STATUS == LibXR::ErrorCode::OK)
      {
        surface_.ClearDirtyRect();
      }
      return STATUS;
    }

    if (transfer_in_progress_.load(std::memory_order_acquire))
    {
      return LibXR::ErrorCode::BUSY;
    }

    const LibXR::ErrorCode STATUS = backend_.Present(frame, mode);
    if (STATUS != LibXR::ErrorCode::OK)
    {
      return STATUS;
    }

    transfer_in_progress_.store(true, std::memory_order_release);
    SwapToNextDrawBuffer(region);
    return LibXR::ErrorCode::OK;
  }

 private:
  DisplayConfig cfg_{};
  BackendCaps caps_{};
  Backend backend_;
  // Keep Present in static/global storage when framebuffer is large.
  std::array<std::array<uint8_t, kFramebufferBytes>, 2> framebuffers_{};
  Surface surface_{};
  uint8_t draw_buffer_index_{0};
  std::atomic<bool> transfer_in_progress_{false};
  bool initialized_{false};
  bool in_frame_{false};
};

}  // namespace MonoGL
}  // namespace LibXR
