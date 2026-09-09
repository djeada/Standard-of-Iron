#pragma once

#include <QVector3D>

#include <array>
#include <cstdint>
#include <source_location>
#include <string>
#include <vector>

#include "building_state.h"
#include "render/render_archetype.h"
#include "render/template_cache.h"

namespace Render::GL {

enum class BuildingPartKind : std::uint8_t {
  Box,
  Cylinder,
  Cone,
  PaletteBox,
  PaletteCylinder,
  RotatedBox,
  PaletteRotatedBox
};

enum class BuildingStateMask : std::uint8_t {
  None = 0U,
  Normal = 1U << 0U,
  Damaged = 1U << 1U,
  Destroyed = 1U << 2U,
  All = (1U << 0U) | (1U << 1U) | (1U << 2U)
};

auto operator|(BuildingStateMask lhs, BuildingStateMask rhs) -> BuildingStateMask;
auto operator&(BuildingStateMask lhs, BuildingStateMask rhs) -> BuildingStateMask;

inline constexpr auto k_building_state_mask_intact =
    static_cast<BuildingStateMask>((1U << 0U) | (1U << 1U));

struct BuildingPartDesc {
  BuildingPartKind kind{BuildingPartKind::Box};

  std::string name;
  std::source_location origin{};
  QVector3D point_a{0.0F, 0.0F, 0.0F};
  QVector3D point_b{1.0F, 1.0F, 1.0F};
  QVector3D color{1.0F, 1.0F, 1.0F};
  QVector3D euler_deg{0.0F, 0.0F, 0.0F};
  float radius{0.0F};
  std::uint8_t palette_slot{k_render_archetype_fixed_color_slot};
  Texture* texture{nullptr};
  float alpha{1.0F};
  int material_id{0};
  Material* material{nullptr};
  BuildingStateMask states{BuildingStateMask::All};
};

class BuildingArchetypeDesc {
public:
  explicit BuildingArchetypeDesc(std::string name);

  void set_label(std::string label);
  [[nodiscard]] auto label() const -> const std::string& { return m_label; }

  void add_box(const QVector3D& center,
               const QVector3D& scale,
               const QVector3D& color,
               BuildingStateMask states = BuildingStateMask::All,
               std::source_location origin = std::source_location::current());
  void add_palette_box(const QVector3D& center,
                       const QVector3D& scale,
                       std::uint8_t palette_slot,
                       BuildingStateMask states = BuildingStateMask::All,
                       std::source_location origin = std::source_location::current());

  void add_palette_rotated_box(
      const QVector3D& center,
      const QVector3D& scale,
      const QVector3D& euler_deg,
      std::uint8_t palette_slot,
      BuildingStateMask states = BuildingStateMask::All,
      std::source_location origin = std::source_location::current());

  void add_rotated_box(const QVector3D& center,
                       const QVector3D& scale,
                       const QVector3D& euler_deg,
                       const QVector3D& color,
                       BuildingStateMask states = BuildingStateMask::All,
                       std::source_location origin = std::source_location::current());
  void add_cylinder(const QVector3D& start,
                    const QVector3D& end,
                    float radius,
                    const QVector3D& color,
                    BuildingStateMask states = BuildingStateMask::All,
                    std::source_location origin = std::source_location::current());
  void add_cone(const QVector3D& base,
                const QVector3D& tip,
                float radius,
                const QVector3D& color,
                BuildingStateMask states = BuildingStateMask::All,
                std::source_location origin = std::source_location::current());
  void
  add_palette_cylinder(const QVector3D& start,
                       const QVector3D& end,
                       float radius,
                       std::uint8_t palette_slot,
                       BuildingStateMask states = BuildingStateMask::All,
                       std::source_location origin = std::source_location::current());

  void scale_uniformly(float factor);

  [[nodiscard]] auto name() const -> const std::string& { return m_name; }
  [[nodiscard]] auto parts() const -> const std::vector<BuildingPartDesc>& {
    return m_parts;
  }

private:
  std::string m_name;
  std::string m_label;
  std::vector<BuildingPartDesc> m_parts;
};

class BuildingPartLabel {
public:
  BuildingPartLabel(BuildingArchetypeDesc& desc, std::string label)
      : m_desc(&desc)
      , m_previous(desc.label()) {
    desc.set_label(std::move(label));
  }
  BuildingPartLabel(const BuildingPartLabel&) = delete;
  auto operator=(const BuildingPartLabel&) -> BuildingPartLabel& = delete;
  BuildingPartLabel(BuildingPartLabel&&) = delete;
  auto operator=(BuildingPartLabel&&) -> BuildingPartLabel& = delete;
  ~BuildingPartLabel() { m_desc->set_label(std::move(m_previous)); }

private:
  BuildingArchetypeDesc* m_desc;
  std::string m_previous;
};

auto build_building_archetype(const BuildingArchetypeDesc& desc,
                              BuildingState state) -> RenderArchetype;
auto build_building_archetype_from_recorded(
    std::string name,
    const std::vector<RecordedMeshCmd>& commands,
    BuildingState state = BuildingState::Normal) -> RenderArchetype;
auto build_building_archetype_from_recorded_lods(
    std::string name,
    const std::vector<RecordedMeshCmd>& full_commands,
    const std::vector<RecordedMeshCmd>& minimal_commands,
    BuildingState state = BuildingState::Normal) -> RenderArchetype;

struct BuildingArchetypeSet {
  std::array<RenderArchetype, 3> states;

  [[nodiscard]] auto for_state(BuildingState state) const -> const RenderArchetype&;
};

template <typename Builder>
auto build_stateful_building_archetype_set(Builder&& builder) -> BuildingArchetypeSet {
  return {{
      builder(BuildingState::Normal),
      builder(BuildingState::Damaged),
      builder(BuildingState::Destroyed),
  }};
}

} // namespace Render::GL
