#pragma once

#include "decoration_gpu.h"
#include "draw_part.h"
#include "frame_budget.h"
#include "primitive_batch.h"
#include "rain_gpu.h"
#include "terrain_scene_types.h"
#include "world_chunk.h"
#include <QMatrix4x4>
#include <QVector2D>
#include <QVector3D>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  QMatrix4x4 model;
  QVector3D color{1, 1, 1};
  float alpha = 1.0F;
  int material_id = 0;
  class Shader *shader = nullptr;
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
  QVector3D color{0.05F, 0.05F, 0.05F};
  float alpha = 1.0F;
  float size = 1.0F;
};

struct FogBatchCmd {
  const FogInstanceData *instances = nullptr;
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
    FireCamp
  };

  Species species = Species::Grass;
  const Material *material = nullptr;
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;

  GrassBatchParams grass{};
  StoneBatchParams stone{};
  PlantBatchParams plant{};
  PineBatchParams pine{};
  OliveBatchParams olive{};
  FireCampBatchParams firecamp{};

  CommandPriority priority{CommandPriority::Low};
};

struct RainBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  RainBatchParams params;
  CommandPriority priority{CommandPriority::Low};
};

struct TerrainSurfaceCmd {

  Mesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 model;
  BoundingBox aabb;
  TerrainChunkParams params;
  std::uint16_t sort_key = 0x8000U;
  bool depth_write = true;
  bool wireframe = false;
  float depth_bias = 0.0F;
  CommandPriority priority{CommandPriority::High};
};

struct TerrainFeatureCmd {

  struct VisibilityResources {
    Texture *texture = nullptr;
    QVector2D size{0.0F, 0.0F};
    float tile_size = 1.0F;
    float explored_alpha = 0.6F;
    bool enabled = false;
  };

  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  LinearFeatureKind kind = LinearFeatureKind::River;
  VisibilityResources visibility{};
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
    StoneImpact
  };

  Kind kind = Kind::HealerAura;

  QVector3D position{0.0F, 0.0F, 0.0F};
  QVector3D end_pos{0.0F, 0.0F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};

  float radius = 1.0F;
  float intensity = 1.0F;
  float time = 0.0F;

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
  static constexpr std::size_t kMaxRoleColors = 32;

  RiggedMesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 world;

  const QMatrix4x4 *bone_palette = nullptr;

  std::uint32_t palette_ubo = 0;
  std::uint32_t palette_offset = 0;
  std::uint32_t bone_count = 0;
  std::array<QVector3D, kMaxRoleColors> role_colors{};
  std::uint32_t role_color_count = 0;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  Texture *texture = nullptr;
  std::int32_t material_id = 0;
  QVector3D variation_scale{1.0F, 1.0F, 1.0F};
  CommandPriority priority{CommandPriority::Normal};
};

using DrawCmd =
    std::variant<GridCmd, SelectionRingCmd, SelectionSmokeCmd, CylinderCmd,
                 MeshCmd, FogBatchCmd, TerrainScatterCmd, RainBatchCmd,
                 TerrainSurfaceCmd, TerrainFeatureCmd, PrimitiveBatchCmd,
                 EffectBatchCmd, ModeIndicatorCmd, DrawPartCmd,
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

constexpr std::size_t MeshCmdIndex =
    static_cast<std::size_t>(DrawCmdType::Mesh);
constexpr std::size_t GridCmdIndex =
    static_cast<std::size_t>(DrawCmdType::Grid);
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

inline auto draw_cmd_type(const DrawCmd &cmd) -> DrawCmdType {
  return static_cast<DrawCmdType>(cmd.index());
}

inline auto extract_cmd_priority(const DrawCmd &cmd) -> CommandPriority {
  return std::visit([](const auto &c) -> CommandPriority { return c.priority; },
                    cmd);
}

enum class PreparedBatchKind : std::uint8_t {
  Single,
  CylinderInstanced,
  MeshInstanced,
  DrawPartInstanced,
  RiggedCreatureInstanced
};

struct PreparedBatch {
  std::size_t start = 0;
  std::size_t count = 0;
  DrawCmdType type = DrawCmdType::Grid;
  PreparedBatchKind kind = PreparedBatchKind::Single;
  std::uint64_t sort_key = 0;

