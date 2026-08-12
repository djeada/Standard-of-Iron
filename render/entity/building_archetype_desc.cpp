#include "building_archetype_desc.h"

#include <limits>
#include <type_traits>
#include <utility>

#include "building_decay.h"
#include "render/gl/primitives.h"

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

void add_part_to_builder(RenderArchetypeBuilder& builder,
                         const BuildingPartDesc& part,
                         const QVector3D& color) {
  switch (part.kind) {
  case BuildingPartKind::Box:
    builder.add_box(part.point_a,
                    part.point_b,
                    color,
                    part.texture,
                    part.alpha,
                    part.material_id,
                    part.material);
    break;
  case BuildingPartKind::PaletteBox:
    builder.add_palette_box(part.point_a,
                            part.point_b,
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
      builder.add_palette_mesh(get_unit_cube(),
                               model,
                               part.palette_slot,
                               part.texture,
                               part.alpha,
                               part.material_id,
                               part.material);
    } else {
      builder.add_mesh(get_unit_cube(),
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

void BuildingArchetypeDesc::add_box(const QVector3D& center,
                                    const QVector3D& scale,
                                    const QVector3D& color,
                                    BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Box;
  part.point_a = center;
  part.point_b = scale;
  part.color = color;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_palette_box(const QVector3D& center,
                                            const QVector3D& scale,
                                            std::uint8_t palette_slot,
                                            BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::PaletteBox;
  part.point_a = center;
  part.point_b = scale;
  part.palette_slot = palette_slot;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_rotated_box(const QVector3D& center,
                                            const QVector3D& scale,
                                            const QVector3D& euler_deg,
                                            const QVector3D& color,
                                            BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::RotatedBox;
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
                                         BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Cylinder;
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
                                     BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Cone;
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
                                                 BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::PaletteCylinder;
  part.point_a = start;
  part.point_b = end;
  part.radius = radius;
  part.palette_slot = palette_slot;
  part.states = states;
  m_parts.push_back(std::move(part));
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
      add_part_to_builder(builder, part, decayed_color(part.color, state, seed));
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
