#include "static_building_batch.h"

#include <algorithm>
#include <atomic>
#include <limits>

#include "draw_commands.h"
#include "entity/unseen_submitter.h"
#include "gl/mesh.h"
#include "material.h"
#include "material_classification.h"

namespace Render::GL {
namespace {

auto next_merged_mesh_id() -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{0};
  return ++counter;
}

auto draws_with_basic_shader(const Material* material) -> bool {
  return material == nullptr || material->shader == nullptr ||
         material->shader == MaterialRegistry::instance().basic()->shader;
}

auto merges(const RenderArchetypeDraw& draw) -> bool {
  return draw.alpha >= k_opaque_threshold && draw.material_id >= 0 &&
         draws_with_basic_shader(draw.material);
}

auto max_axis_scale(const QMatrix4x4& world) -> float {
  return std::max({world.column(0).toVector3D().length(),
                   world.column(1).toVector3D().length(),
                   world.column(2).toVector3D().length()});
}

auto transform_normal(const QMatrix4x4& model, const QVector3D& normal) -> QVector3D {
  QVector3D const c0 = model.column(0).toVector3D();
  QVector3D const c1 = model.column(1).toVector3D();
  QVector3D const c2 = model.column(2).toVector3D();
  QVector3D const x = QVector3D::crossProduct(c1, c2);
  float const handedness = QVector3D::dotProduct(c0, x) < 0.0F ? -1.0F : 1.0F;
  return (x * normal.x() + QVector3D::crossProduct(c2, c0) * normal.y() +
          QVector3D::crossProduct(c0, c1) * normal.z()) *
         handedness;
}

void append_part(MergedBuildingMesh& merged, const RenderArchetypeDraw& draw) {
  auto const base = static_cast<std::uint32_t>(merged.vertices.size());
  float const slot = draw.palette_slot < k_merged_building_palette_capacity
                         ? static_cast<float>(draw.palette_slot)
                         : -1.0F;
  std::array<float, 4> const material{
      slot,
      static_cast<float>(Render::resolve_material_id(draw.material_id, draw.color)),
      static_cast<float>(Render::resolve_material_id(draw.material_id,
                                                     unseen_surface_color(draw.color))),
      static_cast<float>(draw.material_id)};
  for (const Vertex& vertex : draw.mesh->get_vertices()) {
    QVector3D const position = draw.local_model.map(
        QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]));
    QVector3D const normal = transform_normal(
        draw.local_model,
        QVector3D(vertex.normal[0], vertex.normal[1], vertex.normal[2]));
    merged.vertices.push_back(MergedBuildingVertex{
        .position = {position.x(), position.y(), position.z()},
        .normal = {normal.x(), normal.y(), normal.z()},
        .tex_coord = vertex.tex_coord,
        .color_alpha = {draw.color.x(), draw.color.y(), draw.color.z(), draw.alpha},
        .material = material,
    });
  }
  for (unsigned int const index : draw.mesh->get_indices()) {
    merged.indices.push_back(base + index);
  }
}

} // namespace

auto build_merged_building_mesh(const RenderArchetypeSlice& slice)
    -> MergedBuildingMesh {
  MergedBuildingMesh merged;
  merged.id = next_merged_mesh_id();
  std::vector<const RenderArchetypeDraw*> parts;
  for (const RenderArchetypeDraw& draw : slice.draws) {
    if (draw.mesh == nullptr) {
      continue;
    }
    (merges(draw) ? parts : merged.dynamic_draws).push_back(&draw);
  }
  std::stable_sort(parts.begin(), parts.end(), [](const auto* lhs, const auto* rhs) {
    return std::less<const Texture*>{}(lhs->texture, rhs->texture);
  });
  for (const RenderArchetypeDraw* part : parts) {
    if (merged.ranges.empty() || merged.ranges.back().texture != part->texture) {
      merged.ranges.push_back(MergedBuildingRange{
          .texture = part->texture,
          .first_index = static_cast<std::uint32_t>(merged.indices.size())});
    }
    append_part(merged, *part);
    merged.ranges.back().index_count =
        static_cast<std::uint32_t>(merged.indices.size()) -
        merged.ranges.back().first_index;
  }

  if (merged.vertices.empty()) {
    return merged;
  }
  float const inf = std::numeric_limits<float>::infinity();
  QVector3D lo(inf, inf, inf);
  QVector3D hi(-inf, -inf, -inf);
  for (const MergedBuildingVertex& vertex : merged.vertices) {
    for (int axis = 0; axis < 3; ++axis) {
      lo[axis] = std::min(lo[axis], vertex.position[axis]);
      hi[axis] = std::max(hi[axis], vertex.position[axis]);
    }
  }
  merged.bounds_center = (lo + hi) * 0.5F;
  for (const MergedBuildingVertex& vertex : merged.vertices) {
    merged.bounds_radius = std::max(
        merged.bounds_radius,
        (QVector3D(vertex.position[0], vertex.position[1], vertex.position[2]) -
         merged.bounds_center)
            .length());
  }
  return merged;
}

