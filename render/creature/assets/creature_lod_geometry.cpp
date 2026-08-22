#include "creature_lod_geometry.h"

#include <atomic>

#include "render/creature/runtime_bake_guard.h"

namespace Render::Creature {

namespace {

std::atomic<std::uint64_t> g_compile_count{0};

} // namespace

auto compile_creature_lod_geometry(const CreatureRuntimeManifest& manifest)
    -> CreatureLodGeometry {
  g_compile_count.fetch_add(1, std::memory_order_relaxed);
  return {compile_whole_mesh_lod(manifest.lod_minimal),
          compile_whole_mesh_lod(manifest.lod_full)};
}

auto creature_lod_geometry_compile_count() noexcept -> std::uint64_t {
  return g_compile_count.load(std::memory_order_relaxed);
}

void CreatureLodGeometrySlot::initialize() {
  if (m_initialized || m_manifest == nullptr) {
    return;
  }
  m_geometry = compile_creature_lod_geometry(m_manifest());
  m_initialized = true;
}

auto CreatureLodGeometrySlot::get() -> const CreatureLodGeometry& {
  if (!m_initialized) {
    report_missing_preloaded_asset(std::string("lod geometry not initialized: ") +
                                   std::string(m_species));
    initialize();
  }
  return m_geometry;
}

} // namespace Render::Creature
