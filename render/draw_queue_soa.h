#pragma once

#include "ground/grass_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
<parameter name = "cstddef">
#include <cstdint>
#include <vector>

    namespace Render::GL {
  class Mesh;
  class Texture;
  class Buffer;
}

namespace Render::GL {

struct MeshCmd {
  Mesh *mesh = nullptr;
  Texture *texture = nullptr;
  QMatrix4x4 model;
  QMatrix4x4 mvp;
  QVector3D color{1, 1, 1};
  float alpha = 1.0F;
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

struct TerrainChunkCmd {
  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  TerrainChunkParams params;
  std::uint16_t sortKey = 0x8000u;
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

class DrawQueueSoA {
public:
  void clear() {
    m_gridCmds.clear();
    m_selectionRingCmds.clear();
    m_selectionSmokeCmds.clear();
    m_cylinderCmds.clear();
    m_meshCmds.clear();
    m_fogBatchCmds.clear();
    m_grassBatchCmds.clear();
    m_stoneBatchCmds.clear();
    m_plantBatchCmds.clear();
    m_terrainChunkCmds.clear();
  }

  void submit(const GridCmd &cmd) { m_gridCmds.push_back(cmd); }
  void submit(const SelectionRingCmd &cmd) {
    m_selectionRingCmds.push_back(cmd);
  }
  void submit(const SelectionSmokeCmd &cmd) {
    m_selectionSmokeCmds.push_back(cmd);
  }
  void submit(const CylinderCmd &cmd) { m_cylinderCmds.push_back(cmd); }
  void submit(const MeshCmd &cmd) { m_meshCmds.push_back(cmd); }
  void submit(const FogBatchCmd &cmd) { m_fogBatchCmds.push_back(cmd); }
  void submit(const GrassBatchCmd &cmd) { m_grassBatchCmds.push_back(cmd); }
  void submit(const StoneBatchCmd &cmd) { m_stoneBatchCmds.push_back(cmd); }
  void submit(const PlantBatchCmd &cmd) { m_plantBatchCmds.push_back(cmd); }
  void submit(const TerrainChunkCmd &cmd) { m_terrainChunkCmds.push_back(cmd); }

  bool empty() const {
    return m_gridCmds.empty() && m_selectionRingCmds.empty() &&
           m_selectionSmokeCmds.empty() && m_cylinderCmds.empty() &&
           m_meshCmds.empty() && m_fogBatchCmds.empty() &&
           m_grassBatchCmds.empty() && m_stoneBatchCmds.empty() &&
           m_plantBatchCmds.empty() && m_terrainChunkCmds.empty();
  }

  void sortForBatching() {

    std::sort(m_meshCmds.begin(), m_meshCmds.end(),
              [](const MeshCmd &a, const MeshCmd &b) {
                return reinterpret_cast<uintptr_t>(a.texture) <
                       reinterpret_cast<uintptr_t>(b.texture);
              });

    std::sort(m_terrainChunkCmds.begin(), m_terrainChunkCmds.end(),
              [](const TerrainChunkCmd &a, const TerrainChunkCmd &b) {
                return a.sortKey < b.sortKey;
              });
  }

  const std::vector<GridCmd> &gridCmds() const { return m_gridCmds; }
  const std::vector<SelectionRingCmd> &selectionRingCmds() const {
    return m_selectionRingCmds;
  }
  const std::vector<SelectionSmokeCmd> &selectionSmokeCmds() const {
    return m_selectionSmokeCmds;
  }
  const std::vector<CylinderCmd> &cylinderCmds() const {
    return m_cylinderCmds;
  }
  const std::vector<MeshCmd> &meshCmds() const { return m_meshCmds; }
  const std::vector<FogBatchCmd> &fogBatchCmds() const {
    return m_fogBatchCmds;
  }
  const std::vector<GrassBatchCmd> &grassBatchCmds() const {
    return m_grassBatchCmds;
  }
  const std::vector<StoneBatchCmd> &stoneBatchCmds() const {
    return m_stoneBatchCmds;
  }
  const std::vector<PlantBatchCmd> &plantBatchCmds() const {
    return m_plantBatchCmds;
  }
  const std::vector<TerrainChunkCmd> &terrainChunkCmds() const {
    return m_terrainChunkCmds;
  }

private:
  std::vector<GridCmd> m_gridCmds;
  std::vector<SelectionRingCmd> m_selectionRingCmds;
  std::vector<SelectionSmokeCmd> m_selectionSmokeCmds;
  std::vector<CylinderCmd> m_cylinderCmds;
  std::vector<MeshCmd> m_meshCmds;
  std::vector<FogBatchCmd> m_fogBatchCmds;
  std::vector<GrassBatchCmd> m_grassBatchCmds;
  std::vector<StoneBatchCmd> m_stoneBatchCmds;
  std::vector<PlantBatchCmd> m_plantBatchCmds;
  std::vector<TerrainChunkCmd> m_terrainChunkCmds;
};

} // namespace Render::GL