auto pack_building_instance(const RenderInstance& instance) -> BuildingInstanceGpu {
  const float* data = instance.world.constData();
  BuildingInstanceGpu record{
      .model_col0 = {data[0], data[1], data[2], data[12]},
      .model_col1 = {data[4], data[5], data[6], data[13]},
      .model_col2 = {data[8], data[9], data[10], data[14]},
      .state = {static_cast<float>(instance.damage_material_id),
                instance.unseen ? 1.0F : 0.0F,
                0.0F,
                0.0F},
  };
  std::array<float*, k_merged_building_palette_capacity> const entries{record.palette0,
                                                                       record.palette1};
  for (std::size_t slot = 0; slot < instance.palette.size() && slot < entries.size();
       ++slot) {
    QVector3D const color = instance.unseen
                                ? unseen_surface_color(instance.palette[slot])
                                : instance.palette[slot];
    entries[slot][0] = color.x();
    entries[slot][1] = color.y();
    entries[slot][2] = color.z();
    entries[slot][3] = static_cast<float>(Render::classify_material_id(color));
  }
  return record;
}

void StaticBuildingBatch::begin_frame() {
  m_placed.clear();
}

auto StaticBuildingBatch::place(const RenderInstance& instance)
    -> const std::vector<const RenderArchetypeDraw*>* {
  if (instance.static_id == 0U || instance.archetype == nullptr ||
      instance.lod != RenderArchetypeLod::Full ||
      instance.palette.size() > k_merged_building_palette_capacity) {
    return nullptr;
  }
  const RenderArchetype& archetype = *instance.archetype;
  if (archetype.merged_full == nullptr) {
    archetype.merged_full =
        std::make_shared<const MergedBuildingMesh>(build_merged_building_mesh(
            archetype.lods[static_cast<std::size_t>(RenderArchetypeLod::Full)]));
  }
  const MergedBuildingMesh& mesh = *archetype.merged_full;
  if (!mesh.indices.empty()) {
    m_placed.push_back(Placed{
        .mesh = &mesh,
        .default_texture = instance.default_texture,
        .record = pack_building_instance(instance),
        .bounds = {instance.world.map(mesh.bounds_center),
                   mesh.bounds_radius * max_axis_scale(instance.world)},
    });
  }
  return &mesh.dynamic_draws;
}

void StaticBuildingBatch::finish_frame() {
  std::sort(m_placed.begin(), m_placed.end(), [](const Placed& lhs, const Placed& rhs) {
    if (lhs.mesh->id != rhs.mesh->id) {
      return lhs.mesh->id < rhs.mesh->id;
    }
    return std::less<const Texture*>{}(lhs.default_texture, rhs.default_texture);
  });
  m_instances.clear();
  m_bounds.clear();
  m_draws.clear();
  for (const Placed& placed : m_placed) {
    if (m_draws.empty() || m_draws.back().mesh != placed.mesh ||
        m_draws.back().default_texture != placed.default_texture) {
      m_draws.push_back(
          StaticBatchDraw{.mesh = placed.mesh,
                          .default_texture = placed.default_texture,
                          .first = static_cast<std::uint32_t>(m_instances.size())});
    }
    ++m_draws.back().count;
    m_instances.push_back(placed.record);
    m_bounds.push_back(placed.bounds);
  }
}

} // namespace Render::GL
