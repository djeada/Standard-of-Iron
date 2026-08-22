#include "attachment_resolver.h"

namespace Render::Creature::Quadruped {

auto bone_delta(std::span<const QMatrix4x4> pose,
                std::span<const QMatrix4x4> bind,
                std::size_t bone) noexcept -> BoneDelta {
  if (bone >= pose.size() || bone >= bind.size()) {
    return BoneDelta{};
  }
  return BoneDelta{pose[bone] * bind[bone].inverted()};
}

} // namespace Render::Creature::Quadruped
