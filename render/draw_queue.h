#pragma once

#include "decoration_gpu.h"
#include "draw_part.h"
#include "frame_budget.h"
#include "primitive_batch.h"
#include "rain_gpu.h"
#include "world_chunk.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
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

struct DecorationBatchCmd {
  // Stage 18 — unified static decoration batch. Replaces the separate
  // Grass/Stone/Plant/Pine/Olive/FireCamp BatchCmds. The backend
  // dispatcher routes on `kind` to the appropriate sub-pipeline; all
  // share the same (mesh, instance_buffer, per-instance pos/scale/variant)
  // skeleton. Per-kind params (wind, flicker, etc.) live in the
  // matching struct below and are only meaningful for their kind.
  enum class Kind : std::uint8_t {
    Grass = 0,
    Stone,
    Plant,
    Pine,
    Olive,
    FireCamp
  };

  Kind kind = Kind::Grass;
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

struct WorldChunkCmd {
  // Stage 18 — terrain folds in here. A WorldChunkCmd carries a Mesh
  // plus terrain Material + full TerrainChunkParams (the terrain
  // shader is specialised and consumes the whole param set). Future
  // static-world chunks (rock fields, roads) can reuse this cmd.
  Mesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 model;
  BoundingBox aabb;
  TerrainChunkParams params;
  std::uint16_t sort_key = 0x8000U;
  bool depth_write = true;
  float depth_bias = 0.0F;
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
  // Stage 18 — merges HealingBeam / HealerAura / CombatDust /
  // BuildingFlame / StoneImpact. Each still renders through its own
  // backend sub-pipeline; this wrapper unifies the DrawCmd variant
  // slot and routes on `kind`.
  enum class Kind : std::uint8_t {
    HealingBeam = 0,
    HealerAura,
    CombatDust,
    BuildingFlame,
    StoneImpact
  };

  Kind kind = Kind::HealerAura;

  // Positional data. `position` is used by aura/dust/flame/impact and
  // as beam start; `end_pos` is the beam end.
  QVector3D position{0.0F, 0.0F, 0.0F};
  QVector3D end_pos{0.0F, 0.0F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};

  // Shared scalar params.
  float radius = 1.0F;
  float intensity = 1.0F;
  float time = 0.0F;

  // Beam-only.
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

// Stage 15.5b — GPU-skinned rigged creature draw. `bone_palette` points to
// a contiguous array of `bone_count` world-space bone matrices owned by the
// caller; the data must outlive the DrawQueue submit→flush cycle (same
// contract as DrawPartCmd's BonePaletteRef). No production callsite emits
// this yet — that's 15.5c. Until then the dispatch branch is exercised only
// by tests, but it is wired through Backend::execute so the
// std::variant<...> visit path and sort-key table are live.
struct RiggedCreatureCmd {
  RiggedMesh *mesh = nullptr;
  const Material *material = nullptr;
  QMatrix4x4 world;
  // CPU-side bone palette pointer. Owned by Renderer::bone_palette_arena()
  // and pointer-stable for the submit→dispatch lifetime. Software backend,
  // tests, and any non-GPU consumer read from here.
  const QMatrix4x4 *bone_palette = nullptr;
  // GPU-side palette handle (Stage 16.2). `palette_ubo == 0` means the
  // arena had no GL context when allocating; the pipeline then skips the
  // bind and the shader falls back to identity skinning. `palette_offset`
  // is the byte offset into the slab UBO for this palette.
  std::uint32_t palette_ubo = 0;
  std::uint32_t palette_offset = 0;
  std::uint32_t bone_count = 0;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha = 1.0F;
  Texture *texture = nullptr;
  std::int32_t material_id = 0;
  QVector3D variation_scale{1.0F, 1.0F, 1.0F};
  CommandPriority priority{CommandPriority::Normal};
};

using DrawCmd =
    std::variant<GridCmd, SelectionRingCmd, SelectionSmokeCmd, CylinderCmd,
                 MeshCmd, FogBatchCmd, DecorationBatchCmd, RainBatchCmd,
                 WorldChunkCmd, PrimitiveBatchCmd, EffectBatchCmd,
                 ModeIndicatorCmd, DrawPartCmd, RiggedCreatureCmd>;

enum class DrawCmdType : std::uint8_t {
  Grid = 0,
  SelectionRing = 1,
  SelectionSmoke = 2,
  Cylinder = 3,
  Mesh = 4,
  FogBatch = 5,
  DecorationBatch = 6,
  RainBatch = 7,
  WorldChunk = 8,
  PrimitiveBatch = 9,
  EffectBatch = 10,
  ModeIndicator = 11,
  DrawPart = 12,
  RiggedCreature = 13
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
constexpr std::size_t DecorationBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::DecorationBatch);
constexpr std::size_t RainBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::RainBatch);
constexpr std::size_t WorldChunkCmdIndex =
    static_cast<std::size_t>(DrawCmdType::WorldChunk);
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
  return std::visit(
      [](const auto &c) -> CommandPriority { return c.priority; }, cmd);
}

class DrawQueue {
public:
  void clear() { m_items.clear(); }

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

