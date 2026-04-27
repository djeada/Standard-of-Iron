

#pragma once

#include "../rig_def.h"

namespace Render::RigDSL::Barracks {

enum Anchor : AnchorId {

  Platform_BaseLow,
  Platform_BaseHigh,
  Platform_TopLow,
  Platform_TopHigh,

  Court_StoneLow,
  Court_StoneHigh,
  Court_PoolLow,
  Court_PoolHigh,
  Court_PoolTrimSLow,
  Court_PoolTrimSHigh,
  Court_PoolTrimNLow,
  Court_PoolTrimNHigh,

  Court_PillarBot,
  Court_PillarTop,
  Court_PillarCapLow,
  Court_PillarCapHigh,

  Wall_BackLow,
  Wall_BackHigh,
  Wall_LeftLow,
  Wall_LeftHigh,
  Wall_RightLow,
  Wall_RightHigh,

  Door_LDoorLow,
  Door_LDoorHigh,
  Door_LLintelLow,
  Door_LLintelHigh,
  Door_RDoorLow,
  Door_RDoorHigh,
  Door_RLintelLow,
  Door_RLintelHigh,

  Goods_Amp1Bot,
  Goods_Amp1Top,
  Goods_Amp2Bot,
  Goods_Amp2Top,
  Goods_Amp3Bot,
  Goods_Amp3Top,
};

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

    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestoneDark,
     Platform_BaseLow, Platform_BaseHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F,
     1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Platform_TopLow, Platform_TopHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},

    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestoneShade,
     Court_StoneLow, Court_StoneHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueLight,
     Court_PoolLow, Court_PoolHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PoolTrimSLow, Court_PoolTrimSHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F,
     1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PoolTrimNLow, Court_PoolTrimNHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F,
     1.0F, 1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kMarble,
     Court_PillarBot, Court_PillarTop, kInvalidScalar, 0.06F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Court_PillarCapLow, Court_PillarCapHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F,
     1.0F, 1.0F},

    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone, Wall_BackLow,
     Wall_BackHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone, Wall_LeftLow,
     Wall_LeftHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F, 1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kLimestone,
     Wall_RightLow, Wall_RightHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},

    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kCedarDark,
     Door_LDoorLow, Door_LDoorHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Door_LLintelLow, Door_LLintelHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kCedarDark,
     Door_RDoorLow, Door_RDoorHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Box, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Door_RLintelLow, Door_RLintelHigh, kInvalidScalar, 1.0F, 1.0F, 1.0F, 1.0F,
     1.0F},

    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kTerracottaDark,
     Goods_Amp1Bot, Goods_Amp1Top, kInvalidScalar, 0.08F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kTerracotta,
     Goods_Amp2Bot, Goods_Amp2Top, kInvalidScalar, 0.07F, 1.0F, 1.0F, 1.0F,
     1.0F},
    {PartKind::Cylinder, 0, 0xFFU, PaletteSlot::Literal, C::kBlueAccent,
     Goods_Amp3Bot, Goods_Amp3Top, kInvalidScalar, 0.06F, 1.0F, 1.0F, 1.0F,
     1.0F},
};

inline constexpr RigDef kRig = make_rig("barracks_roman_structure", kParts);

} // namespace Render::RigDSL::Barracks
