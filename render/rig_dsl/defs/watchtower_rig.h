#pragma once

// Stage 8a proof-of-concept rig: a simple watchtower.
//
// This is intentionally trivial — a base cylinder, four leg cylinders, a
// platform box, a roof cone. The point is to show the entire rig fits in
// ~30 lines of data instead of a ~300-line imperative C++ class.
//
// Anchor IDs here are local to the watchtower — a matching AnchorResolver
// knows how to turn each into a unit-local QVector3D.

#include "../rig_def.h"

namespace Render::RigDSL::Watchtower {

enum Anchor : AnchorId {
  Base_Centre,
  Base_Top_Centre,
  Leg_Front_Left_Bot,
  Leg_Front_Left_Top,
  Leg_Front_Right_Bot,
  Leg_Front_Right_Top,
  Leg_Back_Left_Bot,
  Leg_Back_Left_Top,
  Leg_Back_Right_Bot,
  Leg_Back_Right_Top,
  Platform_Low_Corner,
  Platform_High_Corner,
  Roof_Base,
  Roof_Apex,
};

inline constexpr PartDef kParts[] = {
    // Stone base pedestal.
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Wood, {180, 165, 140, 255},
     Base_Centre, Base_Top_Centre, kInvalidScalar, 0.9F, 1.0F, 1.0F, 1.0F, 1.0F},
    // Four wooden legs — dropped at LODs where bit is clear.
    {PartKind::Cylinder, 0, 0x07U, PaletteSlot::Wood, {128, 90, 50, 255},
     Leg_Front_Left_Bot, Leg_Front_Left_Top, kInvalidScalar, 0.12F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0x07U, PaletteSlot::Wood, {128, 90, 50, 255},
     Leg_Front_Right_Bot, Leg_Front_Right_Top, kInvalidScalar, 0.12F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0x07U, PaletteSlot::Wood, {128, 90, 50, 255},
     Leg_Back_Left_Bot, Leg_Back_Left_Top, kInvalidScalar, 0.12F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0x07U, PaletteSlot::Wood, {128, 90, 50, 255},
     Leg_Back_Right_Bot, Leg_Back_Right_Top, kInvalidScalar, 0.12F, 1.0F, 1.0F, 1.0F, 1.0F},
    // Wooden platform.
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Wood, {150, 110, 70, 255},
     Platform_Low_Corner, Platform_High_Corner, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    // Roof cone.
    {PartKind::Cone, 0, 0xFFU, PaletteSlot::Wood, {90, 55, 35, 255}, Roof_Base,
     Roof_Apex, kInvalidScalar, 1.3F, 1.0F, 1.0F, 1.0F, 1.0F},
};

inline constexpr RigDef kRig = make_rig("watchtower", kParts);

} // namespace Render::RigDSL::Watchtower
