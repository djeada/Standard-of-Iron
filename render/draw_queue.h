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
  int materialId = 0;
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
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  GrassBatchParams params;
};

struct StoneBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  StoneBatchParams params;
};

struct PlantBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  PlantBatchParams params;
};

struct PineBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  PineBatchParams params;
};

struct OliveBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  OliveBatchParams params;
};

struct FireCampBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instance_count = 0;
  FireCampBatchParams params;
};

struct TerrainChunkCmd {
  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  TerrainChunkParams params;
  std::uint16_t sortKey = 0x8000U;
  bool depthWrite = true;
  float depthBias = 0.0F;
};

struct GridCmd {

  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0.2F, 0.25F, 0.2F};
  float cellSize = 1.0F;
  float thickness = 0.06F;
  float extent = 50.0F;
};

struct SelectionRingCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0, 0, 0};
  float alphaInner = 0.6F;
  float alphaOuter = 0.25F;
};

struct SelectionSmokeCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float baseAlpha = 0.15F;
};

using DrawCmd =
    std::variant<GridCmd, SelectionRingCmd, SelectionSmokeCmd, CylinderCmd,
                 MeshCmd, FogBatchCmd, GrassBatchCmd, StoneBatchCmd,
                 PlantBatchCmd, PineBatchCmd, OliveBatchCmd, FireCampBatchCmd,
                 TerrainChunkCmd, PrimitiveBatchCmd>;

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
  PrimitiveBatch = 13
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

inline auto drawCmdType(const DrawCmd &cmd) -> DrawCmdType {
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

  [[nodiscard]] auto empty() const -> bool { return m_items.empty(); }
  [[nodiscard]] auto size() const -> std::size_t { return m_items.size(); }

  [[nodiscard]] auto getSorted(std::size_t i) const -> const DrawCmd & {
    return m_items[m_sortIndices[i]];
  }

  [[nodiscard]] auto items() const -> const std::vector<DrawCmd> & {
    return m_items;
  }

  void sortForBatching() {
    const std::size_t count = m_items.size();

    m_sortKeys.resize(count);
    m_sortIndices.resize(count);

    for (std::size_t i = 0; i < count; ++i) {
      m_sortIndices[i] = static_cast<uint32_t>(i);
      m_sortKeys[i] = computeSortKey(m_items[i]);
    }

    if (count >= 2) {
      radixSortTwoPass(count);
    }
  }

private:
  void radixSortTwoPass(std::size_t count) {
    constexpr int BUCKETS = 256;

    m_tempIndices.resize(count);

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket =
            static_cast<uint8_t>(m_sortKeys[i] >> k_sort_key_bucket_shift);
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        auto const bucket = static_cast<uint8_t>(m_sortKeys[m_sortIndices[i]] >>
                                                 k_sort_key_bucket_shift);
        m_tempIndices[offsets[bucket]++] = m_sortIndices[i];
      }
    }

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sortKeys[m_tempIndices[i]] >> 48) & 0xFF;
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t const bucket =
            static_cast<uint8_t>(m_sortKeys[m_tempIndices[i]] >> 48) & 0xFF;
        m_sortIndices[offsets[bucket]++] = m_tempIndices[i];
      }
    }
  }

  [[nodiscard]] static auto computeSortKey(const DrawCmd &cmd) -> uint64_t {

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

    static constexpr uint8_t kTypeOrder[] = {
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

    const std::size_t typeIndex = cmd.index();
    constexpr std::size_t typeCount =
        sizeof(kTypeOrder) / sizeof(kTypeOrder[0]);
    const uint8_t typeOrder = typeIndex < typeCount
                                  ? kTypeOrder[typeIndex]
                                  : static_cast<uint8_t>(typeIndex);

    uint64_t key = static_cast<uint64_t>(typeOrder) << 56;

    if (cmd.index() == MeshCmdIndex) {
      const auto &mesh = std::get<MeshCmdIndex>(cmd);

      uint64_t const texPtr =
          reinterpret_cast<uintptr_t>(mesh.texture) & 0x0000FFFFFFFFFFFF;
      key |= texPtr;
    } else if (cmd.index() == GrassBatchCmdIndex) {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(grass.instanceBuffer) &
          0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == StoneBatchCmdIndex) {
      const auto &stone = std::get<StoneBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(stone.instanceBuffer) &
          0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == PlantBatchCmdIndex) {
      const auto &plant = std::get<PlantBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(plant.instanceBuffer) &
          0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == PineBatchCmdIndex) {
      const auto &pine = std::get<PineBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(pine.instanceBuffer) & 0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == OliveBatchCmdIndex) {
      const auto &olive = std::get<OliveBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(olive.instanceBuffer) &
          0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == FireCampBatchCmdIndex) {
      const auto &firecamp = std::get<FireCampBatchCmdIndex>(cmd);
      uint64_t const bufferPtr =
          reinterpret_cast<uintptr_t>(firecamp.instanceBuffer) &
          0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == TerrainChunkCmdIndex) {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);
      auto const sortByte =
          static_cast<uint64_t>((terrain.sortKey >> 8) & 0xFFU);
      key |= sortByte << 48;
      uint64_t const meshPtr =
          reinterpret_cast<uintptr_t>(terrain.mesh) & 0x0000FFFFFFFFFFFFU;
      key |= meshPtr;
    } else if (cmd.index() == PrimitiveBatchCmdIndex) {
      const auto &prim = std::get<PrimitiveBatchCmdIndex>(cmd);

      key |= static_cast<uint64_t>(prim.type) << 48;

      key |= static_cast<uint64_t>(prim.instanceCount() & 0xFFFFFFFF);
    }

    return key;
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sortIndices;
  std::vector<uint64_t> m_sortKeys;
  std::vector<uint32_t> m_tempIndices;
};

} // namespace Render::GL
