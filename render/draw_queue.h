#pragma once

#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "decoration_gpu.h"
#include "draw_part.h"
#include "frame_budget.h"
#include "primitive_batch.h"
#include "rain_gpu.h"
#include "terrain_scene_types.h"
#include "world_chunk.h"

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

struct FogBatchCmd {
  const FogInstanceData* instances = nullptr;
  Buffer* instance_buffer = nullptr;
  std::size_t count = 0;
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
    MagicShrine
  };

  Species species = Species::Grass;
  const Material* material = nullptr;
  Buffer* instance_buffer = nullptr;
  std::size_t instance_count = 0;

  GrassBatchParams grass{};
  StoneBatchParams stone{};
  PlantBatchParams plant{};
  PineBatchParams pine{};
  OliveBatchParams olive{};
  FireCampBatchParams firecamp{};
  TentBatchParams tent{};
  SupplyCartBatchParams supply_cart{};
  WeaponRackBatchParams weapon_rack{};
  RuinsBatchParams ruins{};
  DeadTreeBatchParams dead_tree{};
  IronOreBatchParams iron_ore{};
  MagicShrineBatchParams magic_shrine{};

  CommandPriority priority{CommandPriority::Low};
};

struct RainBatchCmd {
  Buffer* instance_buffer = nullptr;
  std::size_t instance_count = 0;
  RainBatchParams params;
  CommandPriority priority{CommandPriority::Low};
};

struct TerrainSurfaceCmd {
  struct VisibilityResources {
    Texture* texture = nullptr;
    QVector2D size{0.0F, 0.0F};
    float tile_size = 1.0F;
    float explored_alpha = 0.6F;
    bool enabled = false;
  };

  Mesh* mesh = nullptr;
  const Material* material = nullptr;
  QMatrix4x4 model;
  BoundingBox aabb;
  TerrainChunkParams params;
  VisibilityResources visibility{};
  std::uint16_t sort_key = 0x8000U;
  bool depth_write = true;
  bool wireframe = false;
  float depth_bias = 0.0F;
  CommandPriority priority{CommandPriority::High};
};

struct TerrainFeatureCmd {
  Mesh* mesh = nullptr;
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  // Shorelines need the authored terrain palette, not just a single tint, to
  // carry muddy, dry, rocky, and alpine biomes through to the water's edge.
  QVector3D biome_grass_secondary{0.44F, 0.70F, 0.32F};
  QVector3D biome_grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D biome_soil_color{0.28F, 0.24F, 0.18F};
  QVector3D biome_rock_low{0.48F, 0.46F, 0.44F};
  QVector3D biome_rock_high{0.68F, 0.69F, 0.73F};
  QVector3D biome_snow_color{0.92F, 0.94F, 0.98F};
  float biome_moisture = 0.5F;
  float biome_rock_exposure = 0.3F;
  float biome_snow_coverage = 0.0F;
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

struct SelectionRingCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0, 0, 0};
  float alpha_inner = 0.6F;
  float alpha_outer = 0.25F;
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
  static constexpr std::size_t k_max_role_colors = 32;
  static constexpr std::size_t k_max_owned_bones = 64;

  RiggedMesh* mesh = nullptr;
  const Material* material = nullptr;
  QMatrix4x4 world;

  const QMatrix4x4* bone_palette = nullptr;
  std::shared_ptr<std::array<QMatrix4x4, k_max_owned_bones>> owned_bone_palette{};

  std::uint32_t palette_ubo = 0;
  std::uint32_t palette_offset = 0;
  std::uint32_t bone_count = 0;
  std::array<QVector3D, k_max_role_colors> role_colors{};
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
                             SelectionRingCmd,
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

enum class DrawCmdType : std::uint8_t {
  Grid = 0,
  SelectionRing = 1,
  SelectionSmoke = 2,
  Cylinder = 3,
  Mesh = 4,
  FogBatch = 5,
  TerrainScatter = 6,
  RainBatch = 7,
  TerrainSurface = 8,
  TerrainFeature = 9,
  PrimitiveBatch = 10,
  EffectBatch = 11,
  ModeIndicator = 12,
  DrawPart = 13,
  RiggedCreature = 14
};

