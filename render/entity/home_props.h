#pragma once

#include "render/equipment/equipment_registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

struct HomeProps {
  EquipmentHandle soup_bowl{k_invalid_equipment_handle};
};

[[nodiscard]] auto home_props() -> const HomeProps&;

void register_home_prop_archetypes();

[[nodiscard]] auto soup_puddle_archetype() -> const RenderArchetype&;

[[nodiscard]] auto cloth_strip_archetype(bool indigo) -> const RenderArchetype&;
[[nodiscard]] auto cloth_strip_tail_archetype(bool indigo) -> const RenderArchetype&;
inline constexpr int k_cloth_strips = 5;
inline constexpr float k_cloth_link_length = 0.17F;

[[nodiscard]] auto lamp_glow_archetype() -> const RenderArchetype&;

[[nodiscard]] auto shutter_leaf_archetype(bool carthage) -> const RenderArchetype&;

inline constexpr int k_laundry_colours = 4;
[[nodiscard]] auto laundry_piece_archetype(int colour) -> const RenderArchetype&;
[[nodiscard]] auto washing_line_archetype() -> const RenderArchetype&;
[[nodiscard]] auto washing_pole_archetype() -> const RenderArchetype&;

inline constexpr int k_hen_breeds = 3;
[[nodiscard]] auto hen_body_archetype(int breed) -> const RenderArchetype&;
[[nodiscard]] auto hen_head_archetype(int breed) -> const RenderArchetype&;

} // namespace Render::GL
