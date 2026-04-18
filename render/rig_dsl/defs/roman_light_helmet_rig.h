#pragma once

// Stage 8b — Declarative definition of the Roman light-cavalry helmet.
//
// Eight parts, each described in one line of data. Compare to the original
// 90-line imperative render() in roman_light_helmet.cpp. Dynamic radii live
// behind named ScalarIds so the resolver can scale everything off the
// current head_r without touching the def.

#include "../rig_def.h"

namespace Render::RigDSL::RomanLightHelmet {

// Anchor points (all resolved relative to the head AttachmentFrame).
enum Anchor : AnchorId {
  Helmet_Top,
  Helmet_Bot,
  Apex,
  Ring_Low_Top,
  Ring_Low_Bot,
  Ring_High_Top,
  Ring_High_Bot,
  Neck_Guard_Top,
  Neck_Guard_Bot,
  Crest_Base,
  Crest_Mid,
  Crest_Top,
};

// Scalar slots: radii that scale with head_r.
enum Scalar : ScalarId {
  S_Helmet_R,          // head_r * 1.08
  S_Helmet_R_Apex,     // helmet_r * 0.97
  S_Helmet_R_Ring_Low, // helmet_r * 1.06
  S_Helmet_R_Ring_Hi,  // helmet_r * 1.02
  S_Helmet_R_Neck,     // helmet_r * 0.86
};

// NOTE: colour_scale fine-tunes the palette-sourced colour to reproduce
// the exact shades the imperative helmet used (helmet_color * 1.14 for
// accent, 1.04 for the high ring, etc.). The palette resolver supplies
// the helmet_color (metal * warm-tint) as Metal and helmet_accent as
// Metal_Accent — see RomanLightHelmetPalette in the .cpp.
inline constexpr PartDef kParts[] = {
    // 1. Main helmet cylinder: helmet_color, scale 1.0 * helmet_r
    {PartKind::Cylinder, 2, 0xFFU, PaletteSlot::Metal, {},
     Helmet_Bot, Helmet_Top, S_Helmet_R, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    // 2. Apex cone: helmet_accent, scale 0.97 * helmet_r
    {PartKind::Cone, 2, 0xFFU, PaletteSlot::Metal_Accent, {},
     Helmet_Top, Apex, S_Helmet_R_Apex, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    // 3. Low ring: helmet_accent * 1.0, scale 1.06 * helmet_r
    {PartKind::Cylinder, 2, 0xFFU, PaletteSlot::Metal_Accent, {},
     Ring_Low_Bot, Ring_Low_Top, S_Helmet_R_Ring_Low, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    // 4. High ring: helmet_color * 1.04, scale 1.02 * helmet_r
    {PartKind::Cylinder, 2, 0xFFU, PaletteSlot::Metal, {},
     Ring_High_Bot, Ring_High_Top, S_Helmet_R_Ring_Hi, 1.0F, 1.0F, 1.0F, 1.0F,
     1.04F},
    // 5. Neck guard: helmet_color * 0.90, scale 0.86 * helmet_r
    {PartKind::Cylinder, 2, 0xFFU, PaletteSlot::Metal, {},
     Neck_Guard_Bot, Neck_Guard_Top, S_Helmet_R_Neck, 1.0F, 1.0F, 1.0F, 1.0F,
     0.90F},
    // 6. Crest base rod: helmet_accent, absolute radius 0.018
    {PartKind::Cylinder, 2, 0xFFU, PaletteSlot::Metal_Accent, {},
     Crest_Base, Crest_Mid, kInvalidScalar, 0.018F, 1.0F, 1.0F, 1.0F, 1.0F},
    // 7. Crest top cone: literal red, absolute radius 0.042
    {PartKind::Cone, 0, 0xFFU, PaletteSlot::Literal, {224, 46, 46, 255},
     Crest_Mid, Crest_Top, kInvalidScalar, 0.042F, 1.0F, 1.0F, 1.0F, 1.0F},
    // 8. Crest tip sphere: helmet_accent, absolute radius 0.020
    {PartKind::Sphere, 2, 0xFFU, PaletteSlot::Metal_Accent, {},
     Crest_Top, kInvalidAnchor, kInvalidScalar, 0.020F, 1.0F, 1.0F, 1.0F, 1.0F},
};

inline constexpr RigDef kRig = make_rig("roman_light_helmet", kParts);

} // namespace Render::RigDSL::RomanLightHelmet
