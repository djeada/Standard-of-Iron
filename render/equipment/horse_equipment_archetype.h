#pragma once

#include "../creature/archetype_registry.h"
#include "equipment_registry.h"

#include <QVector3D>

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace Render::GL {

struct HorseEquipmentContribution {
  using BuildAttachmentsFn =
      std::vector<Render::Creature::StaticAttachmentSpec> (*)(std::uint8_t);

  BuildAttachmentsFn build_attachments{nullptr};
  Render::Creature::ArchetypeDescriptor::ExtraRoleColorsFn append_role_colors{
      nullptr};
  std::uint8_t role_count{0};
};

void register_horse_equipment_contribution(
    EquipmentHandle handle, HorseEquipmentContribution contribution);

[[nodiscard]] auto resolve_horse_equipment_archetype(
    std::string_view debug_name,
    Render::Creature::ArchetypeId base_archetype_id,
    std::span<const EquipmentHandle> handles) -> Render::Creature::ArchetypeId;

} // namespace Render::GL
