

#pragma once

#include <QMatrix4x4>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>

using GLuint = unsigned int;
using GLintptr = std::intptr_t;

namespace Render::GL {

struct BonePaletteSlot {
  QMatrix4x4 *cpu = nullptr;
  GLuint ubo = 0;
  GLintptr offset = 0;
};

class BonePaletteArena {
public:
  static constexpr std::size_t kPaletteWidth = 64;
  static constexpr std::size_t kMatrixFloats = 16;
  static constexpr std::size_t kPaletteFloats = kPaletteWidth * kMatrixFloats;
  static constexpr std::size_t kPaletteBytes = kPaletteFloats * sizeof(float);
  static constexpr std::size_t kSlotsPerSlab = 64;

  static void pack_palette_for_gpu(const QMatrix4x4 *src, float *dst) noexcept;

  BonePaletteArena();
  ~BonePaletteArena();

  BonePaletteArena(const BonePaletteArena &) = delete;
  auto operator=(const BonePaletteArena &) -> BonePaletteArena & = delete;

  void reset_frame() noexcept;

  [[nodiscard]] auto allocate_palette() -> BonePaletteSlot;

  void flush_to_gpu();

  [[nodiscard]] auto allocations_in_flight() const noexcept -> std::size_t {
    return m_used_slots;
  }
  [[nodiscard]] auto capacity_slabs() const noexcept -> std::size_t;

  [[nodiscard]] auto pending_upload_bytes() const noexcept -> std::size_t;

private:
  struct Slab;
  std::deque<std::unique_ptr<Slab>> m_slabs;
  std::size_t m_used_slots{0};

  auto ensure_slab(std::size_t index) -> Slab &;
};

} // namespace Render::GL
