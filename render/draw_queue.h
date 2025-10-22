#pragma once

#include "ground/grass_gpu.h"
#include "ground/pine_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
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

struct MeshCmd {
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float alpha = 1.0f;
  class Shader *shader = nullptr;
};

struct CylinderCmd {
  QVector3D start{0.0f, -0.5f, 0.0f};
  QVector3D end{0.0f, 0.5f, 0.0f};
  QVector3D color{1.0f, 1.0f, 1.0f};
  float radius = 1.0f;
  float alpha = 1.0f;
};

struct FogInstanceData {
  QVector3D center{0.0f, 0.25f, 0.0f};
  QVector3D color{0.05f, 0.05f, 0.05f};
  float alpha = 1.0f;
  float size = 1.0f;
};

struct FogBatchCmd {
  const FogInstanceData *instances = nullptr;
  std::size_t count = 0;
};

struct GrassBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instanceCount = 0;
  GrassBatchParams params;
};

struct StoneBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instanceCount = 0;
  StoneBatchParams params;
};

struct PlantBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instanceCount = 0;
  PlantBatchParams params;
};

struct PineBatchCmd {
  Buffer *instanceBuffer = nullptr;
  std::size_t instanceCount = 0;
  PineBatchParams params;
};

struct TerrainChunkCmd {
  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  TerrainChunkParams params;
  std::uint16_t sortKey = 0x8000u;
  bool depthWrite = true;
  float depthBias = 0.0f;
};

struct GridCmd {

  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0.2f, 0.25f, 0.2f};
  float cellSize = 1.0f;
  float thickness = 0.06f;
  float extent = 50.0f;
};

struct SelectionRingCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{0, 0, 0};
  float alphaInner = 0.6f;
  float alphaOuter = 0.25f;
};

struct SelectionSmokeCmd {
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float baseAlpha = 0.15f;
};

using DrawCmd =
    std::variant<GridCmd, SelectionRingCmd, SelectionSmokeCmd, CylinderCmd,
                 MeshCmd, FogBatchCmd, GrassBatchCmd, StoneBatchCmd,
                 PlantBatchCmd, PineBatchCmd, TerrainChunkCmd>;

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
  TerrainChunk = 10
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
constexpr std::size_t TerrainChunkCmdIndex =
    static_cast<std::size_t>(DrawCmdType::TerrainChunk);

inline DrawCmdType drawCmdType(const DrawCmd &cmd) {
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
  void submit(const TerrainChunkCmd &c) { m_items.emplace_back(c); }

  bool empty() const { return m_items.empty(); }
  std::size_t size() const { return m_items.size(); }

  const DrawCmd &getSorted(std::size_t i) const {
    return m_items[m_sortIndices[i]];
  }

  const std::vector<DrawCmd> &items() const { return m_items; }

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
        uint8_t bucket = static_cast<uint8_t>(m_sortKeys[i] >> 56);
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t bucket =
            static_cast<uint8_t>(m_sortKeys[m_sortIndices[i]] >> 56);
        m_tempIndices[offsets[bucket]++] = m_sortIndices[i];
      }
    }

    {
      int histogram[BUCKETS] = {0};

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t bucket =
            static_cast<uint8_t>(m_sortKeys[m_tempIndices[i]] >> 48) & 0xFF;
        ++histogram[bucket];
      }

      int offsets[BUCKETS];
      offsets[0] = 0;
      for (int i = 1; i < BUCKETS; ++i) {
        offsets[i] = offsets[i - 1] + histogram[i - 1];
      }

      for (std::size_t i = 0; i < count; ++i) {
        uint8_t bucket =
            static_cast<uint8_t>(m_sortKeys[m_tempIndices[i]] >> 48) & 0xFF;
        m_sortIndices[offsets[bucket]++] = m_tempIndices[i];
      }
    }
  }

  uint64_t computeSortKey(const DrawCmd &cmd) const {

    enum class RenderOrder : uint8_t {
      TerrainChunk = 0,
      GrassBatch = 1,
      StoneBatch = 2,
      PlantBatch = 3,
      PineBatch = 4,
      SelectionRing = 5,
      FogBatch = 6,
      Mesh = 7,
      Cylinder = 8,
      SelectionSmoke = 9,
      Grid = 10
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
        static_cast<uint8_t>(RenderOrder::TerrainChunk)};

    const std::size_t typeIndex = cmd.index();
    constexpr std::size_t typeCount =
        sizeof(kTypeOrder) / sizeof(kTypeOrder[0]);
    const uint8_t typeOrder = typeIndex < typeCount
                                  ? kTypeOrder[typeIndex]
                                  : static_cast<uint8_t>(typeIndex);

    uint64_t key = static_cast<uint64_t>(typeOrder) << 56;

    if (cmd.index() == MeshCmdIndex) {
      const auto &mesh = std::get<MeshCmdIndex>(cmd);

      uint64_t texPtr =
          reinterpret_cast<uintptr_t>(mesh.texture) & 0x0000FFFFFFFFFFFF;
      key |= texPtr;
    } else if (cmd.index() == GrassBatchCmdIndex) {
      const auto &grass = std::get<GrassBatchCmdIndex>(cmd);
      uint64_t bufferPtr = reinterpret_cast<uintptr_t>(grass.instanceBuffer) &
                           0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == StoneBatchCmdIndex) {
      const auto &stone = std::get<StoneBatchCmdIndex>(cmd);
      uint64_t bufferPtr = reinterpret_cast<uintptr_t>(stone.instanceBuffer) &
                           0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == PlantBatchCmdIndex) {
      const auto &plant = std::get<PlantBatchCmdIndex>(cmd);
      uint64_t bufferPtr = reinterpret_cast<uintptr_t>(plant.instanceBuffer) &
                           0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == PineBatchCmdIndex) {
      const auto &pine = std::get<PineBatchCmdIndex>(cmd);
      uint64_t bufferPtr =
          reinterpret_cast<uintptr_t>(pine.instanceBuffer) & 0x0000FFFFFFFFFFFF;
      key |= bufferPtr;
    } else if (cmd.index() == TerrainChunkCmdIndex) {
      const auto &terrain = std::get<TerrainChunkCmdIndex>(cmd);
      uint64_t sortByte = static_cast<uint64_t>((terrain.sortKey >> 8) & 0xFFu);
      key |= sortByte << 48;
      uint64_t meshPtr =
          reinterpret_cast<uintptr_t>(terrain.mesh) & 0x0000FFFFFFFFFFFFu;
      key |= meshPtr;
    }

    return key;
  }

  std::vector<DrawCmd> m_items;
  std::vector<uint32_t> m_sortIndices;
  std::vector<uint64_t> m_sortKeys;
  std::vector<uint32_t> m_tempIndices;
};

} // namespace Render::GL
