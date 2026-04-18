// Stage 11 — Roman barracks static-structure rig.
//
// The barracks renderer is a mix of static structural geometry (platform,
// chamber walls, central courtyard, trading goods) and highly dynamic
// pieces (state-dependent damage, animated cloth banner, health bar,
// selection ring, rally flag). Only the former is data-expressible with
// the current DSL — the latter need per-frame uniforms, state queries,
// or non-DSL primitives (SelectionSmoke, ClothBanner).
//
// So this rig captures exactly the unconditional structural mass. A
// successful migration of these parts proves:
//   * the DSL scales to dozens of parts, not just a 6-part watchtower;
//   * a per-nation palette (RomanPalette) can drive a PaletteResolver
//     without adding new DSL types.
// Everything else in barracks_renderer.cpp is left as imperative code
// since it's not geometry — it's per-frame state binding.

#pragma once

#include "../rig_def.h"

namespace Render::RigDSL::Barracks {

// Static anchors for the barracks' structural geometry. Values are in
// unit-local space (the same frame draw_* helpers used when calling
// `p.model.translate(...)` with these literals).
enum Anchor : AnchorId {
  // Platform base (box diagonal A/B corners, not centre/extent).
  Platform_BaseLow,
  Platform_BaseHigh,
  Platform_TopLow,
  Platform_TopHigh,

  // Central courtyard stone.
  Court_StoneLow,
  Court_StoneHigh,
  Court_PoolLow,
  Court_PoolHigh,
  Court_PoolTrimSLow,
  Court_PoolTrimSHigh,
  Court_PoolTrimNLow,
  Court_PoolTrimNHigh,

  // Courtyard centre pillar (cylinder endpoints + cap corners).
  Court_PillarBot,
  Court_PillarTop,
  Court_PillarCapLow,
  Court_PillarCapHigh,

  // Chamber walls (three wall boxes).
  Wall_BackLow,
  Wall_BackHigh,
  Wall_LeftLow,
  Wall_LeftHigh,
  Wall_RightLow,
  Wall_RightHigh,

  // Chamber doors (two door slabs + lintels).
  Door_LDoorLow,
  Door_LDoorHigh,
  Door_LLintelLow,
  Door_LLintelHigh,
  Door_RDoorLow,
  Door_RDoorHigh,
  Door_RLintelLow,
  Door_RLintelHigh,

  // Trading goods (three cylinders — amphora stand-ins).
  Goods_Amp1Bot,
  Goods_Amp1Top,
  Goods_Amp2Bot,
  Goods_Amp2Top,
  Goods_Amp3Bot,
  Goods_Amp3Top,
};

// Packed colours (kept identical to RomanPalette defaults — the
// PaletteResolver on the barracks side overrides *_team* slots; pure
// structural parts use literal colours that never need tinting).
namespace C {
inline constexpr PackedColor kLimestone{245, 239, 224, 255};
inline constexpr PackedColor kLimestoneShade{224, 217, 199, 255};
inline constexpr PackedColor kLimestoneDark{204, 194, 178, 255};
inline constexpr PackedColor kMarble{250, 247, 242, 255};
inline constexpr PackedColor kCedarDark{97, 66, 41, 255};
inline constexpr PackedColor kTerracotta{209, 158, 115, 255};
inline constexpr PackedColor kTerracottaDark{173, 122, 82, 255};
inline constexpr PackedColor kBlueAccent{71, 122, 173, 255};
inline constexpr PackedColor kBlueLight{102, 153, 204, 255};
} // namespace C

inline constexpr PartDef kParts[] = {
    // --- Platform (two-layer pedestal) ---
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestoneDark,
     Platform_BaseLow, Platform_BaseHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Platform_TopLow, Platform_TopHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},

    // --- Central courtyard ---
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestoneShade,
     Court_StoneLow, Court_StoneHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueLight,
     Court_PoolLow, Court_PoolHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PoolTrimSLow, Court_PoolTrimSHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PoolTrimNLow, Court_PoolTrimNHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kMarble,
     Court_PillarBot, Court_PillarTop, kInvalidScalar,
     0.06F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PillarCapLow, Court_PillarCapHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},

    // --- Chamber walls ---
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Wall_BackLow, Wall_BackHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Wall_LeftLow, Wall_LeftHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Wall_RightLow, Wall_RightHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},

    // --- Doors + lintels ---
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kCedarDark,
     Door_LDoorLow, Door_LDoorHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Door_LLintelLow, Door_LLintelHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kCedarDark,
     Door_RDoorLow, Door_RDoorHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Door_RLintelLow, Door_RLintelHigh, kInvalidScalar,
     1.0F, 1.0F, 1.0F, 1.0F, 1.0F},

    // --- Trading goods (cylinders for amphora stand-ins) ---
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kTerracottaDark,
     Goods_Amp1Bot, Goods_Amp1Top, kInvalidScalar,
     0.08F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kTerracotta,
     Goods_Amp2Bot, Goods_Amp2Top, kInvalidScalar,
     0.07F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Goods_Amp3Bot, Goods_Amp3Top, kInvalidScalar,
     0.06F, 1.0F, 1.0F, 1.0F, 1.0F},
};

inline constexpr RigDef kRig = make_rig("barracks_roman_structure", kParts);

} // namespace Render::RigDSL::Barracks
