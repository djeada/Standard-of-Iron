#pragma once

#include "ground/firecamp_gpu.h"
#include "ground/grass_gpu.h"
#include "ground/olive_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
#include "primitive_batch.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace Render::GL {
class Mesh;
class Texture;
class Buffer;
class Shader;
} // namespace Render::GL

namespace Render::GL {

constexpr int k_sort_key_bucket_shift = 56;

struct MeshCmd {
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float alpha = 1.0F;
  int material_id = 0;
  class Shader *shader = nullptr;
};

struct CylinderCmd {
  QVector3D start{0.0F, -0.5F, 0.0F};
  QVector3D end{0.0F, 0.5F, 0.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};
  float radius = 1.0F;
  float alpha = 1.0F;
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
};

struct GrassBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  GrassBatchParams params;
};

struct StoneBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  StoneBatchParams params;
};

struct PlantBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  PlantBatchParams params;
};

struct PineBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  PineBatchParams params;
};

struct OliveBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  OliveBatchParams params;
};

struct FireCampBatchCmd {
  Buffer *instance_buffer = nullptr;
  std::size_t instance_count = 0;
  FireCampBatchParams params;
};

struct TerrainChunkCmd {
  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  TerrainChunkParams params;
  std::uint16_t sort_key = 0x8000U;
  bool depth_write = true;
  float depth_bias = 0.0F;
};

struct GridCmd {

  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0.2F, 0.25F, 0.2F};
  float cell_size = 1.0F;
  float thickness = 0.06F;
  float extent = 50.0F;
};

struct SelectionRingCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0, 0, 0};
  float alpha_inner = 0.6F;
  float alpha_outer = 0.25F;
};

struct SelectionSmokeCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float base_alpha = 0.15F;
};

struct HealingBeamCmd {
  QVector3D start_pos{0, 0, 0};
  QVector3D end_pos{0, 0, 0};
  QVector3D color{0.4F, 1.0F, 0.5F};
  float progress = 1.0F;
  float beam_width = 0.15F;
  float intensity = 1.0F;
  float time = 0.0F;
};

struct HealerAuraCmd {
  QVector3D position{0, 0, 0};
  QVector3D color{0.4F, 1.0F, 0.5F};
  float radius = 5.0F;
  float intensity = 1.0F;
  float time = 0.0F;
};

using DrawCmd = std::variant<GridCmd, SelectionRingCmd, SelectionSmokeCmd,
                             CylinderCmd, MeshCmd, FogBatchCmd, GrassBatchCmd,
                             StoneBatchCmd, PlantBatchCmd, PineBatchCmd,
                             OliveBatchCmd, FireCampBatchCmd, TerrainChunkCmd,
                             PrimitiveBatchCmd, HealingBeamCmd, HealerAuraCmd>;

enum class DrawCmdType : std::uint8_t {
  Grid = 0,
  SelectionRing = 1,
  SelectionSmoke = 2,
  Cylinder = 3,
  Mesh = 4,
  FogBatch = 5,
  GrassBatch = 6,
  StoneBatch = 7,
  PlantBatch = 8,
  PineBatch = 9,
  OliveBatch = 10,
  FireCampBatch = 11,
  TerrainChunk = 12,
  PrimitiveBatch = 13,
  HealingBeam = 14,
  HealerAura = 15
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
constexpr std::size_t GrassBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::GrassBatch);
constexpr std::size_t StoneBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::StoneBatch);
constexpr std::size_t PlantBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::PlantBatch);
constexpr std::size_t PineBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::PineBatch);
constexpr std::size_t OliveBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::OliveBatch);
constexpr std::size_t FireCampBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::FireCampBatch);
constexpr std::size_t TerrainChunkCmdIndex =
    static_cast<std::size_t>(DrawCmdType::TerrainChunk);
constexpr std::size_t PrimitiveBatchCmdIndex =
    static_cast<std::size_t>(DrawCmdType::PrimitiveBatch);
constexpr std::size_t HealingBeamCmdIndex =
    static_cast<std::size_t>(DrawCmdType::HealingBeam);
constexpr std::size_t HealerAuraCmdIndex =
    static_cast<std::size_t>(DrawCmdType::HealerAura);

inline auto draw_cmd_type(const DrawCmd &cmd) -> DrawCmdType {
  return static_cast<DrawCmdType>(cmd.index());
}

class DrawQueue {
public:
  void clear() { m_items.clear(); }

