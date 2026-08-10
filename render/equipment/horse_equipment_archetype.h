#pragma once

#include <span>
#include <string_view>

#include "equipment_archetype_resolver.h"
#include "equipment_registry.h"
#include "render/creature/archetype_registry.h"

namespace Render::GL {

using HorseEquipmentContribution = EquipmentArchetype::Contribution;

void register_horse_equipment_contribution(EquipmentHandle handle,
                                           HorseEquipmentContribution contribution);

[[nodiscard]] auto resolve_horse_equipment_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId;

} // namespace Render::GL
