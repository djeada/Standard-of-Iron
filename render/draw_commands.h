#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

#include "game/accessibility/team_identity.h"
#include "render/decoration_gpu.h"
#include "render/draw_part.h"
#include "render/local_lighting.h"
#include "render/primitive_batch.h"
#include "render/rain_gpu.h"
#include "render/role_color_palette.h"
#include "render/terrain_scene_types.h"
#include "render/world_chunk.h"

namespace Render::GL {
class Mesh;
class RiggedMesh;
class Texture;
class Buffer;
class Shader;
struct Material;
} // namespace Render::GL

namespace Render::GL {

constexpr int k_sort_key_bucket_shift = 56;

constexpr float k_opaque_threshold = 0.999F;

struct MeshCmd {
  Mesh* mesh = nullptr;
  Texture* texture = nullptr;
  QMatrix4x4 model;
  QVector3D color{1, 1, 1};
  QVector3D trim_color{1, 1, 1};
  bool has_trim_color = false;
  float alpha = 1.0F;
  int material_id = 0;
  class Shader* shader = nullptr;

  bool blend_batchable = false;
  CommandPriority priority{CommandPriority::Normal};
};

struct CylinderCmd {
  QVector3D start{0.0F, -0.5F, 0.0F};
  QVector3D end{0.0F, 0.5F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};
  float radius = 1.0F;
  float alpha = 1.0F;
  CommandPriority priority{CommandPriority::Normal};
};

struct FogInstanceData {
  QVector3D center{0.0F, 0.25F, 0.0F};
  float size = 1.0F;
  QVector3D color{0.05F, 0.05F, 0.05F};
  float alpha = 1.0F;
};

struct VisibilityMaskResources {
  Texture* texture = nullptr;
  QVector2D size{0.0F, 0.0F};
  float tile_size = 1.0F;
  float explored_alpha = 0.82F;
  bool enabled = false;
};

struct FogMaskResources {
  Texture* texture = nullptr;
  QVector2D size{0.0F, 0.0F};
  float tile_size = 1.0F;
  bool enabled = false;
};

struct FogBatchCmd {
  const FogInstanceData* instances = nullptr;
  Buffer* instance_buffer = nullptr;
  std::size_t count = 0;
  FogMaskResources mask{};
  CommandPriority priority{CommandPriority::Low};
};

struct TerrainScatterCmd {

  enum class Species : std::uint8_t {
    Grass = 0,
    Stone,
    Plant,
    Pine,
    Olive,
    FireCamp,
    Tent,
    SupplyCart,
    WeaponRack,
    Ruins,
    DeadTree,
    IronOre,
    MagicShrine,
    AbandonedHome,
    Statue
  };

  Species species = Species::Grass;
  const Material* material = nullptr;
  Buffer* instance_buffer = nullptr;
  std::size_t instance_count = 0;

  GrassBatchParams grass{};
  StoneBatchParams stone{};
  FoliageBatchParams foliage{};
  FireCampBatchParams firecamp{};
  PropBatchParams prop{};

  VisibilityMaskResources visibility{};
  CommandPriority priority{CommandPriority::Low};
};

struct RainBatchCmd {
  RainBatchParams params;
  CommandPriority priority{CommandPriority::Low};
};

struct TerrainSurfaceCmd {
  struct HeightResources {
    Texture* texture = nullptr;
    Texture* field_texture = nullptr;
    unsigned int noise_atlas = 0U;
    unsigned int noise_atlas_detail = 0U;
    unsigned int microdetail = 0U;
    QVector2D noise_atlas_world_size{0.0F, 0.0F};
    QVector2D texel_size{0.0F, 0.0F};
    QVector2D uv_scale{0.0F, 0.0F};
    QVector2D uv_offset{0.5F, 0.5F};
    float to_world = 1.0F;
    bool enabled = false;
  };

  using VisibilityResources = VisibilityMaskResources;

  Mesh* mesh = nullptr;
  const Material* material = nullptr;
  QMatrix4x4 model;
  BoundingBox aabb;
  TerrainChunkParams params;
  HeightResources height{};
  VisibilityResources visibility{};
  std::uint16_t sort_key = 0x8000U;
  bool depth_write = true;
  bool wireframe = false;

  bool horizon_dressing = false;
  float depth_bias = 0.0F;
  CommandPriority priority{CommandPriority::High};
};

struct TerrainFeatureCmd {
  Mesh* mesh = nullptr;
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};

  QVector3D biome_grass_secondary{0.44F, 0.70F, 0.32F};
  QVector3D biome_grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D biome_soil_color{0.28F, 0.24F, 0.18F};
  QVector3D biome_rock_low{0.48F, 0.46F, 0.44F};
  QVector3D biome_rock_high{0.68F, 0.69F, 0.73F};
  QVector3D biome_snow_color{0.92F, 0.94F, 0.98F};
  float biome_moisture = 0.5F;
  float biome_rock_exposure = 0.3F;
  float biome_snow_coverage = 0.0F;
  int biome_ground_type = 0;

