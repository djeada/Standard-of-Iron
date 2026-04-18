#include "bone_palette_arena.h"

#include "gl/ubo_bindings.h"

#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>
#include <algorithm>
#include <cstring>

namespace Render::GL {

namespace {

auto gl_funcs() -> QOpenGLFunctions_3_3_Core * {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) {
    return nullptr;
  }
  return QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
}

} // namespace

struct BonePaletteArena::Slab {
  // Slab CPU storage is one big contiguous mat4 array, laid out as
  // [slot0_bone0..slot0_bone63][slot1_bone0..slot1_bone63]... so a
  // `glBindBufferRange(offset = slot_idx * 4096, size = 4096)` reads a
  // single palette and a future instanced bind reads N palettes.
  std::array<QMatrix4x4, kSlotsPerSlab * kPaletteWidth> data{};
  GLuint ubo = 0;
  std::size_t high_water_slot = 0; // slots written this frame
  bool dirty = false;
  // Owns its UBO so destruction reclaims it. We can't use unique_ptr
  // because we need the GL context current; explicit cleanup below.
  ~Slab() {
    if (ubo != 0) {
      auto *fn = gl_funcs();
      if (fn != nullptr) {
        fn->glDeleteBuffers(1, &ubo);
      }
      // No GL context → leak intentionally (driver reaps on context teardown).
    }
  }
};

BonePaletteArena::BonePaletteArena() = default;
BonePaletteArena::~BonePaletteArena() = default;

void BonePaletteArena::reset_frame() noexcept {
  m_used_slots = 0;
  for (auto &slab : m_slabs) {
    slab->high_water_slot = 0;
    slab->dirty = false;
  }
}

auto BonePaletteArena::ensure_slab(std::size_t index) -> Slab & {
  while (m_slabs.size() <= index) {
    m_slabs.emplace_back(std::make_unique<Slab>());
  }
  return *m_slabs[index];
}

auto BonePaletteArena::allocate_palette() -> BonePaletteSlot {
  std::size_t const slab_idx = m_used_slots / kSlotsPerSlab;
  std::size_t const intra_idx = m_used_slots % kSlotsPerSlab;
  Slab &slab = ensure_slab(slab_idx);

  // Lazily create the slab's UBO on first allocation when a GL context
  // is current. Submitters always run on the render thread, so this is
  // expected to succeed in production; headless tests get ubo == 0
  // and the pipeline silently skips the bind.
  if (slab.ubo == 0) {
    auto *fn = gl_funcs();
    if (fn != nullptr) {
      fn->glGenBuffers(1, &slab.ubo);
      fn->glBindBuffer(GL_UNIFORM_BUFFER, slab.ubo);
      fn->glBufferData(GL_UNIFORM_BUFFER,
                       static_cast<GLsizeiptr>(kSlotsPerSlab * kPaletteBytes),
                       nullptr, GL_DYNAMIC_DRAW);
      fn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
  }

  QMatrix4x4 *base = slab.data.data() + intra_idx * kPaletteWidth;
  for (std::size_t i = 0; i < kPaletteWidth; ++i) {
    base[i].setToIdentity();
  }
  slab.high_water_slot = std::max(slab.high_water_slot, intra_idx + 1);
  slab.dirty = true;
  ++m_used_slots;

  BonePaletteSlot slot{};
  slot.cpu = base;
  slot.ubo = slab.ubo;
  slot.offset = static_cast<GLintptr>(intra_idx * kPaletteBytes);
  return slot;
}

void BonePaletteArena::flush_to_gpu() {
  if (m_used_slots == 0) {
    return;
  }
  auto *fn = gl_funcs();
  if (fn == nullptr) {
    return; // headless: leave CPU data only
  }

  for (auto &slab_ptr : m_slabs) {
    Slab &slab = *slab_ptr;
    if (!slab.dirty) {
      continue;
    }
    if (slab.ubo == 0) {
      // GL context appeared after allocation. Create now and upload.
      fn->glGenBuffers(1, &slab.ubo);
      fn->glBindBuffer(GL_UNIFORM_BUFFER, slab.ubo);
      fn->glBufferData(GL_UNIFORM_BUFFER,
                       static_cast<GLsizeiptr>(kSlotsPerSlab * kPaletteBytes),
                       nullptr, GL_DYNAMIC_DRAW);
    } else {
      fn->glBindBuffer(GL_UNIFORM_BUFFER, slab.ubo);
    }
    GLsizeiptr const upload_bytes =
        static_cast<GLsizeiptr>(slab.high_water_slot * kPaletteBytes);
    fn->glBufferSubData(GL_UNIFORM_BUFFER, 0, upload_bytes, slab.data.data());
    slab.dirty = false;
  }

  fn->glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

auto BonePaletteArena::capacity_slabs() const noexcept -> std::size_t {
  return m_slabs.size();
}

auto BonePaletteArena::pending_upload_bytes() const noexcept -> std::size_t {
  std::size_t total = 0;
  for (const auto &slab : m_slabs) {
    if (slab->dirty) {
      total += slab->high_water_slot * kPaletteBytes;
    }
  }
  return total;
}

} // namespace Render::GL
