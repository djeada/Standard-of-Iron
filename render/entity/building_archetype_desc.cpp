#include "building_archetype_desc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <utility>

#include "building_decay.h"
#include "render/gl/primitives.h"
#include "render/gl/shared_geometry_cache.h"
#include "render/material_classification.h"

namespace Render::GL {
namespace {

using BuildingStateMaskInt = std::underlying_type_t<BuildingStateMask>;

auto state_mask_for(BuildingState state) -> BuildingStateMask {
  switch (state) {
  case BuildingState::Normal:
    return BuildingStateMask::Normal;
  case BuildingState::Damaged:
    return BuildingStateMask::Damaged;
  case BuildingState::Destroyed:
    return BuildingStateMask::Destroyed;
  }
  return BuildingStateMask::Normal;
}

auto state_index(BuildingState state) -> std::size_t {
  switch (state) {
  case BuildingState::Normal:
    return 0U;
  case BuildingState::Damaged:
    return 1U;
  case BuildingState::Destroyed:
    return 2U;
  }
  return 0U;
}

auto supports_state(BuildingStateMask mask, BuildingState state) -> bool {
  return (mask & state_mask_for(state)) != BuildingStateMask::None;
}

auto building_box_mesh(const QVector3D& half, int material) -> Mesh* {
  float const shortest = std::min({half.x(), half.y(), half.z()});
  if (shortest < 0.025F ||
      (material != k_building_material_stone && material != k_building_material_wood &&
       material != k_building_material_ceramic)) {
    return get_unit_cube();
  }

  float const bevel = std::min(0.012F, shortest * 0.10F);
  constexpr std::array<float, 7> levels{
      0.0F, 0.00390625F, 0.0078125F, 0.015625F, 0.03125F, 0.0625F, 0.125F};
  QVector3D inset;
  std::uint64_t variant = 0;
  for (int axis = 0; axis < 3; ++axis) {
    float const wanted = bevel / half[axis];
    std::size_t closest = 0;
    for (std::size_t level = 1; level < levels.size(); ++level) {
      if (std::abs(levels[level] - wanted) < std::abs(levels[closest] - wanted)) {
        closest = level;
      }
    }
    inset[axis] = levels[closest];
    variant = variant * levels.size() + closest;
  }

  if (inset.x() == 0.0F || inset.y() == 0.0F || inset.z() == 0.0F) {
    return get_unit_cube();
  }
  return SharedGeometryCache::instance().get_or_build(
      geometry_key("building/beveled_box", variant), [inset] {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        auto face = [&](std::initializer_list<QVector3D> points) {
          std::vector<QVector3D> polygon(points);
          QVector3D normal =
              QVector3D::crossProduct(polygon[1] - polygon[0], polygon[2] - polygon[0])
                  .normalized();
          QVector3D center;
          for (auto const& point : polygon) {
            center += point;
          }
          if (QVector3D::dotProduct(normal, center) < 0.0F) {
            std::reverse(polygon.begin(), polygon.end());
            normal = -normal;
          }
          auto const base = static_cast<unsigned int>(vertices.size());
          for (auto const& point : polygon) {
            vertices.push_back(
                {{point.x(), point.y(), point.z()},
                 {normal.x(), normal.y(), normal.z()},
                 {(point.x() + 1.0F) * 0.5F, (point.z() + 1.0F) * 0.5F}});
          }
          for (unsigned int i = 1; i + 1 < polygon.size(); ++i) {
            indices.insert(indices.end(), {base, base + i, base + i + 1});
          }
        };
        QVector3D const inner = QVector3D(1.0F, 1.0F, 1.0F) - inset;
        for (int axis = 0; axis < 3; ++axis) {
          int const u = (axis + 1) % 3;
          int const v = (axis + 2) % 3;
          for (float sign : {-1.0F, 1.0F}) {
            auto point = [&](float a, float b) {
              QVector3D p;
              p[axis] = sign;
              p[u] = a * inner[u];
              p[v] = b * inner[v];
              return p;
            };
            face({point(-1, -1), point(1, -1), point(1, 1), point(-1, 1)});
          }

          for (float su : {-1.0F, 1.0F}) {
            for (float sv : {-1.0F, 1.0F}) {
              QVector3D a, b;
              a[axis] = b[axis] = -inner[axis];
              a[u] = su;
              a[v] = sv * inner[v];
              b[u] = su * inner[u];
              b[v] = sv;
              QVector3D c = b, d = a;
              c[axis] = d[axis] = inner[axis];
              face({a, b, c, d});
            }
          }
        }
        for (float x : {-1.0F, 1.0F}) {
          for (float y : {-1.0F, 1.0F}) {
            for (float z : {-1.0F, 1.0F}) {
              face({{x, y * inner.y(), z * inner.z()},
                    {x * inner.x(), y, z * inner.z()},
                    {x * inner.x(), y * inner.y(), z}});
            }
          }
        }
        return std::make_unique<Mesh>(vertices, indices);
      });
}

auto building_material(const BuildingPartDesc& part) -> int {
  if (part.material_id != 0) {
    return part.material_id;
  }
  if (part.palette_slot != k_render_archetype_fixed_color_slot) {

    return 0;
  }

  return Render::classify_material_id(part.color) == Render::k_material_wood
             ? k_building_material_wood
             : k_building_material_stone;
}

void add_part_to_builder(RenderArchetypeBuilder& builder,
                         const BuildingPartDesc& part,
                         const QVector3D& color) {
  switch (part.kind) {
  case BuildingPartKind::Box:
    builder.add_mesh(building_box_mesh(part.point_b, part.material_id),
                     box_local_model(part.point_a, part.point_b),
                     color,
                     part.texture,
                     part.alpha,
                     part.material_id,
                     part.material);
    break;
  case BuildingPartKind::PaletteBox:
    builder.add_palette_mesh(building_box_mesh(part.point_b, part.material_id),
                             box_local_model(part.point_a, part.point_b),
                             part.palette_slot,
                             part.texture,
                             part.alpha,
                             part.material_id,
                             part.material);
    break;
  case BuildingPartKind::RotatedBox:
  case BuildingPartKind::PaletteRotatedBox: {
    QMatrix4x4 model;
    model.translate(part.point_a);
    model.rotate(part.euler_deg.z(), 0.0F, 0.0F, 1.0F);
    model.rotate(part.euler_deg.y(), 0.0F, 1.0F, 0.0F);
    model.rotate(part.euler_deg.x(), 1.0F, 0.0F, 0.0F);
    model.scale(part.point_b);
    if (part.kind == BuildingPartKind::PaletteRotatedBox) {
      builder.add_palette_mesh(building_box_mesh(part.point_b, part.material_id),
                               model,
                               part.palette_slot,
                               part.texture,
                               part.alpha,
                               part.material_id,
                               part.material);
    } else {
      builder.add_mesh(building_box_mesh(part.point_b, part.material_id),
                       model,
                       color,
                       part.texture,
                       part.alpha,
                       part.material_id,
                       part.material);
    }
    break;
  }
  case BuildingPartKind::Cylinder:
    builder.add_cylinder(part.point_a,
                         part.point_b,
                         part.radius,
                         color,
                         part.texture,
                         part.alpha,
                         part.material_id,
                         part.material);
    break;
  case BuildingPartKind::Cone:
    builder.add_cone(part.point_a,
                     part.point_b,
                     part.radius,
                     color,
                     part.texture,
                     part.alpha,
                     part.material_id,
                     part.material);
    break;
  case BuildingPartKind::PaletteCylinder:
    builder.add_palette_cylinder(part.point_a,
                                 part.point_b,
                                 part.radius,
                                 part.palette_slot,
                                 part.texture,
                                 part.alpha,
                                 part.material_id,
                                 part.material);
    break;
  }
}

} // namespace

auto operator|(BuildingStateMask lhs, BuildingStateMask rhs) -> BuildingStateMask {
  return static_cast<BuildingStateMask>(static_cast<BuildingStateMaskInt>(lhs) |
                                        static_cast<BuildingStateMaskInt>(rhs));
}

auto operator&(BuildingStateMask lhs, BuildingStateMask rhs) -> BuildingStateMask {
  return static_cast<BuildingStateMask>(static_cast<BuildingStateMaskInt>(lhs) &
                                        static_cast<BuildingStateMaskInt>(rhs));
}

BuildingArchetypeDesc::BuildingArchetypeDesc(std::string name)
    : m_name(std::move(name)) {
}

void BuildingArchetypeDesc::set_label(std::string label) {
  m_label = std::move(label);
}

void BuildingArchetypeDesc::add_box(const QVector3D& center,
                                    const QVector3D& scale,
                                    const QVector3D& color,
                                    BuildingStateMask states,
                                    std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Box;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = center;
  part.point_b = scale;
  part.color = color;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_palette_box(const QVector3D& center,
                                            const QVector3D& scale,
                                            std::uint8_t palette_slot,
                                            BuildingStateMask states,
                                            std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::PaletteBox;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = center;
  part.point_b = scale;
  part.palette_slot = palette_slot;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_palette_rotated_box(const QVector3D& center,
                                                    const QVector3D& scale,
                                                    const QVector3D& euler_deg,
                                                    std::uint8_t palette_slot,
                                                    BuildingStateMask states,
                                                    std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::PaletteRotatedBox;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = center;
  part.point_b = scale;
  part.euler_deg = euler_deg;
  part.palette_slot = palette_slot;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_rotated_box(const QVector3D& center,
                                            const QVector3D& scale,
                                            const QVector3D& euler_deg,
                                            const QVector3D& color,
                                            BuildingStateMask states,
                                            std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::RotatedBox;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = center;
  part.point_b = scale;
  part.euler_deg = euler_deg;
  part.color = color;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_cylinder(const QVector3D& start,
                                         const QVector3D& end,
                                         float radius,
                                         const QVector3D& color,
                                         BuildingStateMask states,
                                         std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Cylinder;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = start;
  part.point_b = end;
  part.color = color;
  part.radius = radius;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_cone(const QVector3D& base,
                                     const QVector3D& tip,
                                     float radius,
                                     const QVector3D& color,
                                     BuildingStateMask states,
                                     std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Cone;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = base;
  part.point_b = tip;
  part.color = color;
  part.radius = radius;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_palette_cylinder(const QVector3D& start,
                                                 const QVector3D& end,
                                                 float radius,
                                                 std::uint8_t palette_slot,
                                                 BuildingStateMask states,
                                                 std::source_location origin) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::PaletteCylinder;
  part.name = m_label;
  part.material_id = m_material;
  part.origin = origin;
  part.point_a = start;
  part.point_b = end;
  part.radius = radius;
  part.palette_slot = palette_slot;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::scale_uniformly(float factor) {
  for (auto& part : m_parts) {
    part.point_a *= factor;
    part.point_b *= factor;
    part.radius *= factor;
  }
}

auto build_building_archetype(const BuildingArchetypeDesc& desc,
                              BuildingState state) -> RenderArchetype {
  RenderArchetypeBuilder builder(desc.name());

  const auto emit_parts = [&]() {
    int seed = 0;
    for (const auto& part : desc.parts()) {
      ++seed;
      if (!supports_state(part.states, state)) {
        continue;
      }
      auto surfaced = part;
      surfaced.material_id = building_material(part);
      builder.set_timber(part.material_id % 10 == k_building_material_wood);
      add_part_to_builder(builder, surfaced, decayed_color(part.color, state, seed));
    }
  };

  builder.use_lod(RenderArchetypeLod::Full);
  builder.set_max_distance(std::numeric_limits<float>::infinity());
  emit_parts();

  return std::move(builder).build();
}

auto build_building_archetype_from_recorded(
    std::string name,
    const std::vector<RecordedMeshCmd>& commands,
    BuildingState state) -> RenderArchetype {
  RenderArchetypeBuilder builder(std::move(name));
  builder.set_max_distance(std::numeric_limits<float>::infinity());

  int seed = 0;
  for (const auto& cmd : commands) {
    ++seed;
    builder.add_mesh(cmd.mesh,
                     cmd.local_model,
                     decayed_color(cmd.color, state, seed),
                     cmd.texture,
                     cmd.alpha,
                     cmd.material_id,
                     const_cast<Material*>(cmd.material));
  }

  return std::move(builder).build();
}

auto BuildingArchetypeSet::for_state(BuildingState state) const
    -> const RenderArchetype& {
  return states[state_index(state)];
}

} // namespace Render::GL