  void sort_for_batching() {
    const std::size_t count = m_items.size();

    m_sort_keys.resize(count);
    m_sort_indices.resize(count);

    for (std::size_t i = 0; i < count; ++i) {
      m_sort_indices[i] = static_cast<uint32_t>(i);
      m_sort_keys[i] = compute_sort_key(m_items[i]);
    }

    if (count >= 2) {
      radix_sort_two_pass(count);
    }
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
  void radix_sort_two_pass(std::size_t count) {
    constexpr int BUCKETS = 256;

    m_temp_indices.resize(count);

    // LSD radix sort: sort by the lower byte (bits 48-55) first, then by the
    // higher byte (bits 56-63 = type_order). The final (most significant)
    // pass dominates, so type_order becomes the primary sort criterion while
    // the lower byte (terrain sort_byte, primitive type, …) acts as a
    // tie-breaker within each type.
    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sort_keys[i] >> 48) & 0xFF;
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sort_keys[m_sort_indices[i]] >> 48) & 0xFF;
        m_temp_indices[offsets[bucket]++] = m_sort_indices[i];
      }
    }

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket = static_cast<uint8_t>(
            m_sort_keys[m_temp_indices[i]] >> k_sort_key_bucket_shift);
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket = static_cast<uint8_t>(
            m_sort_keys[m_temp_indices[i]] >> k_sort_key_bucket_shift);
        m_sort_indices[offsets[bucket]++] = m_temp_indices[i];
      }
    }
  }

  [[nodiscard]] static auto compute_sort_key(const DrawCmd &cmd) -> uint64_t {

    enum class RenderOrder : uint8_t {
      WorldChunk = 0,
      DecorationBatch = 1,
      RainBatch = 2,
      PrimitiveBatch = 3,
      Mesh = 4,
      Cylinder = 5,
      FogBatch = 6,
      SelectionSmoke = 7,
      Grid = 8,
      EffectBatch = 9,
      SelectionRing = 16,
      ModeIndicator = 17
    };

    // Indexed by DrawCmd::index(); must match DrawCmdType ordering.
    static constexpr uint8_t k_type_order[] = {
        static_cast<uint8_t>(RenderOrder::Grid),
        static_cast<uint8_t>(RenderOrder::SelectionRing),
        static_cast<uint8_t>(RenderOrder::SelectionSmoke),
        static_cast<uint8_t>(RenderOrder::Cylinder),
        static_cast<uint8_t>(RenderOrder::Mesh),
        static_cast<uint8_t>(RenderOrder::FogBatch),
        static_cast<uint8_t>(RenderOrder::DecorationBatch),
        static_cast<uint8_t>(RenderOrder::RainBatch),
        static_cast<uint8_t>(RenderOrder::WorldChunk),
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

    // Type order is the primary sort criterion — this preserves the required
    // draw order (terrain -> units -> selection rings -> UI overlays).
    // Priority is metadata used only by the frame-budget defer logic and
    // MUST NOT influence sort order, otherwise critical commands (e.g.
    // selection rings) end up rendered under the terrain.
    uint64_t key = static_cast<uint64_t>(type_order) << 56;

    if (cmd.index() == MeshCmdIndex) {
      const auto &mesh = std::get<MeshCmdIndex>(cmd);

      auto ptr_bits16 = [](const void *p) -> uint64_t {
        return (reinterpret_cast<uintptr_t>(p) >> 4) & 0xFFFF;
      };
      key |= ptr_bits16(mesh.shader) << 32;
      key |= ptr_bits16(mesh.mesh) << 16;
      key |= ptr_bits16(mesh.texture);
    } else if (cmd.index() == DecorationBatchCmdIndex) {
      const auto &deco = std::get<DecorationBatchCmdIndex>(cmd);
      // Sub-sort by kind so same-kind draws batch contiguously.
      key |= static_cast<uint64_t>(deco.kind) << 48;
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(deco.instance_buffer) &
          0x0000FFFFFFFFFFFFU;
      key |= buffer_ptr;
    } else if (cmd.index() == WorldChunkCmdIndex) {
      const auto &chunk = std::get<WorldChunkCmdIndex>(cmd);
      auto const sort_byte =
          static_cast<uint64_t>((chunk.sort_key >> 8) & 0xFFU);
      key |= sort_byte << 48;
      uint64_t const mesh_ptr =
          reinterpret_cast<uintptr_t>(chunk.mesh) & 0x0000FFFFFFFFFFFFU;
      key |= mesh_ptr;
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto &prim = std::get<PrimitiveBatchCmdIndex>(cmd);

      key |= static_cast<uint64_t>(prim.type) << 48;

      key |= static_cast<uint64_t>(prim.instance_count() & 0xFFFFFFFF);
    } else if (cmd.index() == DrawPartCmdIndex) {
      const auto &part = std::get<DrawPartCmdIndex>(cmd);
      // Batch DrawPartCmds by material. The two-pass radix sort only
      // examines the top two bytes of the key (bits 48-63), so the
      // material-discriminating bits MUST live in the 48-55 byte to
      // actually influence ordering. Mesh pointer goes into the lower
      // bits as batch-locality metadata (unused by sort, useful for
      // debugging / can_batch predicates).
      auto ptr_byte = [](const void *p) -> uint64_t {
        return (reinterpret_cast<uintptr_t>(p) >> 4) & 0xFFU;
      };
      key |= ptr_byte(part.material) << 48;
      key |= (reinterpret_cast<uintptr_t>(part.mesh) & 0xFFFFU) << 16;
    } else if (cmd.index() == RiggedCreatureCmdIndex) {
      // Stage 15.5b — batch rigged creatures by material/mesh so
      // adjacent draws can share shader + VAO binds. Mirrors DrawPartCmd's
      // material-byte-into-48 convention.
      const auto &rig = std::get<RiggedCreatureCmdIndex>(cmd);
      auto ptr_byte = [](const void *p) -> uint64_t {
        return (reinterpret_cast<uintptr_t>(p) >> 4) & 0xFFU;
      };
      key |= ptr_byte(rig.material) << 48;
      key |= (reinterpret_cast<uintptr_t>(rig.mesh) & 0xFFFFU) << 16;
    }

    return key;
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sort_indices;
  std::vector<uint64_t> m_sort_keys;
  std::vector<uint32_t> m_temp_indices;
};

} // namespace Render::GL
