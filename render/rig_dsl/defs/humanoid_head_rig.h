// Stage 8c — humanoid head + neck as a data-driven RigDef.
//
// This is the smallest meaningful humanoid slice: two parts (skin-coloured
// neck cylinder and skin-coloured head sphere). It proves that the Stage 8
// DSL scales from equipment (helmet, Stage 8b) to character anatomy without
// needing any new primitive kinds — only the existing Cylinder + Sphere
// parts plus the ScalarResolver hook added in 8b for dynamic per-unit radii.
//
// Anchors are bound at emit time by a resolver that reads the current
// HumanoidPose. Palette is bound to the unit's skin slot (with a 0.9×
// multiplier on the neck to match the tint used in skin.cpp).

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
    // Neck: skin * 0.9, cylinder_between(neck_base, chin_pos, neck_r).
    {PartKind::Cylinder, /*material_id=*/2, /*lod_mask=*/0xFFU,
     PaletteSlot::Skin, /*literal_color=*/{0, 0, 0, 0}, A_NeckBase, A_ChinPos,
     S_NeckR, /*size_x=*/1.0F, /*size_y=*/1.0F, /*size_z=*/1.0F,
     /*alpha=*/1.0F, /*color_scale=*/0.9F},

    // Head: skin, sphere at head_pos with head_r radius.
    {PartKind::Sphere, /*material_id=*/2, /*lod_mask=*/0xFFU,
     PaletteSlot::Skin, /*literal_color=*/{0, 0, 0, 0}, A_HeadPos,
     kInvalidAnchor, S_HeadR, /*size_x=*/1.0F, /*size_y=*/1.0F,
     /*size_z=*/1.0F, /*alpha=*/1.0F, /*color_scale=*/1.0F},
};

inline constexpr auto kRig = make_rig("humanoid_head", kParts);

} // namespace Render::RigDSL::HumanoidHead
