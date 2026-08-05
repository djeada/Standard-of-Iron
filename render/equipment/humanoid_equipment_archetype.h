#pragma once

#include <span>
#include <string_view>

#include "../creature/archetype_registry.h"
#include "equipment_archetype_resolver.h"
#include "equipment_registry.h"

namespace Render::GL {

using HumanoidEquipmentContribution = EquipmentArchetype::Contribution;

void register_humanoid_equipment_contribution(
    EquipmentHandle handle, HumanoidEquipmentContribution contribution);

[[nodiscard]] auto resolve_humanoid_equipment_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId;

} // namespace Render::GL
