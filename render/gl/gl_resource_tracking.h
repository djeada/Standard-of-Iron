#pragma once

#include <cstddef>
#include <cstdint>

#include "render/profiling/asset_counters.h"
#include "render/profiling/gl_creation_trace.h"

namespace Render::GL {

inline void note_buffers_created(std::size_t count = 1) noexcept {
  if (count == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlBufferCreated, count);
  Profiling::record_gl_creation("buffer", count);
}

inline void note_vertex_arrays_created(std::size_t count = 1) noexcept {
  if (count == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlVertexArrayCreated, count);
  Profiling::record_gl_creation("vertex_array", count);
}

inline void note_textures_created(std::size_t count = 1) noexcept {
  if (count == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlTextureCreated, count);
  Profiling::record_gl_creation("texture", count);
}

inline void note_buffer_transfer(std::size_t bytes) noexcept {
  if (bytes == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlBufferTransferBytes, bytes);
  Profiling::count_asset(Profiling::AssetCounter::GlUploadBytes, bytes);
}

inline void note_texture_transfer(std::size_t bytes) noexcept {
  if (bytes == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlTextureTransferBytes, bytes);
  Profiling::count_asset(Profiling::AssetCounter::GlUploadBytes, bytes);
}

inline void note_texture_storage(std::size_t bytes, bool has_data) noexcept {
  Profiling::count_asset(Profiling::AssetCounter::GlTextureStorageBytes, bytes);
  if (has_data) {
    note_texture_transfer(bytes);
  }
}

inline void note_buffer_storage(std::size_t bytes, bool has_data) noexcept {
  Profiling::count_asset(Profiling::AssetCounter::GlBufferStorageBytes, bytes);
  if (has_data) {
    note_buffer_transfer(bytes);
  } else {
    Profiling::count_asset(Profiling::AssetCounter::GlBufferOrphans);
  }
}

inline void note_mapped_buffer_range(std::size_t bytes) noexcept {
  if (bytes == 0) {
    return;
  }
  Profiling::count_asset(Profiling::AssetCounter::GlMappedBufferBytes, bytes);
}

[[nodiscard]] inline auto
texture_transfer_bytes(std::size_t width,
                       std::size_t height,
                       std::size_t bytes_per_texel) noexcept -> std::size_t {
  return width * height * bytes_per_texel;
}

} // namespace Render::GL
