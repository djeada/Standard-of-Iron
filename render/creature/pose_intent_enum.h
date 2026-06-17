#pragma once

#include "animation/pose_manifest.h"

namespace Render::Creature {

using PoseIntent = Animation::PoseIntent;

[[nodiscard]] constexpr auto pose_intent_count() noexcept -> std::size_t {
  return Animation::pose_intent_count();
}

} // namespace Render::Creature