  float ambient_boost = TerrainChunkParams::k_default_ambient_boost;
  float alpha = 1.0F;
  LinearFeatureKind kind = LinearFeatureKind::Water;
  WaterSurfaceKind water_kind = WaterSurfaceKind::River;
  RoadSurfaceKind road_surface_kind = RoadSurfaceKind::PackedEarth;
  TerrainSurfaceCmd::VisibilityResources visibility{};
  CommandPriority priority{CommandPriority::High};
};

struct GridCmd {

  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0.2F, 0.25F, 0.2F};
  float cell_size = 1.0F;
  float thickness = 0.06F;
  float extent = 50.0F;
  CommandPriority priority{CommandPriority::Low};
};

struct GroundMarkerCmd {
  QVector3D center{0.0F, 0.0F, 0.0F};
  float outer_radius = 1.0F;
  float thickness = 0.06F;
  QVector3D color{0.0F, 0.0F, 0.0F};
  float alpha = 0.6F;
  float phase = 0.0F;

  Game::Accessibility::TeamPattern pattern{Game::Accessibility::TeamPattern::Solid};
  bool focused = false;
  TerrainSurfaceCmd::HeightResources height{};
  CommandPriority priority{CommandPriority::Critical};
};

struct SelectionSmokeCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float base_alpha = 0.15F;
  CommandPriority priority{CommandPriority::Critical};
};

struct EffectBatchCmd {

  enum class Kind : std::uint8_t {
    HealingBeam = 0,
    HealerAura,
    CombatDust,
    BuildingFlame,
    BurningFlame,
    Fireball,
    BloodPool,
    StoneImpact,
    MetalSpark
  };

  Kind kind = Kind::HealerAura;

  QVector3D position{0.0F, 0.0F, 0.0F};
  QVector3D end_pos{0.0F, 0.0F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};

  float radius = 1.0F;
  float intensity = 1.0F;
  float time = 0.0F;
  float alpha_scale = 1.0F;
  float rotation = 0.0F;
  float aspect_ratio = 1.0F;
  float seed = 0.0F;

  float progress = 1.0F;
  float beam_width = 0.15F;

  QVector3D direction{0.0F, 0.0F, 0.0F};

  CommandPriority priority{CommandPriority::Normal};
};

struct ModeIndicatorCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  int mode_type = 0;
  CommandPriority priority{CommandPriority::Critical};
};

struct RiggedCreatureCmd {
  static constexpr std::size_t k_max_role_colors = Render::RoleColorPalette::k_capacity;
  static constexpr std::size_t k_max_owned_bones = 64;

  RiggedMesh* mesh = nullptr;

  RiggedMesh* shadow_mesh = nullptr;
  const Material* material = nullptr;
  QMatrix4x4 world;

  const QMatrix4x4* bone_palette = nullptr;
  const QMatrix4x4* bone_palette_next = nullptr;
  std::shared_ptr<std::array<QMatrix4x4, k_max_owned_bones>> owned_bone_palette{};

  std::uint32_t palette_ubo = 0;
  std::uint32_t palette_offset = 0;
  std::uint32_t palette_next_offset = 0;
  float palette_lerp = 0.0F;
  bool palette_frames_resident = false;
  std::uint32_t bone_count = 0;
  std::shared_ptr<const Render::RoleColorPalette> role_colors{};
  std::uint32_t role_color_count = 0;
  QVector3D color{1.0F, 1.0F, 1.0F};
  QVector4D wear_params{0.0F, 0.0F, 0.0F, 0.0F};
  float alpha = 1.0F;
  Texture* texture = nullptr;
  std::int32_t material_id = 0;
  QVector3D variation_scale{1.0F, 1.0F, 1.0F};
  CommandPriority priority{CommandPriority::Normal};
};

using DrawCmd = std::variant<GridCmd,
                             GroundMarkerCmd,
                             SelectionSmokeCmd,
                             CylinderCmd,
                             MeshCmd,
                             FogBatchCmd,
                             TerrainScatterCmd,
                             RainBatchCmd,
                             TerrainSurfaceCmd,
                             TerrainFeatureCmd,
                             PrimitiveBatchCmd,
                             EffectBatchCmd,
                             ModeIndicatorCmd,
                             DrawPartCmd,
                             RiggedCreatureCmd>;

[[nodiscard]] inline constexpr auto
scatter_species_is_blended(TerrainScatterCmd::Species species) noexcept -> bool {
  return species == TerrainScatterCmd::Species::FireCamp;
}

} // namespace Render::GL
