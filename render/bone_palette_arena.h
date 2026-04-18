// Stage 15.5c / 16.2 — per-frame bone-palette arena (UBO-backed).
//
// Each RiggedCreatureCmd needs a 64-mat4 bone palette accessible to the
// vertex shader. The arena gives the submit phase pointer-stable CPU
// scratch storage AND a pre-allocated GL_UNIFORM_BUFFER slice the
// pipeline can `glBindBufferRange` into without per-draw uploads.
//
// Layout: a deque of fixed-size slabs, each slab owning
//   * `kSlotsPerSlab` palettes worth of contiguous CPU mat4 storage, and
//   * one GL_UNIFORM_BUFFER object sized `kSlotsPerSlab * 4096` bytes.
//
// `allocate_palette` returns a `BonePaletteSlot` carrying the CPU
// pointer (caller writes into it during submit), the slab's UBO
// handle, and the byte offset to bind. After the queue is sorted but
// before it is executed, `flush_to_gpu` pushes every dirty slab's CPU
// data to its UBO via `glBufferSubData`. UBOs are reused frame to
// frame; the deque only grows.
//
// Pointer stability is preserved by the deque: pushing new slabs never
// invalidates earlier ones. Headless (no GL context) callers still get
// valid CPU pointers; `ubo == 0` signals "no GPU upload happened" so
// tests and the software backend can introspect palettes from the CPU
// pointer alone.

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
  QMatrix4x4 *cpu = nullptr; // kPaletteWidth contiguous mat4 entries
  GLuint ubo = 0;            // 0 when no GL context is current
  GLintptr offset = 0;       // byte offset into `ubo`
};

class BonePaletteArena {
public:
  // Width of one palette. Matches `BonePalette { mat4 bones[64]; }` in
  // character_skinned.vert.
  static constexpr std::size_t kPaletteWidth = 64;
  static constexpr std::size_t kPaletteBytes =
      kPaletteWidth * sizeof(float) * 16;
  static constexpr std::size_t kSlotsPerSlab = 64; // 64 * 4 KiB = 256 KiB

  BonePaletteArena();
  ~BonePaletteArena();

  BonePaletteArena(const BonePaletteArena &) = delete;
  auto operator=(const BonePaletteArena &) -> BonePaletteArena & = delete;

  // Reset the per-frame allocation cursor. CPU storage and UBO objects
  // are kept alive — only the "in flight" count is rewound. All
  // previously-handed-out `BonePaletteSlot`s become invalid.
  void reset_frame() noexcept;

  // Allocate one identity-initialised palette slot. Pointer stable
  // until the next `reset_frame()`.
  [[nodiscard]] auto allocate_palette() -> BonePaletteSlot;

  // Push every slab touched this frame to its GPU buffer. No-op when
  // no GL context is current. Safe to call from end_frame() before the
  // backend executes the draw queue.
  void flush_to_gpu();

  [[nodiscard]] auto allocations_in_flight() const noexcept -> std::size_t {
    return m_used_slots;
  }
  [[nodiscard]] auto capacity_slabs() const noexcept -> std::size_t;

  // Test hook: total UBO bytes that would be uploaded if flushed now.
  [[nodiscard]] auto pending_upload_bytes() const noexcept -> std::size_t;

private:
  struct Slab;
  std::deque<std::unique_ptr<Slab>> m_slabs;
  std::size_t m_used_slots{0}; // monotonic across slabs; reset per frame

  auto ensure_slab(std::size_t index) -> Slab &;
};

} // namespace Render::GL
