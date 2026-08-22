#include "render/humanoid/asset/humanoid_capabilities.h"

#include "render/creature/archetype_registry.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

auto resolve_humanoid_capabilities(Render::Creature::ArchetypeId archetype_id) noexcept
    -> HumanoidCapabilities {
  HumanoidCapabilities capabilities;

  auto const* descriptor =
      Render::Creature::ArchetypeRegistry::instance().get(archetype_id);
  if (descriptor == nullptr) {
    return capabilities;
  }

  constexpr auto k_hand_l_bone = static_cast<std::uint16_t>(HumanoidBone::HandL);
  for (auto const& attachment : descriptor->attachments_view()) {
    if (attachment.socket_bone_index == k_hand_l_bone) {
      capabilities.add(HumanoidCapability::LeftHandShield);
      break;
    }
  }

  return capabilities;
}

} // namespace Render::Humanoid