  void submit(const MeshCmd &c) { m_items.emplace_back(c); }
  void submit(const GridCmd &c) { m_items.emplace_back(c); }
  void submit(const SelectionRingCmd &c) { m_items.emplace_back(c); }
  void submit(const SelectionSmokeCmd &c) { m_items.emplace_back(c); }
  void submit(const CylinderCmd &c) { m_items.emplace_back(c); }
  void submit(const FogBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const GrassBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const StoneBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const PlantBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const PineBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const OliveBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const FireCampBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const TerrainChunkCmd &c) { m_items.emplace_back(c); }
  void submit(const PrimitiveBatchCmd &c) { m_items.emplace_back(c); }
  void submit(const HealingBeamCmd &c) { m_items.emplace_back(c); }
  void submit(const HealerAuraCmd &c) { m_items.emplace_back(c); }

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

private:
  void radix_sort_two_pass(std::size_t count) {
    constexpr int BUCKETS = 256;

    m_temp_indices.resize(count);

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket =
            static_cast<uint8_t>(m_sort_keys[i] >> k_sort_key_bucket_shift);
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket = static_cast<uint8_t>(
            m_sort_keys[m_sort_indices[i]] >> k_sort_key_bucket_shift);
        m_temp_indices[offsets[bucket]++] = m_sort_indices[i];
      }
    }

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sort_keys[m_temp_indices[i]] >> 48) & 0xFF;
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sort_keys[m_temp_indices[i]] >> 48) & 0xFF;
        m_sort_indices[offsets[bucket]++] = m_temp_indices[i];
      }
    }
  }

  [[nodiscard]] static auto compute_sort_key(const DrawCmd &cmd) -> uint64_t {

    enum class RenderOrder : uint8_t {
      TerrainChunk = 0,
      GrassBatch = 1,
      StoneBatch = 2,
      PlantBatch = 3,
      PineBatch = 4,
      OliveBatch = 5,
      FireCampBatch = 6,
      PrimitiveBatch = 7,
      Mesh = 8,
      Cylinder = 9,
      FogBatch = 10,
      SelectionSmoke = 11,
      Grid = 12,
      SelectionRing = 15
    };

    static constexpr uint8_t k_type_order[] = {
        static_cast<uint8_t>(RenderOrder::Grid),
        static_cast<uint8_t>(RenderOrder::SelectionRing),
        static_cast<uint8_t>(RenderOrder::SelectionSmoke),
        static_cast<uint8_t>(RenderOrder::Cylinder),
        static_cast<uint8_t>(RenderOrder::Mesh),
        static_cast<uint8_t>(RenderOrder::FogBatch),
        static_cast<uint8_t>(RenderOrder::GrassBatch),
        static_cast<uint8_t>(RenderOrder::StoneBatch),
        static_cast<uint8_t>(RenderOrder::PlantBatch),
        static_cast<uint8_t>(RenderOrder::PineBatch),
        static_cast<uint8_t>(RenderOrder::OliveBatch),
        static_cast<uint8_t>(RenderOrder::FireCampBatch),
        static_cast<uint8_t>(RenderOrder::TerrainChunk),
        static_cast<uint8_t>(RenderOrder::PrimitiveBatch)};

    const std::size_t type_index = cmd.index();
    constexpr std::size_t type_count =
        sizeof(k_type_order) / sizeof(k_type_order[0]);
    const uint8_t type_order = type_index < type_count
                                   ? k_type_order[type_index]
                                   : static_cast<uint8_t>(type_index);

    uint64_t key = static_cast<uint64_t>(type_order) << 56;

    if (cmd.index() == MeshCmdIndex) {
      const auto &mesh = std::get<MeshCmdIndex>(cmd);

      uint64_t const tex_ptr =
          reinterpret_cast<uintptr_t>(mesh.texture) & 0x0000FFFFFFFFFFFF;
      key |= tex_ptr;
    } else if (cmd.index() == GrassBatchCmdIndex) {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(grass.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == StoneBatchCmdIndex) {
      const auto &stone = std::get<StoneBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(stone.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == PlantBatchCmdIndex) {
      const auto &plant = std::get<PlantBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(plant.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == PineBatchCmdIndex) {
      const auto &pine = std::get<PineBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(pine.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == OliveBatchCmdIndex) {
      const auto &olive = std::get<OliveBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(olive.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == FireCampBatchCmdIndex) {
      const auto &firecamp = std::get<FireCampBatchCmdIndex>(cmd);
      uint64_t const buffer_ptr =
          reinterpret_cast<uintptr_t>(firecamp.instance_buffer) &
          0x0000FFFFFFFFFFFF;
      key |= buffer_ptr;
    } else if (cmd.index() == TerrainChunkCmdIndex) {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);
      auto const sort_byte =
          static_cast<uint64_t>((terrain.sort_key >> 8) & 0xFFU);
      key |= sort_byte << 48;
      uint64_t const mesh_ptr =
          reinterpret_cast<uintptr_t>(terrain.mesh) & 0x0000FFFFFFFFFFFFU;
      key |= mesh_ptr;
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto &prim = std::get<PrimitiveBatchCmdIndex>(cmd);

      key |= static_cast<uint64_t>(prim.type) << 48;

      key |= static_cast<uint64_t>(prim.instance_count() & 0xFFFFFFFF);
    }

    return key;
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sort_indices;
  std::vector<uint64_t> m_sort_keys;
  std::vector<uint32_t> m_temp_indices;
};

} // namespace Render::GL
