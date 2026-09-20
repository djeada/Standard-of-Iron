#pragma once

#include "render/equipment/equipment_registry.h"
#include "render/render_archetype.h"

namespace Render::GL {

// Props for the residential ambient gag. Shared by both nations: a clay bowl
// is a clay bowl.
struct HomeProps {
  EquipmentHandle soup_bowl{k_invalid_equipment_handle};
};

[[nodiscard]] auto home_props() -> const HomeProps&;

// Registers every house prop with the archetype registry so warm_all() builds
// the geometry at renderer init rather than on the first frame that draws a
// house. Call once, from the house renderer registration.
void register_home_prop_archetypes();

// The spilled broth, drawn flat on the ground for the tail of the sequence.
[[nodiscard]] auto soup_puddle_archetype() -> const RenderArchetype&;

// One hanging cloth strip in two links: the upper hangs from the top edge,
// the tail hangs from the upper's hem and lags it, so the free edge swings
// further than the hung edge and the strip bends instead of tilting. Several
// are submitted side by side, each on its own phase, which reads as cloth in
// a breeze without a cloth simulation.
[[nodiscard]] auto cloth_strip_archetype(bool indigo) -> const RenderArchetype&;
[[nodiscard]] auto cloth_strip_tail_archetype(bool indigo) -> const RenderArchetype&;
inline constexpr int k_cloth_strips = 5;
inline constexpr float k_cloth_link_length = 0.17F;

// The warm patch a lamp throws on the ground outside a doorway at night.
[[nodiscard]] auto lamp_glow_archetype() -> const RenderArchetype&;

// One shutter leaf, hinged along the origin's vertical axis and spanning +z
// by width. Scaled per nation to the window it covers.
[[nodiscard]] auto shutter_leaf_archetype(bool carthage) -> const RenderArchetype&;

// Washing on a line: a hung piece of cloth per colour, a unit-length line
// along +x, and a pole for the far end of a rooftop line.
inline constexpr int k_laundry_colours = 4;
[[nodiscard]] auto laundry_piece_archetype(int colour) -> const RenderArchetype&;
[[nodiscard]] auto washing_line_archetype() -> const RenderArchetype&;
[[nodiscard]] auto washing_pole_archetype() -> const RenderArchetype&;

} // namespace Render::GL
