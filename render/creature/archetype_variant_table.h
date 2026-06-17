#pragma once

#include "animation/selection_manifest.h"
#include "pose_intent_enum.h"
#include "render_request.h"

namespace Render::Creature {

inline constexpr std::size_t k_max_visual_variants = Animation::k_max_visual_variants;

using ArchetypeVariantTable = Animation::ArchetypeVariantTable;

} // namespace Render::Creature
