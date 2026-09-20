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

// The spilled broth, drawn flat on the ground for the tail of the sequence.
[[nodiscard]] auto soup_puddle_archetype() -> const RenderArchetype&;

} // namespace Render::GL