constexpr std::size_t MeshCmdIndex = static_cast<std::size_t>(DrawCmdType::Mesh);
constexpr std::size_t GridCmdIndex = static_cast<std::size_t>(DrawCmdType::Grid);
constexpr std::size_t SelectionRingCmdIndex =
    static_cast<std::size_t>(DrawCmdType::SelectionRing);
constexpr std::size_t SelectionSmokeCmdIndex =
    static_cast<std::size_t>(DrawCmdType::SelectionSmoke);
constexpr std::size_t CylinderCmdIndex =
    static_cast<std::size_t>(DrawCmdType::Cylinder);
constexpr std::size_t FogBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::FogBatch);
constexpr std::size_t TerrainScatterCmdIndex =
    static_cast<std::size_t>(DrawCmdType::TerrainScatter);
constexpr std::size_t RainBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::RainBatch);
constexpr std::size_t TerrainSurfaceCmdIndex =
    static_cast<std::size_t>(DrawCmdType::TerrainSurface);
constexpr std::size_t TerrainFeatureCmdIndex =
    static_cast<std::size_t>(DrawCmdType::TerrainFeature);
constexpr std::size_t PrimitiveBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::PrimitiveBatch);
constexpr std::size_t EffectBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::EffectBatch);
constexpr std::size_t ModeIndicatorCmdIndex =
    static_cast<std::size_t>(DrawCmdType::ModeIndicator);
constexpr std::size_t DrawPartCmdIndex =
    static_cast<std::size_t>(DrawCmdType::DrawPart);
constexpr std::size_t RiggedCreatureCmdIndex =
    static_cast<std::size_t>(DrawCmdType::RiggedCreature);

inline auto draw_cmd_type(const DrawCmd& cmd) -> DrawCmdType {
  return static_cast<DrawCmdType>(cmd.index());
}

inline auto extract_cmd_priority(const DrawCmd& cmd) -> CommandPriority {
  return std::visit([](const auto& c) -> CommandPriority { return c.priority; }, cmd);
}

enum class PreparedBatchKind : std::uint8_t {
  Single,
  CylinderInstanced,
  MeshInstanced,
  DrawPartInstanced,
  RiggedCreatureInstanced,
  SelectionRingInstanced,
  EffectInstanced,
  ModeIndicatorInstanced
};

struct PreparedBatch {
  std::size_t start = 0;
  std::size_t count = 0;
  DrawCmdType type = DrawCmdType::Grid;
  PreparedBatchKind kind = PreparedBatchKind::Single;
  std::uint64_t sort_key = 0;

  [[nodiscard]] auto end() const noexcept -> std::size_t { return start + count; }

  [[nodiscard]] auto is_instanced() const noexcept -> bool {
    return kind != PreparedBatchKind::Single;
  }
};

class DrawQueue {
public:
  void clear() {
    m_items_high_water = std::max(m_items_high_water, m_items.size());
    m_prepared_high_water = std::max(m_prepared_high_water, m_prepared_batches.size());
    m_submission_bucket_high_water =
        std::max(m_submission_bucket_high_water, m_submission_bucket_spans.size());
    m_items.clear();
    m_sort_indices.clear();
    m_sort_keys.clear();
    m_prepared_batches.clear();
    m_submission_bucket_spans.clear();
    m_submission_bucket_ordered = true;
  }

  void reserve_for_frame(std::size_t items_hint = 0) {
    const std::size_t target = std::max(items_hint, m_items_high_water);
    if (target > m_items.capacity()) {
      m_items.reserve(target);
      m_sort_indices.reserve(target);
      m_sort_keys.reserve(target);
    }
    if (m_prepared_high_water > m_prepared_batches.capacity()) {
      m_prepared_batches.reserve(m_prepared_high_water);
    }
    if (m_submission_bucket_high_water > m_submission_bucket_spans.capacity()) {
      m_submission_bucket_spans.reserve(m_submission_bucket_high_water);
    }
  }

  [[nodiscard]] auto items_high_water() const noexcept -> std::size_t {
    return m_items_high_water;
  }
  [[nodiscard]] auto prepared_high_water() const noexcept -> std::size_t {
    return m_prepared_high_water;
  }
  [[nodiscard]] auto submission_bucket_high_water() const noexcept -> std::size_t {
    return m_submission_bucket_high_water;
  }
  [[nodiscard]] auto submission_bucket_capacity() const noexcept -> std::size_t {
    return m_submission_bucket_spans.capacity();
  }

