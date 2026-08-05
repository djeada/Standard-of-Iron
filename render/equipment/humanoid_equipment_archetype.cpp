#include "humanoid_equipment_archetype.h"

namespace Render::GL {
namespace {

auto resolver() -> EquipmentArchetype::Resolver& {
  static EquipmentArchetype::Resolver instance("humanoid");
  return instance;
}

} // namespace

void register_humanoid_equipment_contribution(
    EquipmentHandle handle, HumanoidEquipmentContribution contribution) {
  resolver().register_contribution(handle, contribution);
}

auto resolve_humanoid_equipment_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId {
  return resolver().resolve(debug_name, base_archetype_id, handles);
}

} // namespace Render::GL