  [[nodiscard]] auto end() const noexcept -> std::size_t {
    return start + count;
  }

  [[nodiscard]] auto is_instanced() const noexcept -> bool {
    return kind != PreparedBatchKind::Single;
  }
};

class DrawQueue {
public:
  void clear() {
    m_items.clear();
    m_sort_indices.clear();
    m_sort_keys.clear();
    m_prepared_batches.clear();
    clear_sort_id_maps();
  }

  template <typename CmdT, typename = std::enable_if_t<
                               std::is_constructible_v<DrawCmd, CmdT &&>>>
  void submit(CmdT &&cmd) {
    m_items.emplace_back(std::forward<CmdT>(cmd));
  }

  [[nodiscard]] auto empty() const -> bool { return m_items.empty(); }
  [[nodiscard]] auto size() const -> std::size_t { return m_items.size(); }

  [[nodiscard]] auto get_sorted(std::size_t i) const -> const DrawCmd & {
    return m_items[m_sort_indices[i]];
  }

  [[nodiscard]] auto items() const -> const std::vector<DrawCmd> & {
    return m_items;
  }

  [[nodiscard]] auto
  prepared_batches() const -> const std::vector<PreparedBatch> & {
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
    clear_sort_id_maps();

    for (std::size_t i = 0; i < count; ++i) {
      m_sort_indices[i] = static_cast<uint32_t>(i);
      m_sort_keys[i] = compute_sort_key(m_items[i]);
    }

    if (count >= 2) {
      sort_full_keys(count);
    }
    build_prepared_batches();
  }

  [[nodiscard]] auto can_batch_mesh(std::size_t sorted_idx_a,
                                    std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto &a = m_items[m_sort_indices[sorted_idx_a]];
    const auto &b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != MeshCmdIndex || b.index() != MeshCmdIndex) {
      return false;
    }
    const auto &mesh_a = std::get<MeshCmdIndex>(a);
    const auto &mesh_b = std::get<MeshCmdIndex>(b);
    if (mesh_a.alpha < k_opaque_threshold ||
        mesh_b.alpha < k_opaque_threshold) {
      return false;
    }
    return mesh_a.mesh == mesh_b.mesh && mesh_a.shader == mesh_b.shader &&
           mesh_a.texture == mesh_b.texture;
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
    TerrainFeatureRiver = 22,
    TerrainFeatureRoad = 23,
    TerrainFeatureRiverbank = 24,
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

  void sort_full_keys(std::size_t count) {
    std::stable_sort(m_sort_indices.begin(), m_sort_indices.begin() + count,
                     [&](std::uint32_t lhs, std::uint32_t rhs) {
                       if (m_sort_keys[lhs] == m_sort_keys[rhs]) {
                         return lhs < rhs;
                       }
                       return m_sort_keys[lhs] < m_sort_keys[rhs];
                     });
  }

