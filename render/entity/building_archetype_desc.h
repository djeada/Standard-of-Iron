#pragma once

#include "../render_archetype.h"
#include "../template_cache.h"
#include "building_state.h"

#include <QVector3D>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Render::GL {

enum class BuildingPartKind : std::uint8_t {
  Box,
  Cylinder,
  PaletteBox,
  PaletteCylinder
};

enum class BuildingStateMask : std::uint8_t {
  None = 0U,
  Normal = 1U << 0U,
  Damaged = 1U << 1U,
  Destroyed = 1U << 2U,
  All = (1U << 0U) | (1U << 1U) | (1U << 2U)
};

auto operator|(BuildingStateMask lhs,
               BuildingStateMask rhs) -> BuildingStateMask;
auto operator&(BuildingStateMask lhs,
               BuildingStateMask rhs) -> BuildingStateMask;

inline constexpr auto kBuildingStateMaskIntact =
    static_cast<BuildingStateMask>((1U << 0U) | (1U << 1U));

enum class BuildingLODMask : std::uint8_t {
  None = 0U,
  Full = 1U << 0U,
  Minimal = 1U << 1U,
  All = (1U << 0U) | (1U << 1U)
};

auto operator|(BuildingLODMask lhs, BuildingLODMask rhs) -> BuildingLODMask;
auto operator&(BuildingLODMask lhs, BuildingLODMask rhs) -> BuildingLODMask;

struct BuildingPartDesc {
  BuildingPartKind kind{BuildingPartKind::Box};
  QVector3D point_a{0.0F, 0.0F, 0.0F};
  QVector3D point_b{1.0F, 1.0F, 1.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};
  float radius{0.0F};
  std::uint8_t palette_slot{kRenderArchetypeFixedColorSlot};
  Texture *texture{nullptr};
  float alpha{1.0F};
  int material_id{0};
  Material *material{nullptr};
  BuildingStateMask states{BuildingStateMask::All};
  BuildingLODMask lod{BuildingLODMask::All};
};

class BuildingArchetypeDesc {
public:
  explicit BuildingArchetypeDesc(std::string name);

  void add_box(const QVector3D &center, const QVector3D &scale,
               const QVector3D &color,
               BuildingStateMask states = BuildingStateMask::All,
               BuildingLODMask lod = BuildingLODMask::All);
  void add_palette_box(const QVector3D &center, const QVector3D &scale,
                       std::uint8_t palette_slot,
                       BuildingStateMask states = BuildingStateMask::All,
                       BuildingLODMask lod = BuildingLODMask::All);
  void add_cylinder(const QVector3D &start, const QVector3D &end, float radius,
                    const QVector3D &color,
                    BuildingStateMask states = BuildingStateMask::All,
                    BuildingLODMask lod = BuildingLODMask::All);
  void add_palette_cylinder(const QVector3D &start, const QVector3D &end,
                            float radius, std::uint8_t palette_slot,
                            BuildingStateMask states = BuildingStateMask::All,
                            BuildingLODMask lod = BuildingLODMask::All);

  void set_full_lod_max_distance(float max_distance);

  [[nodiscard]] auto name() const -> const std::string & { return m_name; }
  [[nodiscard]] auto parts() const -> const std::vector<BuildingPartDesc> & {
    return m_parts;
  }
  [[nodiscard]] auto full_lod_max_distance() const -> float {
    return m_full_lod_max_distance;
  }

private:
  std::string m_name;
  std::vector<BuildingPartDesc> m_parts;
  float m_full_lod_max_distance{60.0F};
};

auto build_building_archetype(const BuildingArchetypeDesc &desc,
                              BuildingState state) -> RenderArchetype;
auto build_building_archetype_from_recorded(
    std::string name,
    const std::vector<RecordedMeshCmd> &commands) -> RenderArchetype;

struct BuildingArchetypeSet {
  std::array<RenderArchetype, 3> states;

  [[nodiscard]] auto
  for_state(BuildingState state) const -> const RenderArchetype &;
};

template <typename Builder>
auto build_stateful_building_archetype_set(Builder &&builder)
    -> BuildingArchetypeSet {
  return {{
      builder(BuildingState::Normal),
      builder(BuildingState::Damaged),
      builder(BuildingState::Destroyed),
  }};
}

} // namespace Render::GL