  template <typename CmdT,
            typename = std::enable_if_t<std::is_constructible_v<DrawCmd, CmdT&&>>>
  void submit(CmdT&& cmd) {
    DrawCmd draw_cmd(std::forward<CmdT>(cmd));
    record_submission_bucket(draw_cmd);
    m_items.emplace_back(std::move(draw_cmd));
  }

  [[nodiscard]] auto empty() const -> bool { return m_items.empty(); }
  [[nodiscard]] auto size() const -> std::size_t { return m_items.size(); }

  [[nodiscard]] auto get_sorted(std::size_t i) const -> const DrawCmd& {
    return m_items[m_sort_indices[i]];
  }

  [[nodiscard]] auto items() const -> const std::vector<DrawCmd>& { return m_items; }

  [[nodiscard]] auto prepared_batches() const -> const std::vector<PreparedBatch>& {
    return m_prepared_batches;
  }

  [[nodiscard]] auto sort_key_for_sorted(std::size_t i) const -> std::uint64_t {
    return m_sort_keys[m_sort_indices[i]];
  }

  void sort_for_batching() {
    const std::size_t count = m_items.size();

    m_sort_keys.resize(count);
    m_sort_indices.resize(count);
    m_prepared_batches.clear();

    for (std::size_t i = 0; i < count; ++i) {
      m_sort_indices[i] = static_cast<uint32_t>(i);
      m_sort_keys[i] = compute_sort_key(m_items[i]);
    }

    if (count >= 2) {
      if (!m_submission_bucket_ordered || !sort_bucketed_ranges(count)) {
        sort_full_keys(0, count);
      }
    }
    build_prepared_batches();
  }

  [[nodiscard]] auto can_batch_mesh(std::size_t sorted_idx_a,
                                    std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto& a = m_items[m_sort_indices[sorted_idx_a]];
    const auto& b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != MeshCmdIndex || b.index() != MeshCmdIndex) {
      return false;
    }
    const auto& mesh_a = std::get<MeshCmdIndex>(a);
    const auto& mesh_b = std::get<MeshCmdIndex>(b);
    if (mesh_a.alpha < k_opaque_threshold || mesh_b.alpha < k_opaque_threshold) {
      return false;
    }
    return mesh_a.mesh == mesh_b.mesh && mesh_a.shader == mesh_b.shader &&
           mesh_a.texture == mesh_b.texture && mesh_a.material_id == mesh_b.material_id;
  }

