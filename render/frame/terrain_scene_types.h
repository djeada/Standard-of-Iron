#pragma once

#include <cstddef>
#include <cstdint>

#include "game/map/biome_settings.h"
#include "game/map/terrain.h"
#include "render/frame/i_draw_source.h"

namespace Render::Ground::Scatter {

struct SyncStats {
  std::uint32_t visibility_rebuilds = 0;
  std::uint32_t buffer_uploads = 0;
  std::uint32_t buffer_resets = 0;
  std::uint32_t chunks_rescanned = 0;

  [[nodiscard]] auto did_upload_or_rebuild() const noexcept -> bool {
    return visibility_rebuilds != 0U || buffer_uploads != 0U || buffer_resets != 0U;
  }

  auto operator+=(const SyncStats& other) noexcept -> SyncStats& {
    visibility_rebuilds += other.visibility_rebuilds;
    buffer_uploads += other.buffer_uploads;
    buffer_resets += other.buffer_resets;
    chunks_rescanned += other.chunks_rescanned;
    return *this;
  }
};

} // namespace Render::Ground::Scatter

namespace Render::GL {

enum class TerrainSurfaceKind {
  GroundPlane,
  TerrainMesh
};

struct TerrainSurfaceShaderParams {
  const Game::Map::BiomeSettings* biome_settings = nullptr;
  const Game::Map::TerrainField* field = nullptr;
  bool is_ground_plane = false;
};

struct TerrainSurfaceChunk {
  TerrainSurfaceKind kind = TerrainSurfaceKind::GroundPlane;
  IDrawSource* source = nullptr;
  TerrainSurfaceShaderParams params;
};

enum class LinearFeatureKind {
  Water,
  Road,
  Shoreline,
  Bridge
};

enum class WaterSurfaceKind : std::uint8_t {
  River = 0,
  Lake = 1,
};

enum class RoadSurfaceKind : std::uint8_t {
  PackedEarth = 0,
  RoughTrack = 1,
  Paved = 2,
};

enum class LinearFeatureVisibilityMode {
  None,
  SegmentSampled,
  TextureDriven
};

struct LinearFeatureChunk {
  LinearFeatureKind kind = LinearFeatureKind::Road;
  LinearFeatureVisibilityMode visibility_mode = LinearFeatureVisibilityMode::None;
  IDrawSource* source = nullptr;
  std::size_t geometry_count = 0;
};

enum class ScatterSpeciesId {
  Grass,
  Stone,
  Plant,
  Pine,
  Olive,
  Cypress,
  Palm,
  FireCamp,
  Tent,
  SupplyCart,
  WeaponRack,
  Ruins,
  DeadTree,
  Boulder,
  IronOre,
  MagicShrine,
  AbandonedHome,
  Statue,
  CursedGoldVein
};

[[nodiscard]] constexpr auto scatter_species_name(ScatterSpeciesId species) -> const
    char* {
  switch (species) {
  case ScatterSpeciesId::Grass:
    return "grass";
  case ScatterSpeciesId::Stone:
    return "stone";
  case ScatterSpeciesId::Plant:
    return "plant";
  case ScatterSpeciesId::Pine:
    return "pine";
  case ScatterSpeciesId::Olive:
    return "olive";
  case ScatterSpeciesId::Cypress:
    return "cypress";
  case ScatterSpeciesId::Palm:
    return "palm";
  case ScatterSpeciesId::FireCamp:
    return "firecamp";
  case ScatterSpeciesId::Tent:
    return "tent";
  case ScatterSpeciesId::SupplyCart:
    return "supply_cart";
  case ScatterSpeciesId::WeaponRack:
    return "weapon_rack";
  case ScatterSpeciesId::Ruins:
    return "ruins";
  case ScatterSpeciesId::DeadTree:
    return "dead_tree";
  case ScatterSpeciesId::Boulder:
    return "boulder";
  case ScatterSpeciesId::IronOre:
    return "iron_ore";
  case ScatterSpeciesId::MagicShrine:
    return "magic_shrine";
  case ScatterSpeciesId::AbandonedHome:
    return "abandoned_home";
  case ScatterSpeciesId::Statue:
    return "statue";
  case ScatterSpeciesId::CursedGoldVein:
    return "cursed_gold_vein";
  }
  return "unknown";
}

enum class ScatterVisibilityMode {
  None,
  InstanceFiltered
};

struct ScatterChunk {
  ScatterSpeciesId species = ScatterSpeciesId::Grass;
  ScatterVisibilityMode visibility_mode = ScatterVisibilityMode::None;
  IDrawSource* source = nullptr;
  std::size_t instance_count = 0;
  bool gpu_ready = false;
  Render::Ground::Scatter::SyncStats last_sync_stats{};
};

} // namespace Render::GL
