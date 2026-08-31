#include "asset_counters.h"

#include <array>
#include <cstdio>

namespace Render::Profiling {

namespace {

constexpr std::array<std::string_view, AssetCounters::k_count> k_names{
    "rigged_mesh_bakes",
    "snapshot_mesh_bakes",
    "rigged_meshes_constructed",
    "rigged_mesh_vertex_bytes",
    "rigged_cache_hits",
    "rigged_cache_misses",
    "snapshot_cache_hits",
    "snapshot_cache_misses",
    "snapshot_loads",
    "snapshot_evictions",
    "skin_atlas_builds",
    "skin_ubo_uploads",
    "skin_ubo_bytes",
    "forbidden_bakes",
    "missing_preloaded_assets",
    "gl_buffers_created",
    "gl_vertex_arrays_created",
    "gl_textures_created",
    "gl_programs_created",
    "shaders_compiled",
    "programs_linked",
    "gl_upload_bytes",
    "render_thread_allocations",
    "render_thread_allocated_bytes",
    "prewarm_items_queued",
    "prewarm_items_processed",
    "prewarm_invocations",
    "prewarm_skipped_no_registry"};

constexpr std::array k_forbidden_after_barrier{AssetCounter::RiggedMeshBake,
                                               AssetCounter::SnapshotMeshBake,
                                               AssetCounter::RiggedMeshConstructed,
                                               AssetCounter::ForbiddenBake,
                                               AssetCounter::MissingPreloadedAsset,
                                               AssetCounter::GlBufferCreated,
                                               AssetCounter::GlVertexArrayCreated,
                                               AssetCounter::GlTextureCreated,
                                               AssetCounter::GlProgramCreated,
                                               AssetCounter::ShaderCompiled,
                                               AssetCounter::ProgramLinked};

} // namespace

auto asset_counter_name(AssetCounter counter) noexcept -> std::string_view {
  const auto index = static_cast<std::size_t>(counter);
  if (index >= k_names.size()) {
    return "unknown";
  }
  return k_names[index];
}

void AssetCounters::mark_load_barrier() noexcept {
  for (std::size_t i = 0; i < k_count; ++i) {
    m_at_barrier[i].store(m_total[i].load(std::memory_order_relaxed),
                          std::memory_order_relaxed);
  }
  m_barrier_marked.store(true, std::memory_order_release);
}

void AssetCounters::clear_load_barrier() noexcept {
  m_barrier_marked.store(false, std::memory_order_release);
}

void AssetCounters::reset() noexcept {
  for (std::size_t i = 0; i < k_count; ++i) {
    m_total[i].store(0, std::memory_order_relaxed);
    m_at_barrier[i].store(0, std::memory_order_relaxed);
  }
  m_barrier_marked.store(false, std::memory_order_release);
}

auto AssetCounters::post_barrier_asset_work() const noexcept -> std::uint64_t {
  if (!load_barrier_marked()) {
    return 0U;
  }
  std::uint64_t total = 0U;
  for (AssetCounter counter : k_forbidden_after_barrier) {
    total += since_barrier(counter);
  }
  return total;
}

auto asset_counters() noexcept -> AssetCounters& {
  static AssetCounters g_counters;
  return g_counters;
}

auto format_post_barrier_violations() -> std::string {
  const AssetCounters& counters = asset_counters();
  std::string out;
  if (!counters.load_barrier_marked()) {
    return out;
  }
  char line[128];
  for (AssetCounter counter : k_forbidden_after_barrier) {
    const std::uint64_t value = counters.since_barrier(counter);
    if (value == 0U) {
      continue;
    }
    const std::string_view name = asset_counter_name(counter);
    std::snprintf(line,
                  sizeof(line),
                  "%s%.*s=%llu",
                  out.empty() ? "" : " ",
                  static_cast<int>(name.size()),
                  name.data(),
                  static_cast<unsigned long long>(value));
    out += line;
  }
  return out;
}

} // namespace Render::Profiling