private:
  struct SortIdentity {
    std::uint8_t pass = 0;
    std::uint8_t pipeline = 0;
    std::uint8_t transparency_bucket = 0;
    std::uint16_t material = 0;
    std::uint16_t mesh = 0;
    std::uint16_t texture = 0;
    std::uint8_t skeleton = 0;

    [[nodiscard]] auto pack() const noexcept -> std::uint64_t {
      return (static_cast<std::uint64_t>(pass) << 56) |
             (static_cast<std::uint64_t>(pipeline) << 48) |
             (static_cast<std::uint64_t>(transparency_bucket & 0x0FU) << 44) |
             (static_cast<std::uint64_t>(material & 0x0FFFU) << 32) |
             (static_cast<std::uint64_t>(mesh) << 16) |
             (static_cast<std::uint64_t>(texture & 0x0FFFU) << 4) |
             static_cast<std::uint64_t>(skeleton & 0x0FU);
    }
  };

  struct SubmissionBucketSpan {
    std::uint32_t bucket = 0;
    std::size_t start = 0;
    std::size_t count = 0;
    bool preserves_append_order = false;

    [[nodiscard]] auto end() const noexcept -> std::size_t { return start + count; }
  };

  enum class SortPipeline : std::uint8_t {
    Mesh = 0,
    DrawPart = 1,
    RiggedCreature = 2,
    Cylinder = 3,
    Fog = 4,
    TerrainScatterGrass = 8,
    TerrainScatterStone = 9,
    TerrainScatterPlant = 10,
    TerrainScatterPine = 11,
    TerrainScatterOlive = 12,
    TerrainScatterFireCamp = 13,
    Rain = 20,
    TerrainSurface = 21,
    TerrainFeatureWater = 22,
    TerrainFeatureRoad = 23,
    TerrainFeatureShoreline = 24,
    TerrainFeatureBridge = 25,
    PrimitiveSphere = 27,
    PrimitiveCylinder = 28,
    PrimitiveCone = 29,
    Effect = 30,
    Grid = 31,
    SelectionSmoke = 32,
    SelectionRing = 33,
    ModeIndicator = 34
  };

  void sort_full_keys(std::size_t start, std::size_t end) {
    std::stable_sort(m_sort_indices.begin() + static_cast<std::ptrdiff_t>(start),
                     m_sort_indices.begin() + static_cast<std::ptrdiff_t>(end),
                     [&](std::uint32_t lhs, std::uint32_t rhs) {
                       if (m_sort_keys[lhs] == m_sort_keys[rhs]) {
                         const auto lhs_full = full_resource_identity(m_items[lhs]);
                         const auto rhs_full = full_resource_identity(m_items[rhs]);
                         if (lhs_full != rhs_full) {
                           return lhs_full < rhs_full;
                         }
                         return lhs < rhs;
                       }
                       return m_sort_keys[lhs] < m_sort_keys[rhs];
                     });
  }

  [[nodiscard]] auto sort_bucketed_ranges(std::size_t count) -> bool {
    if (m_submission_bucket_spans.empty()) {
      return false;
    }

    std::size_t covered = 0;
    for (const SubmissionBucketSpan& span : m_submission_bucket_spans) {

      if (span.start != covered || span.end() > count) {
        return false;
      }
      if (span.count >= 2U && !span.preserves_append_order) {
        sort_full_keys(span.start, span.end());
      }
      covered = span.end();
    }

    return covered == count;
  }

  void populate_sort_identity_prefix(const DrawCmd& cmd, SortIdentity& identity) const {
    enum class RenderOrder : uint8_t {
      TerrainSurface = 0,
      TerrainFeature = 1,
      TerrainScatter = 2,
      RainBatch = 3,
      PrimitiveBatch = 4,
      Mesh = 5,
      Cylinder = 6,
      FogBatch = 7,
      SelectionSmoke = 8,
      Grid = 9,
      EffectBatch = 10,
      SelectionRing = 16,
      ModeIndicator = 17
    };

    static constexpr uint8_t k_type_order[] = {
        static_cast<uint8_t>(RenderOrder::Grid),
        static_cast<uint8_t>(RenderOrder::SelectionRing),
        static_cast<uint8_t>(RenderOrder::SelectionSmoke),
        static_cast<uint8_t>(RenderOrder::Cylinder),
        static_cast<uint8_t>(RenderOrder::Mesh),
        static_cast<uint8_t>(RenderOrder::FogBatch),
        static_cast<uint8_t>(RenderOrder::TerrainScatter),
        static_cast<uint8_t>(RenderOrder::RainBatch),
        static_cast<uint8_t>(RenderOrder::TerrainSurface),
        static_cast<uint8_t>(RenderOrder::TerrainFeature),
        static_cast<uint8_t>(RenderOrder::PrimitiveBatch),
        static_cast<uint8_t>(RenderOrder::EffectBatch),
        static_cast<uint8_t>(RenderOrder::ModeIndicator),
        static_cast<uint8_t>(RenderOrder::Mesh),
        static_cast<uint8_t>(RenderOrder::Mesh)};

    const std::size_t type_index = cmd.index();
    constexpr std::size_t type_count = sizeof(k_type_order) / sizeof(k_type_order[0]);
    const uint8_t type_order = type_index < type_count
                                   ? k_type_order[type_index]
                                   : static_cast<uint8_t>(type_index);

    identity.pass = type_order;

    if (cmd.index() == MeshCmdIndex) {
      const auto& mesh = std::get<MeshCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Mesh);
      identity.transparency_bucket = transparency_bucket(mesh.alpha);
    } else if (cmd.index() == TerrainScatterCmdIndex) {
      const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainScatterGrass) +
                          static_cast<std::uint8_t>(deco.species);
    } else if (cmd.index() == TerrainSurfaceCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainSurface);
    } else if (cmd.index() == TerrainFeatureCmdIndex) {
      const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::TerrainFeatureWater) +
                          static_cast<std::uint8_t>(feature.kind);
      identity.transparency_bucket = transparency_bucket(feature.alpha);
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::PrimitiveSphere) +
                          static_cast<std::uint8_t>(prim.type);
    } else if (cmd.index() == DrawPartCmdIndex) {
      const auto& part = std::get<DrawPartCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::DrawPart);
      identity.transparency_bucket = transparency_bucket(part.alpha);
    } else if (cmd.index() == RiggedCreatureCmdIndex) {
      const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::RiggedCreature);
      identity.transparency_bucket = transparency_bucket(rig.alpha);
    } else if (cmd.index() == CylinderCmdIndex) {
      const auto& cy = std::get<CylinderCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Cylinder);
      identity.transparency_bucket = transparency_bucket(cy.alpha);
    } else if (cmd.index() == FogBatchCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Fog);
    } else if (cmd.index() == RainBatchCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Rain);
    } else if (cmd.index() == EffectBatchCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Effect);
    } else if (cmd.index() == SelectionSmokeCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::SelectionSmoke);
      identity.transparency_bucket = 1U;
    } else if (cmd.index() == GridCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Grid);
    } else if (cmd.index() == SelectionRingCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::SelectionRing);
      identity.transparency_bucket = 1U;
    } else if (cmd.index() == ModeIndicatorCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::ModeIndicator);
    }
  }

  [[nodiscard]] auto
  compute_submission_bucket(const DrawCmd& cmd) const -> std::uint32_t {
    SortIdentity identity;
    populate_sort_identity_prefix(cmd, identity);
    return (static_cast<std::uint32_t>(identity.pass) << 16) |
           (static_cast<std::uint32_t>(identity.pipeline) << 8) |
           static_cast<std::uint32_t>(identity.transparency_bucket & 0x0FU);
  }

  [[nodiscard]] auto preserves_append_order(const DrawCmd& cmd) const -> bool {
    switch (cmd.index()) {
    case TerrainScatterCmdIndex:
    case RainBatchCmdIndex:
    case TerrainSurfaceCmdIndex:
    case PrimitiveBatchCmdIndex:
    case FogBatchCmdIndex:
    case SelectionSmokeCmdIndex:
    case SelectionRingCmdIndex:
    case GridCmdIndex:
    case EffectBatchCmdIndex:
    case ModeIndicatorCmdIndex:
      return true;
    case TerrainFeatureCmdIndex: {
      const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
      return feature.kind != LinearFeatureKind::Shoreline ||
             !feature.visibility.enabled;
    }
    default:
      return false;
    }
  }

  void record_submission_bucket(const DrawCmd& cmd) {
    const std::uint32_t bucket = compute_submission_bucket(cmd);
    const bool append_ordered = preserves_append_order(cmd);
    const std::size_t next_index = m_items.size();
    if (!m_submission_bucket_spans.empty()) {
      SubmissionBucketSpan& last = m_submission_bucket_spans.back();
      if (bucket < last.bucket) {
        m_submission_bucket_ordered = false;
      }
      if (bucket == last.bucket) {
        ++last.count;
        last.preserves_append_order = last.preserves_append_order && append_ordered;
        return;
      }
    }

    m_submission_bucket_spans.push_back(
        SubmissionBucketSpan{.bucket = bucket,
                             .start = next_index,
                             .count = 1U,
                             .preserves_append_order = append_ordered});
  }

  [[nodiscard]] auto compute_sort_key(const DrawCmd& cmd) -> uint64_t {
    SortIdentity identity;
    populate_sort_identity_prefix(cmd, identity);

    if (cmd.index() == MeshCmdIndex) {
      const auto& mesh = std::get<MeshCmdIndex>(cmd);
      identity.material = pack_12(sort_id(mesh.shader));
      identity.mesh = pack_16(sort_id(mesh.mesh));
      identity.texture = pack_12(sort_id(mesh.texture));
    } else if (cmd.index() == TerrainScatterCmdIndex) {
      const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
      identity.material = pack_12(sort_id(deco.material));
      identity.mesh = pack_16(sort_id(deco.instance_buffer));
    } else if (cmd.index() == FogBatchCmdIndex) {
      const auto& fog = std::get<FogBatchCmdIndex>(cmd);
      identity.mesh = pack_16(sort_id(
          fog.instance_buffer != nullptr ? static_cast<const void*>(fog.instance_buffer)
                                         : static_cast<const void*>(fog.instances)));
    } else if (cmd.index() == TerrainSurfaceCmdIndex) {
      const auto& chunk = std::get<TerrainSurfaceCmdIndex>(cmd);
      identity.material = pack_12(chunk.sort_key);
      identity.mesh = pack_16(sort_id(chunk.mesh));
      identity.texture = pack_12(sort_id(chunk.material));
    } else if (cmd.index() == TerrainFeatureCmdIndex) {
      const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
      identity.mesh = pack_16(sort_id(feature.mesh));
      identity.texture = pack_12(sort_id(feature.visibility.texture));
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
      identity.mesh = pack_16(static_cast<std::uint32_t>(std::min<std::size_t>(
          prim.instance_count(), std::numeric_limits<std::uint16_t>::max())));
    } else if (cmd.index() == DrawPartCmdIndex) {
      const auto& part = std::get<DrawPartCmdIndex>(cmd);
      identity.material = pack_12(sort_id(part.material));
      identity.mesh = pack_16(sort_id(part.mesh));
      identity.texture = pack_12(sort_id(part.texture));
      identity.skeleton = part.palette.empty() ? 0U : 1U;
    } else if (cmd.index() == RiggedCreatureCmdIndex) {
      const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
      identity.material = pack_12(sort_id(rig.material));
      identity.mesh = pack_16(sort_id(rig.mesh));
      identity.texture = pack_12(sort_id(rig.texture));
      identity.skeleton = pack_4(rig.bone_count);
    } else if (cmd.index() == EffectBatchCmdIndex) {
      const auto& effect = std::get<EffectBatchCmdIndex>(cmd);
      identity.material = pack_12(static_cast<std::uint32_t>(effect.kind));
    } else if (cmd.index() == ModeIndicatorCmdIndex) {
      const auto& mode = std::get<ModeIndicatorCmdIndex>(cmd);
      identity.material = pack_12(static_cast<std::uint32_t>(mode.mode_type));
    }

    return identity.pack();
  }

  void build_prepared_batches() {
    m_prepared_batches.clear();
    const std::size_t count = m_sort_indices.size();
    std::size_t i = 0;
    while (i < count) {
      const DrawCmd& head = get_sorted(i);
      const DrawCmdType type = draw_cmd_type(head);
      PreparedBatchKind kind = PreparedBatchKind::Single;
      std::size_t end = i + 1;

      if (head.index() == CylinderCmdIndex) {
        while (end < count && get_sorted(end).index() == CylinderCmdIndex) {
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::CylinderInstanced;
        }
      } else if (head.index() == MeshCmdIndex) {
        while (end < count && can_batch_mesh(i, end)) {
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::MeshInstanced;
        }
      } else if (head.index() == DrawPartCmdIndex) {
        while (end < count && can_batch_draw_part(i, end)) {
          ++end;
        }
        constexpr std::size_t k_draw_part_min_run = 4;
        if (end - i >= k_draw_part_min_run) {
          kind = PreparedBatchKind::DrawPartInstanced;
        } else {
          end = i + 1;
        }
      } else if (head.index() == RiggedCreatureCmdIndex) {
        while (end < count && can_batch_rigged(i, end)) {
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::RiggedCreatureInstanced;
        }
      } else if (head.index() == SelectionRingCmdIndex) {
        while (end < count && get_sorted(end).index() == SelectionRingCmdIndex) {
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::SelectionRingInstanced;
        }
      } else if (head.index() == EffectBatchCmdIndex) {
        const auto& head_eff = std::get<EffectBatchCmdIndex>(head);
        while (end < count) {
          const DrawCmd& next_cmd = get_sorted(end);
          if (next_cmd.index() != EffectBatchCmdIndex) {
            break;
          }
          if (std::get<EffectBatchCmdIndex>(next_cmd).kind != head_eff.kind) {
            break;
          }
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::EffectInstanced;
        }
      } else if (head.index() == ModeIndicatorCmdIndex) {
        const auto& head_mode = std::get<ModeIndicatorCmdIndex>(head);
        while (end < count) {
          const DrawCmd& next_cmd = get_sorted(end);
          if (next_cmd.index() != ModeIndicatorCmdIndex) {
            break;
          }
          if (std::get<ModeIndicatorCmdIndex>(next_cmd).mode_type !=
              head_mode.mode_type) {
            break;
          }
          ++end;
        }
        if (end - i > 1U) {
          kind = PreparedBatchKind::ModeIndicatorInstanced;
        }
      } else if (head.index() == TerrainSurfaceCmdIndex) {
        while (end < count && can_batch_terrain_surface(i, end)) {
          ++end;
        }
      } else if (head.index() == TerrainFeatureCmdIndex) {
        while (end < count && can_batch_terrain_feature(i, end)) {
          ++end;
        }
      }

      m_prepared_batches.push_back(
          PreparedBatch{.start = i,
                        .count = end - i,
                        .type = type,
                        .kind = kind,
                        .sort_key = m_sort_keys[m_sort_indices[i]]});
      i = end;
    }
  }

  [[nodiscard]] auto can_batch_draw_part(std::size_t sorted_idx_a,
                                         std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto& a = m_items[m_sort_indices[sorted_idx_a]];
    const auto& b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != DrawPartCmdIndex || b.index() != DrawPartCmdIndex) {
      return false;
    }
    const auto& part_a = std::get<DrawPartCmdIndex>(a);
    const auto& part_b = std::get<DrawPartCmdIndex>(b);
    if (part_a.alpha < k_opaque_threshold || part_b.alpha < k_opaque_threshold) {
      return false;
    }
    return part_a.mesh == part_b.mesh && part_a.material == part_b.material &&
           part_a.texture == part_b.texture &&
           part_a.material_id == part_b.material_id &&
           part_a.priority == part_b.priority && part_a.palette.empty() &&
           part_b.palette.empty();
  }

  [[nodiscard]] auto can_batch_rigged(std::size_t sorted_idx_a,
                                      std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto& a = m_items[m_sort_indices[sorted_idx_a]];
    const auto& b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != RiggedCreatureCmdIndex || b.index() != RiggedCreatureCmdIndex) {
      return false;
    }
    const auto& rig_a = std::get<RiggedCreatureCmdIndex>(a);
    const auto& rig_b = std::get<RiggedCreatureCmdIndex>(b);

    return rig_a.mesh != nullptr && rig_a.texture == nullptr &&
           rig_b.texture == nullptr && rig_a.mesh == rig_b.mesh &&
           rig_a.material == rig_b.material;
  }

  [[nodiscard]] auto can_batch_terrain_surface(std::size_t sorted_idx_a,
                                               std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto& a = m_items[m_sort_indices[sorted_idx_a]];
    const auto& b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != TerrainSurfaceCmdIndex || b.index() != TerrainSurfaceCmdIndex) {
      return false;
    }
    const auto& surface_a = std::get<TerrainSurfaceCmdIndex>(a);
    const auto& surface_b = std::get<TerrainSurfaceCmdIndex>(b);
    return surface_a.mesh != nullptr && surface_b.mesh != nullptr &&
           surface_a.params.is_ground_plane == surface_b.params.is_ground_plane &&
           surface_a.depth_write == surface_b.depth_write &&
           surface_a.wireframe == surface_b.wireframe &&
           surface_a.depth_bias == surface_b.depth_bias;
  }

  [[nodiscard]] auto can_batch_terrain_feature(std::size_t sorted_idx_a,
                                               std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto& a = m_items[m_sort_indices[sorted_idx_a]];
    const auto& b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != TerrainFeatureCmdIndex || b.index() != TerrainFeatureCmdIndex) {
      return false;
    }
    const auto& feature_a = std::get<TerrainFeatureCmdIndex>(a);
    const auto& feature_b = std::get<TerrainFeatureCmdIndex>(b);
    return feature_a.mesh != nullptr && feature_b.mesh != nullptr &&
           feature_a.kind == feature_b.kind &&
           feature_a.road_surface_kind == feature_b.road_surface_kind &&
           transparency_bucket(feature_a.alpha) ==
               transparency_bucket(feature_b.alpha) &&
           feature_a.visibility.texture == feature_b.visibility.texture &&
           feature_a.visibility.size == feature_b.visibility.size &&
           feature_a.visibility.tile_size == feature_b.visibility.tile_size &&
           feature_a.visibility.explored_alpha == feature_b.visibility.explored_alpha &&
           feature_a.visibility.enabled == feature_b.visibility.enabled;
  }

  static auto transparency_bucket(float alpha) noexcept -> std::uint8_t {
    return alpha < k_opaque_threshold ? 1U : 0U;
  }

  static auto pack_4(std::uint32_t value) noexcept -> std::uint8_t {
    return static_cast<std::uint8_t>(std::min<std::uint32_t>(value, 0x0FU));
  }

  static auto pack_12(std::uint32_t value) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0x0FFFU));
  }

  static auto pack_16(std::uint32_t value) noexcept -> std::uint16_t {
    return static_cast<std::uint16_t>(std::min<std::uint32_t>(value, 0xFFFFU));
  }

  static auto sort_id(const void* ptr) noexcept -> std::uint32_t {
    if (ptr == nullptr) {
      return 0;
    }
    const auto value =
        static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(ptr));
    return static_cast<std::uint32_t>(value ^ (value >> 16U) ^ (value >> 32U));
  }

  [[nodiscard]] static auto ptr_value(const void* ptr) noexcept -> std::uintptr_t {
    return reinterpret_cast<std::uintptr_t>(ptr);
  }

  [[nodiscard]] static auto
  full_resource_identity(const DrawCmd& cmd) noexcept -> std::array<std::uintptr_t, 4> {
    if (cmd.index() == MeshCmdIndex) {
      const auto& mesh = std::get<MeshCmdIndex>(cmd);
      return {
          ptr_value(mesh.shader), ptr_value(mesh.mesh), ptr_value(mesh.texture), 0U};
    }
    if (cmd.index() == TerrainScatterCmdIndex) {
      const auto& deco = std::get<TerrainScatterCmdIndex>(cmd);
      return {ptr_value(deco.material), ptr_value(deco.instance_buffer), 0U, 0U};
    }
    if (cmd.index() == FogBatchCmdIndex) {
      const auto& fog = std::get<FogBatchCmdIndex>(cmd);
      return {ptr_value(fog.instance_buffer), ptr_value(fog.instances), fog.count, 0U};
    }
    if (cmd.index() == TerrainSurfaceCmdIndex) {
      const auto& chunk = std::get<TerrainSurfaceCmdIndex>(cmd);
      return {static_cast<std::uintptr_t>(chunk.sort_key),
              ptr_value(chunk.mesh),
              ptr_value(chunk.material),
              0U};
    }
    if (cmd.index() == TerrainFeatureCmdIndex) {
      const auto& feature = std::get<TerrainFeatureCmdIndex>(cmd);
      return {ptr_value(feature.mesh), ptr_value(feature.visibility.texture), 0U, 0U};
    }
    if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto& prim = std::get<PrimitiveBatchCmdIndex>(cmd);
      return {prim.instance_count(), 0U, 0U, 0U};
    }
    if (cmd.index() == DrawPartCmdIndex) {
      const auto& part = std::get<DrawPartCmdIndex>(cmd);
      return {ptr_value(part.material),
              ptr_value(part.mesh),
              ptr_value(part.texture),
              part.palette.empty() ? 0U : 1U};
    }
    if (cmd.index() == RiggedCreatureCmdIndex) {
      const auto& rig = std::get<RiggedCreatureCmdIndex>(cmd);
      return {ptr_value(rig.material),
              ptr_value(rig.mesh),
              ptr_value(rig.texture),
              rig.bone_count};
    }
    if (cmd.index() == EffectBatchCmdIndex) {
      const auto& effect = std::get<EffectBatchCmdIndex>(cmd);
      return {static_cast<std::uintptr_t>(effect.kind), 0U, 0U, 0U};
    }
    if (cmd.index() == ModeIndicatorCmdIndex) {
      const auto& mode = std::get<ModeIndicatorCmdIndex>(cmd);
      return {static_cast<std::uintptr_t>(mode.mode_type), 0U, 0U, 0U};
    }
    return {0U, 0U, 0U, 0U};
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sort_indices;
  std::vector<uint64_t> m_sort_keys;
  std::vector<PreparedBatch> m_prepared_batches;
  std::vector<SubmissionBucketSpan> m_submission_bucket_spans;
  bool m_submission_bucket_ordered = true;
  std::size_t m_items_high_water = 0;
  std::size_t m_prepared_high_water = 0;
  std::size_t m_submission_bucket_high_water = 0;
};

} // namespace Render::GL
