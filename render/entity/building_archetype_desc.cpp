#include "building_archetype_desc.h"

#include <limits>
#include <type_traits>
#include <utility>

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

} // namespace

auto operator|(BuildingStateMask lhs,
               BuildingStateMask rhs) -> BuildingStateMask {
  return static_cast<BuildingStateMask>(static_cast<BuildingStateMaskInt>(lhs) |
                                        static_cast<BuildingStateMaskInt>(rhs));
}

auto operator&(BuildingStateMask lhs,
               BuildingStateMask rhs) -> BuildingStateMask {
  return static_cast<BuildingStateMask>(static_cast<BuildingStateMaskInt>(lhs) &
                                        static_cast<BuildingStateMaskInt>(rhs));
}

BuildingArchetypeDesc::BuildingArchetypeDesc(std::string name)
    : m_name(std::move(name)) {}

void BuildingArchetypeDesc::add_box(const QVector3D &center,
                                    const QVector3D &scale,
                                    const QVector3D &color,
                                    BuildingStateMask states) {
  BuildingPartDesc part;
  part.kind = BuildingPartKind::Box;
  part.point_a = center;
  part.point_b = scale;
  part.color = color;
  part.states = states;
  m_parts.push_back(std::move(part));
}

void BuildingArchetypeDesc::add_palette_box(const QVector3D &center,
                                            const QVector3D &scale,
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

void BuildingArchetypeDesc::add_cylinder(const QVector3D &start,
                                         const QVector3D &end, float radius,
                                         const QVector3D &color,
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

void BuildingArchetypeDesc::add_palette_cylinder(const QVector3D &start,
                                                 const QVector3D &end,
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

auto build_building_archetype(const BuildingArchetypeDesc &desc,
                              BuildingState state) -> RenderArchetype {
  RenderArchetypeBuilder builder(desc.name());
  builder.set_max_distance(std::numeric_limits<float>::infinity());

  for (const auto &part : desc.parts()) {
    if (!supports_state(part.states, state)) {
      continue;
    }

    switch (part.kind) {
    case BuildingPartKind::Box:
      builder.add_box(part.point_a, part.point_b, part.color, part.texture,
                      part.alpha, part.material_id, part.material);
      break;
    case BuildingPartKind::PaletteBox:
      builder.add_palette_box(part.point_a, part.point_b, part.palette_slot,
                              part.texture, part.alpha, part.material_id,
                              part.material);
      break;
    case BuildingPartKind::Cylinder:
      builder.add_cylinder(part.point_a, part.point_b, part.radius, part.color,
                           part.texture, part.alpha, part.material_id,
                           part.material);
      break;
    case BuildingPartKind::PaletteCylinder:
      builder.add_palette_cylinder(part.point_a, part.point_b, part.radius,
                                   part.palette_slot, part.texture, part.alpha,
                                   part.material_id, part.material);
      break;
    }
  }

  return std::move(builder).build();
}

auto build_building_archetype_from_recorded(
    std::string name,
    const std::vector<RecordedMeshCmd> &commands) -> RenderArchetype {
  RenderArchetypeBuilder builder(std::move(name));
  builder.set_max_distance(std::numeric_limits<float>::infinity());

  for (const auto &cmd : commands) {
    builder.add_mesh(cmd.mesh, cmd.local_model, cmd.color, cmd.texture,
                     cmd.alpha, cmd.material_id,
                     const_cast<Material *>(cmd.material));
  }

  return std::move(builder).build();
}

auto BuildingArchetypeSet::for_state(BuildingState state) const
    -> const RenderArchetype & {
  return states[state_index(state)];
}

} // namespace Render::GL