  [[nodiscard]] auto compute_sort_key(const DrawCmd &cmd) -> uint64_t {

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
    constexpr std::size_t type_count =
        sizeof(k_type_order) / sizeof(k_type_order[0]);
    const uint8_t type_order = type_index < type_count
                                   ? k_type_order[type_index]
                                   : static_cast<uint8_t>(type_index);

    SortIdentity identity;
    identity.pass = type_order;

    if (cmd.index() == MeshCmdIndex) {
      const auto &mesh = std::get<MeshCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Mesh);
      identity.transparency_bucket = transparency_bucket(mesh.alpha);
      identity.material = pack_12(intern_material_id(mesh.shader));
      identity.mesh = pack_16(intern_mesh_id(mesh.mesh));
      identity.texture = pack_12(intern_texture_id(mesh.texture));
    } else if (cmd.index() == TerrainScatterCmdIndex) {
      const auto &deco = std::get<TerrainScatterCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::TerrainScatterGrass) +
          static_cast<std::uint8_t>(deco.species);
      identity.material = pack_12(intern_material_id(deco.material));
      identity.mesh = pack_16(intern_mesh_id(deco.instance_buffer));
    } else if (cmd.index() == TerrainSurfaceCmdIndex) {
      const auto &chunk = std::get<TerrainSurfaceCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::TerrainSurface);
      identity.material = pack_12(chunk.sort_key);
      identity.mesh = pack_16(intern_mesh_id(chunk.mesh));
      identity.texture = pack_12(intern_material_id(chunk.material));
    } else if (cmd.index() == TerrainFeatureCmdIndex) {
      const auto &feature = std::get<TerrainFeatureCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::TerrainFeatureRiver) +
          static_cast<std::uint8_t>(feature.kind);
      identity.transparency_bucket = transparency_bucket(feature.alpha);
      identity.mesh = pack_16(intern_mesh_id(feature.mesh));
      identity.texture = pack_12(intern_texture_id(feature.visibility.texture));
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto &prim = std::get<PrimitiveBatchCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::PrimitiveSphere) +
          static_cast<std::uint8_t>(prim.type);
      identity.mesh = pack_16(static_cast<std::uint32_t>(std::min<std::size_t>(
          prim.instance_count(), std::numeric_limits<std::uint16_t>::max())));
    } else if (cmd.index() == DrawPartCmdIndex) {
      const auto &part = std::get<DrawPartCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::DrawPart);
      identity.transparency_bucket = transparency_bucket(part.alpha);
      identity.material = pack_12(intern_material_id(part.material));
      identity.mesh = pack_16(intern_mesh_id(part.mesh));
      identity.texture = pack_12(intern_texture_id(part.texture));
      identity.skeleton = part.palette.empty() ? 0U : 1U;
    } else if (cmd.index() == RiggedCreatureCmdIndex) {
      const auto &rig = std::get<RiggedCreatureCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::RiggedCreature);
      identity.transparency_bucket = transparency_bucket(rig.alpha);
      identity.material = pack_12(intern_material_id(rig.material));
      identity.mesh = pack_16(intern_mesh_id(rig.mesh));
      identity.texture = pack_12(intern_texture_id(rig.texture));
      identity.skeleton = pack_4(rig.bone_count);
    } else if (cmd.index() == CylinderCmdIndex) {
      const auto &cy = std::get<CylinderCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Cylinder);
      identity.transparency_bucket = transparency_bucket(cy.alpha);
    } else if (cmd.index() == FogBatchCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Fog);
    } else if (cmd.index() == RainBatchCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Rain);
    } else if (cmd.index() == EffectBatchCmdIndex) {
      const auto &effect = std::get<EffectBatchCmdIndex>(cmd);
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Effect);
      identity.material = pack_12(static_cast<std::uint32_t>(effect.kind));
    } else if (cmd.index() == SelectionSmokeCmdIndex) {
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::SelectionSmoke);
      identity.transparency_bucket = 1U;
    } else if (cmd.index() == GridCmdIndex) {
      identity.pipeline = static_cast<std::uint8_t>(SortPipeline::Grid);
    } else if (cmd.index() == SelectionRingCmdIndex) {
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::SelectionRing);
      identity.transparency_bucket = 1U;
    } else if (cmd.index() == ModeIndicatorCmdIndex) {
      const auto &mode = std::get<ModeIndicatorCmdIndex>(cmd);
      identity.pipeline =
          static_cast<std::uint8_t>(SortPipeline::ModeIndicator);
      identity.material = pack_12(static_cast<std::uint32_t>(mode.mode_type));
    }

    return identity.pack();
  }

  void build_prepared_batches() {
    m_prepared_batches.clear();
    const std::size_t count = m_sort_indices.size();
    std::size_t i = 0;
    while (i < count) {
      const DrawCmd &head = get_sorted(i);
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

  [[nodiscard]] auto
  can_batch_draw_part(std::size_t sorted_idx_a,
                      std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto &a = m_items[m_sort_indices[sorted_idx_a]];
    const auto &b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != DrawPartCmdIndex || b.index() != DrawPartCmdIndex) {
      return false;
    }
    const auto &part_a = std::get<DrawPartCmdIndex>(a);
    const auto &part_b = std::get<DrawPartCmdIndex>(b);
    if (part_a.alpha < k_opaque_threshold ||
        part_b.alpha < k_opaque_threshold) {
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
    const auto &a = m_items[m_sort_indices[sorted_idx_a]];
    const auto &b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != RiggedCreatureCmdIndex ||
        b.index() != RiggedCreatureCmdIndex) {
      return false;
    }
    const auto &rig_a = std::get<RiggedCreatureCmdIndex>(a);
    const auto &rig_b = std::get<RiggedCreatureCmdIndex>(b);
    return rig_a.mesh != nullptr && rig_a.texture == nullptr &&
           rig_b.texture == nullptr && rig_a.mesh == rig_b.mesh &&
           rig_a.material == rig_b.material;
  }

  [[nodiscard]] auto
  can_batch_terrain_surface(std::size_t sorted_idx_a,
                            std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto &a = m_items[m_sort_indices[sorted_idx_a]];
    const auto &b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != TerrainSurfaceCmdIndex ||
        b.index() != TerrainSurfaceCmdIndex) {
      return false;
    }
    const auto &surface_a = std::get<TerrainSurfaceCmdIndex>(a);
    const auto &surface_b = std::get<TerrainSurfaceCmdIndex>(b);
    return surface_a.mesh != nullptr && surface_b.mesh != nullptr &&
           surface_a.params.is_ground_plane ==
               surface_b.params.is_ground_plane &&
           surface_a.depth_write == surface_b.depth_write &&
           surface_a.wireframe == surface_b.wireframe &&
           surface_a.depth_bias == surface_b.depth_bias;
  }

  [[nodiscard]] auto
  can_batch_terrain_feature(std::size_t sorted_idx_a,
                            std::size_t sorted_idx_b) const -> bool {
    if (sorted_idx_a >= m_items.size() || sorted_idx_b >= m_items.size()) {
      return false;
    }
    const auto &a = m_items[m_sort_indices[sorted_idx_a]];
    const auto &b = m_items[m_sort_indices[sorted_idx_b]];
    if (a.index() != TerrainFeatureCmdIndex ||
        b.index() != TerrainFeatureCmdIndex) {
      return false;
    }
    const auto &feature_a = std::get<TerrainFeatureCmdIndex>(a);
    const auto &feature_b = std::get<TerrainFeatureCmdIndex>(b);
    return feature_a.mesh != nullptr && feature_b.mesh != nullptr &&
           feature_a.kind == feature_b.kind &&
           transparency_bucket(feature_a.alpha) ==
               transparency_bucket(feature_b.alpha) &&
           feature_a.visibility.texture == feature_b.visibility.texture &&
           feature_a.visibility.size == feature_b.visibility.size &&
           feature_a.visibility.tile_size == feature_b.visibility.tile_size &&
           feature_a.visibility.explored_alpha ==
               feature_b.visibility.explored_alpha &&
           feature_a.visibility.enabled == feature_b.visibility.enabled;
  }

  void clear_sort_id_maps() {
    m_material_ids.clear();
    m_mesh_ids.clear();
    m_texture_ids.clear();
    m_next_material_id = 1;
    m_next_mesh_id = 1;
    m_next_texture_id = 1;
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

  auto intern_material_id(const void *ptr) -> std::uint32_t {
    return intern_id(m_material_ids, m_next_material_id, ptr);
  }

  auto intern_mesh_id(const void *ptr) -> std::uint32_t {
    return intern_id(m_mesh_ids, m_next_mesh_id, ptr);
  }

  auto intern_texture_id(const void *ptr) -> std::uint32_t {
    return intern_id(m_texture_ids, m_next_texture_id, ptr);
  }

  static auto intern_id(std::unordered_map<const void *, std::uint32_t> &ids,
                        std::uint32_t &next, const void *ptr) -> std::uint32_t {
    if (ptr == nullptr) {
      return 0;
    }
    auto found = ids.find(ptr);
    if (found != ids.end()) {
      return found->second;
    }

    const std::uint32_t id = next;
    if (next != std::numeric_limits<std::uint32_t>::max()) {
      ++next;
    }
    ids.emplace(ptr, id);
    return id;
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sort_indices;
  std::vector<uint64_t> m_sort_keys;
  std::vector<PreparedBatch> m_prepared_batches;
  std::unordered_map<const void *, std::uint32_t> m_material_ids;
  std::unordered_map<const void *, std::uint32_t> m_mesh_ids;
  std::unordered_map<const void *, std::uint32_t> m_texture_ids;
  std::uint32_t m_next_material_id = 1;
  std::uint32_t m_next_mesh_id = 1;
  std::uint32_t m_next_texture_id = 1;
};

} // namespace Render::GL
