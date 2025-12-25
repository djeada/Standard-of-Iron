#pragma once

#include "ground/grass_gpu.h"
#include "ground/plant_gpu.h"
#include "ground/stone_gpu.h"
#include "ground/terrain_gpu.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Render::GL {
class Mesh;
class Texture;
class Buffer;
} // namespace Render::GL

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

struct TerrainChunkCmd {
  Mesh *mesh = nullptr;
  QMatrix4x4 model;
  TerrainChunkParams params;
  std::uint16_t sort_key = 0x8000u;
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

class DrawQueueSoA {
public:
  void clear() {
    m_grid_cmds.clear();
    m_selection_ring_cmds.clear();
    m_selection_smoke_cmds.clear();
    m_cylinder_cmds.clear();
    m_mesh_cmds.clear();
    m_fog_batch_cmds.clear();
    m_grass_batch_cmds.clear();
    m_stone_batch_cmds.clear();
    m_plant_batch_cmds.clear();
    m_terrain_chunk_cmds.clear();
  }

  void submit(const GridCmd &cmd) { m_grid_cmds.push_back(cmd); }
  void submit(const SelectionRingCmd &cmd) {
    m_selection_ring_cmds.push_back(cmd);
  }
  void submit(const SelectionSmokeCmd &cmd) {
    m_selection_smoke_cmds.push_back(cmd);
  }
  void submit(const CylinderCmd &cmd) { m_cylinder_cmds.push_back(cmd); }
  void submit(const MeshCmd &cmd) { m_mesh_cmds.push_back(cmd); }
  void submit(const FogBatchCmd &cmd) { m_fog_batch_cmds.push_back(cmd); }
  void submit(const GrassBatchCmd &cmd) { m_grass_batch_cmds.push_back(cmd); }
  void submit(const StoneBatchCmd &cmd) { m_stone_batch_cmds.push_back(cmd); }
  void submit(const PlantBatchCmd &cmd) { m_plant_batch_cmds.push_back(cmd); }
  void submit(const TerrainChunkCmd &cmd) {
    m_terrain_chunk_cmds.push_back(cmd);
  }

  bool empty() const {
    return m_grid_cmds.empty() && m_selection_ring_cmds.empty() &&
           m_selection_smoke_cmds.empty() && m_cylinder_cmds.empty() &&
           m_mesh_cmds.empty() && m_fog_batch_cmds.empty() &&
           m_grass_batch_cmds.empty() && m_stone_batch_cmds.empty() &&
           m_plant_batch_cmds.empty() && m_terrain_chunk_cmds.empty();
  }

  void sort_for_batching() {

    std::sort(m_mesh_cmds.begin(), m_mesh_cmds.end(),
              [](const MeshCmd &a, const MeshCmd &b) {
                return reinterpret_cast<uintptr_t>(a.texture) <
                       reinterpret_cast<uintptr_t>(b.texture);
              });

    std::sort(m_terrain_chunk_cmds.begin(), m_terrain_chunk_cmds.end(),
              [](const TerrainChunkCmd &a, const TerrainChunkCmd &b) {
                return a.sort_key < b.sort_key;
              });
  }

  const std::vector<GridCmd> &grid_cmds() const { return m_grid_cmds; }
  const std::vector<SelectionRingCmd> &selection_ring_cmds() const {
    return m_selection_ring_cmds;
  }
  const std::vector<SelectionSmokeCmd> &selection_smoke_cmds() const {
    return m_selection_smoke_cmds;
  }
  const std::vector<CylinderCmd> &cylinder_cmds() const {
    return m_cylinder_cmds;
  }
  const std::vector<MeshCmd> &mesh_cmds() const { return m_mesh_cmds; }
  const std::vector<FogBatchCmd> &fog_batch_cmds() const {
    return m_fog_batch_cmds;
  }
  const std::vector<GrassBatchCmd> &grass_batch_cmds() const {
    return m_grass_batch_cmds;
  }
  const std::vector<StoneBatchCmd> &stone_batch_cmds() const {
    return m_stone_batch_cmds;
  }
  const std::vector<PlantBatchCmd> &plant_batch_cmds() const {
    return m_plant_batch_cmds;
  }
  const std::vector<TerrainChunkCmd> &terrain_chunk_cmds() const {
    return m_terrain_chunk_cmds;
  }

private:
  std::vector<GridCmd> m_grid_cmds;
  std::vector<SelectionRingCmd> m_selection_ring_cmds;
  std::vector<SelectionSmokeCmd> m_selection_smoke_cmds;
  std::vector<CylinderCmd> m_cylinder_cmds;
  std::vector<MeshCmd> m_mesh_cmds;
  std::vector<FogBatchCmd> m_fog_batch_cmds;
  std::vector<GrassBatchCmd> m_grass_batch_cmds;
  std::vector<StoneBatchCmd> m_stone_batch_cmds;
  std::vector<PlantBatchCmd> m_plant_batch_cmds;
  std::vector<TerrainChunkCmd> m_terrain_chunk_cmds;
};

} // namespace Render::GL
