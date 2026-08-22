#pragma once

#include "render/creature/skeleton_blend_profile.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

inline constexpr Render::Creature::BoneMask k_upper_body_bones =
    Render::Creature::BoneMask::of({
        static_cast<std::size_t>(HumanoidBone::Chest),
        static_cast<std::size_t>(HumanoidBone::Neck),
        static_cast<std::size_t>(HumanoidBone::Head),
        static_cast<std::size_t>(HumanoidBone::ShoulderL),
        static_cast<std::size_t>(HumanoidBone::UpperArmL),
        static_cast<std::size_t>(HumanoidBone::ForearmL),
        static_cast<std::size_t>(HumanoidBone::HandL),
        static_cast<std::size_t>(HumanoidBone::ShoulderR),
        static_cast<std::size_t>(HumanoidBone::UpperArmR),
        static_cast<std::size_t>(HumanoidBone::ForearmR),
        static_cast<std::size_t>(HumanoidBone::HandR),
    });

inline constexpr Render::Creature::SkeletonBlendProfile k_humanoid_blend_profile{
    .upper_body = k_upper_body_bones};

} // namespace Render::Humanoid
