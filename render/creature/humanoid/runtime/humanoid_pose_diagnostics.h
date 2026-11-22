#pragma once

#include <QMatrix4x4>

#include <cstdint>

#include "render/creature/pipeline/creature_asset.h"

namespace Render::Humanoid {

void record_humanoid_body_pose(std::uint32_t entity_id,
                               std::uint16_t instance_index,
                               const QMatrix4x4& world,
                               std::uint32_t bone_count,
                               Render::Creature::Pipeline::BoneOriginFn origin,
                               const void* user);

} // namespace Render::Humanoid
