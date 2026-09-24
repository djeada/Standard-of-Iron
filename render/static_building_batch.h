#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "render_archetype.h"

namespace Render::GL {

struct MergedBuildingVertex {
  std::array<float, 3> position{};
  std::array<float, 3> normal{};
  std::array<float, 2> tex_coord{};
  std::array<float, 4> color_alpha{};
  std::array<float, 4> material{};
};

struct MergedBuildingRange {
  Texture* texture = nullptr;
  std::uint32_t first_index = 0;
  std::uint32_t index_count = 0;
};

struct MergedBuildingMesh {
  std::uint64_t id = 0;
  std::vector<MergedBuildingVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<MergedBuildingRange> ranges;
  std::vector<const RenderArchetypeDraw*> dynamic_draws;
  QVector3D bounds_center;
  float bounds_radius = 0.0F;
};

struct BuildingInstanceGpu {
  float model_col0[4]{1, 0, 0, 0};
  float model_col1[4]{0, 1, 0, 0};
  float model_col2[4]{0, 0, 1, 0};
  float palette0[4]{0, 0, 0, -1};
  float palette1[4]{0, 0, 0, -1};
  float state[4]{0, 0, 0, 0};
};

struct StaticBatchBounds {
  QVector3D center;
  float radius = 0.0F;
};

struct StaticBatchDraw {
  const MergedBuildingMesh* mesh = nullptr;
  Texture* default_texture = nullptr;
  std::uint32_t first = 0;
  std::uint32_t count = 0;
};

inline constexpr std::size_t k_merged_building_palette_capacity = 2;

[[nodiscard]] auto
build_merged_building_mesh(const RenderArchetypeSlice& slice) -> MergedBuildingMesh;
[[nodiscard]] auto
pack_building_instance(const RenderInstance& instance) -> BuildingInstanceGpu;

class StaticBuildingBatch {
public:
  void begin_frame();

  [[nodiscard]] auto place(const RenderInstance& instance)
      -> const std::vector<const RenderArchetypeDraw*>*;

  void finish_frame();

  [[nodiscard]] auto
  instances() const noexcept -> const std::vector<BuildingInstanceGpu>& {
    return m_instances;
  }
  [[nodiscard]] auto bounds() const noexcept -> const std::vector<StaticBatchBounds>& {
    return m_bounds;
  }
  [[nodiscard]] auto draws() const noexcept -> const std::vector<StaticBatchDraw>& {
    return m_draws;
  }

private:
  struct Placed {
    const MergedBuildingMesh* mesh = nullptr;
    Texture* default_texture = nullptr;
    BuildingInstanceGpu record;
    StaticBatchBounds bounds;
  };

  std::vector<Placed> m_placed;
  std::vector<BuildingInstanceGpu> m_instances;
  std::vector<StaticBatchBounds> m_bounds;
  std::vector<StaticBatchDraw> m_draws;
};

} // namespace Render::GL
