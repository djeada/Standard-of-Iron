#pragma once

/// Declarative horse creature profile data: dimension ranges, color/material
/// variant constants, and gait timing/stride constants used by the horse rig.

#include <cstdint>

namespace Render::GL {

namespace HorseDimensionRange {

constexpr float kBodyLengthMin = 0.92F;
constexpr float kBodyLengthMax = 1.08F;
constexpr float kBodyWidthMin = 0.22F;
constexpr float kBodyWidthMax = 0.30F;
constexpr float kBodyHeightMin = 0.44F;
constexpr float kBodyHeightMax = 0.54F;

constexpr float kNeckLengthMin = 0.48F;
constexpr float kNeckLengthMax = 0.58F;
constexpr float kNeckRiseMin = 0.30F;
constexpr float kNeckRiseMax = 0.38F;

constexpr float kHeadLengthMin = 0.36F;
constexpr float kHeadLengthMax = 0.46F;
constexpr float kHeadWidthMin = 0.16F;
constexpr float kHeadWidthMax = 0.20F;
constexpr float kHeadHeightMin = 0.22F;
constexpr float kHeadHeightMax = 0.28F;
constexpr float kMuzzleLengthMin = 0.16F;
constexpr float kMuzzleLengthMax = 0.20F;

constexpr float kLegLengthMin = 1.05F;
constexpr float kLegLengthMax = 1.18F;
constexpr float kHoofHeightMin = 0.095F;
constexpr float kHoofHeightMax = 0.115F;

constexpr float kTailLengthMin = 0.55F;
constexpr float kTailLengthMax = 0.72F;

constexpr float kSaddleThicknessMin = 0.035F;
constexpr float kSaddleThicknessMax = 0.045F;
constexpr float kSeatForwardOffsetMin = 0.010F;
constexpr float kSeatForwardOffsetMax = 0.035F;
constexpr float kStirrupOutScaleMin = 0.75F;
constexpr float kStirrupOutScaleMax = 0.88F;
constexpr float kStirrupDropMin = 0.28F;
constexpr float kStirrupDropMax = 0.32F;

constexpr float kIdleBobAmplitudeMin = 0.004F;
constexpr float kIdleBobAmplitudeMax = 0.007F;
constexpr float kMoveBobAmplitudeMin = 0.024F;
constexpr float kMoveBobAmplitudeMax = 0.032F;

constexpr float kLegSegmentRatioUpper = 0.59F;
constexpr float kLegSegmentRatioMiddle = 0.30F;
constexpr float kLegSegmentRatioLower = 0.12F;
constexpr float kShoulderBarrelOffsetScale = 0.05F;
constexpr float kShoulderBarrelOffsetBase = 0.05F;
constexpr float kSaddleHeightBodyScale = 0.55F;

constexpr uint32_t kSaltBodyLength = 0x12U;
constexpr uint32_t kSaltBodyWidth = 0x34U;
constexpr uint32_t kSaltBodyHeight = 0x56U;
constexpr uint32_t kSaltNeckLength = 0x9AU;
constexpr uint32_t kSaltNeckRise = 0xBCU;
constexpr uint32_t kSaltHeadLength = 0xDEU;
constexpr uint32_t kSaltHeadWidth = 0xF1U;
constexpr uint32_t kSaltHeadHeight = 0x1357U;
constexpr uint32_t kSaltMuzzleLength = 0x2468U;
constexpr uint32_t kSaltLegLength = 0x369CU;
constexpr uint32_t kSaltHoofHeight = 0x48AEU;
constexpr uint32_t kSaltTailLength = 0x5ABCU;
constexpr uint32_t kSaltSaddleThickness = 0x6CDEU;
constexpr uint32_t kSaltSeatForwardOffset = 0x7531U;
constexpr uint32_t kSaltStirrupOut = 0x8642U;
constexpr uint32_t kSaltStirrupDrop = 0x9753U;
constexpr uint32_t kSaltIdleBob = 0xA864U;
constexpr uint32_t kSaltMoveBob = 0xB975U;

} // namespace HorseDimensionRange

namespace HorseVariantConstants {

constexpr float kGrayCoatThreshold = 0.18F;
constexpr float kBayCoatThreshold = 0.38F;
constexpr float kChestnutCoatThreshold = 0.65F;
constexpr float kBlackCoatThreshold = 0.85F;

constexpr float kGrayCoatR = 0.70F;
constexpr float kGrayCoatG = 0.68F;
constexpr float kGrayCoatB = 0.63F;
constexpr float kBayCoatR = 0.40F;
constexpr float kBayCoatG = 0.30F;
constexpr float kBayCoatB = 0.22F;
constexpr float kChestnutCoatR = 0.28F;
constexpr float kChestnutCoatG = 0.22F;
constexpr float kChestnutCoatB = 0.19F;
constexpr float kBlackCoatR = 0.18F;
constexpr float kBlackCoatG = 0.15F;
constexpr float kBlackCoatB = 0.13F;
constexpr float kDunCoatR = 0.48F;
constexpr float kDunCoatG = 0.42F;
constexpr float kDunCoatB = 0.39F;

constexpr float kBlazeChanceThreshold = 0.82F;
constexpr float kBlazeColorR = 0.92F;
constexpr float kBlazeColorG = 0.92F;
constexpr float kBlazeColorB = 0.90F;
constexpr float kBlazeBlendFactor = 0.25F;

constexpr float kManeBlendMin = 0.55F;
constexpr float kManeBlendMax = 0.85F;
constexpr float kManeBaseR = 0.10F;
constexpr float kManeBaseG = 0.09F;
constexpr float kManeBaseB = 0.08F;
constexpr float kTailBlendFactor = 0.35F;

constexpr float kMuzzleBlendFactor = 0.65F;
constexpr float kMuzzleBaseR = 0.18F;
constexpr float kMuzzleBaseG = 0.14F;
constexpr float kMuzzleBaseB = 0.12F;
constexpr float kHoofDarkR = 0.16F;
constexpr float kHoofDarkG = 0.14F;
constexpr float kHoofDarkB = 0.12F;
constexpr float kHoofLightR = 0.40F;
constexpr float kHoofLightG = 0.35F;
constexpr float kHoofLightB = 0.32F;
constexpr float kHoofBlendMin = 0.15F;
constexpr float kHoofBlendMax = 0.65F;

constexpr float kLeatherToneMin = 0.78F;
constexpr float kLeatherToneMax = 0.96F;
constexpr float kTackToneMin = 0.58F;
constexpr float kTackToneMax = 0.78F;
constexpr float kSpecialTackThreshold = 0.90F;
constexpr float kSpecialTackR = 0.18F;
constexpr float kSpecialTackG = 0.19F;
constexpr float kSpecialTackB = 0.22F;
constexpr float kSpecialTackBlend = 0.25F;

constexpr float kBlanketTintMin = 0.92F;
constexpr float kBlanketTintMax = 1.05F;

constexpr float kEarInnerBaseR = 0.45F;
constexpr float kEarInnerBaseG = 0.35F;
constexpr float kEarInnerBaseB = 0.32F;
constexpr float kEarInnerBlendFactor = 0.30F;

constexpr uint32_t kSaltCoatHue = 0x23456U;
constexpr uint32_t kSaltBlazeChance = 0x1122U;
constexpr uint32_t kSaltManeBlend = 0x3344U;
constexpr uint32_t kSaltHoofBlend = 0x5566U;
constexpr uint32_t kSaltLeatherTone = 0x7788U;
constexpr uint32_t kSaltTackTone = 0x88AAU;
constexpr uint32_t kSaltBlanketTint = 0x99B0U;

} // namespace HorseVariantConstants

namespace HorseGaitConstants {

constexpr float kCycleTimeMin = 0.60F;
constexpr float kCycleTimeMax = 0.72F;
constexpr float kFrontLegPhaseMin = 0.08F;
constexpr float kFrontLegPhaseMax = 0.16F;
constexpr float kDiagonalLeadMin = 0.44F;
constexpr float kDiagonalLeadMax = 0.54F;
constexpr float kStrideSwingMin = 0.26F;
constexpr float kStrideSwingMax = 0.32F;
constexpr float kStrideLiftMin = 0.10F;
constexpr float kStrideLiftMax = 0.14F;

constexpr uint32_t kSaltCycleTime = 0xAA12U;
constexpr uint32_t kSaltFrontLegPhase = 0xBB34U;
constexpr uint32_t kSaltDiagonalLead = 0xCC56U;
constexpr uint32_t kSaltStrideSwing = 0xDD78U;
constexpr uint32_t kSaltStrideLift = 0xEE9AU;

} // namespace HorseGaitConstants

} // namespace Render::GL
