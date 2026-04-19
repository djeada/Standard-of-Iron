

#pragma once

#include "../rig_def.h"

namespace Render::RigDSL::HumanoidHead {

enum Anchor : AnchorId {
  A_NeckBase = 0,
  A_ChinPos = 1,
  A_HeadPos = 2,
};

enum Scalar : ScalarId {
  S_NeckR = 0,
  S_HeadR = 1,
};

inline constexpr PartDef kParts[] = {

    {PartKind::Cylinder,
     2,
     0xFFU,
     PaletteSlot::Skin,
     {0, 0, 0, 0},
     A_NeckBase,
     A_ChinPos,
     S_NeckR,
     1.0F,
     1.0F,
     1.0F,
     1.0F,
     0.9F},

    {PartKind::Sphere,
     2,
     0xFFU,
     PaletteSlot::Skin,
     {0, 0, 0, 0},
     A_HeadPos,
     kInvalidAnchor,
     S_HeadR,
     1.0F,
     1.0F,
     1.0F,
     1.0F,
     1.0F},
};

inline constexpr auto kRig = make_rig("humanoid_head", kParts);

} // namespace Render::RigDSL::HumanoidHead
