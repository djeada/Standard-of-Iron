#pragma once

#include <cstdint>

namespace Render::GL {

namespace ElephantDimensionRange {

constexpr float kBodyLengthMin = 0.7333333F;
constexpr float kBodyLengthMax = 0.8666667F;
constexpr float kBodyWidthMin = 0.30F;
constexpr float kBodyWidthMax = 0.3666667F;
constexpr float kBodyHeightMin = 0.40F;
constexpr float kBodyHeightMax = 0.50F;

constexpr float kNeckLengthMin = 0.175F;
constexpr float kNeckLengthMax = 0.25F;
constexpr float kNeckWidthMin = 0.225F;
constexpr float kNeckWidthMax = 0.275F;

constexpr float kHeadLengthMin = 0.275F;
constexpr float kHeadLengthMax = 0.35F;
constexpr float kHeadWidthMin = 0.25F;
constexpr float kHeadWidthMax = 0.325F;
constexpr float kHeadHeightMin = 0.275F;
constexpr float kHeadHeightMax = 0.35F;

constexpr float kTrunkLengthMin = 0.80F;
constexpr float kTrunkLengthMax = 1.00F;
constexpr float kTrunkBaseRadiusMin = 0.09F;
constexpr float kTrunkBaseRadiusMax = 0.12F;
constexpr float kTrunkTipRadiusMin = 0.02F;
constexpr float kTrunkTipRadiusMax = 0.035F;

constexpr float kEarWidthMin = 0.35F;
constexpr float kEarWidthMax = 0.45F;
constexpr float kEarHeightMin = 0.40F;
constexpr float kEarHeightMax = 0.50F;
constexpr float kEarThicknessMin = 0.012F;
constexpr float kEarThicknessMax = 0.022F;

constexpr float kLegLengthMin = 0.70F;
constexpr float kLegLengthMax = 0.85F;
constexpr float kLegRadiusMin = 0.09F;
constexpr float kLegRadiusMax = 0.125F;
constexpr float kFootRadiusMin = 0.11F;
constexpr float kFootRadiusMax = 0.15F;

constexpr float kTailLengthMin = 0.35F;
constexpr float kTailLengthMax = 0.475F;

constexpr float kTuskLengthMin = 0.25F;
constexpr float kTuskLengthMax = 0.425F;
constexpr float kTuskRadiusMin = 0.02F;
constexpr float kTuskRadiusMax = 0.035F;

constexpr float kHowdahWidthMin = 0.40F;
constexpr float kHowdahWidthMax = 0.50F;
constexpr float kHowdahLengthMin = 0.50F;
constexpr float kHowdahLengthMax = 0.65F;
constexpr float kHowdahHeightMin = 0.20F;
constexpr float kHowdahHeightMax = 0.275F;

constexpr float kIdleBobAmplitudeMin = 0.004F;
constexpr float kIdleBobAmplitudeMax = 0.0075F;
constexpr float kMoveBobAmplitudeMin = 0.0175F;
constexpr float kMoveBobAmplitudeMax = 0.0275F;

constexpr uint32_t kSaltBodyLength = 0x12U;
constexpr uint32_t kSaltBodyWidth = 0x34U;
constexpr uint32_t kSaltBodyHeight = 0x56U;
constexpr uint32_t kSaltNeckLength = 0x78U;
constexpr uint32_t kSaltNeckWidth = 0x9AU;
constexpr uint32_t kSaltHeadLength = 0xBCU;
constexpr uint32_t kSaltHeadWidth = 0xDEU;
constexpr uint32_t kSaltHeadHeight = 0xF0U;
constexpr uint32_t kSaltTrunkLength = 0x123U;
constexpr uint32_t kSaltTrunkBaseRadius = 0x234U;
constexpr uint32_t kSaltTrunkTipRadius = 0x345U;
constexpr uint32_t kSaltEarWidth = 0x456U;
constexpr uint32_t kSaltEarHeight = 0x567U;
constexpr uint32_t kSaltEarThickness = 0x678U;
constexpr uint32_t kSaltLegLength = 0x789U;
constexpr uint32_t kSaltLegRadius = 0x89AU;
constexpr uint32_t kSaltFootRadius = 0x9ABU;
constexpr uint32_t kSaltTailLength = 0xABCU;
constexpr uint32_t kSaltTuskLength = 0xBCDU;
constexpr uint32_t kSaltTuskRadius = 0xCDEU;
constexpr uint32_t kSaltHowdahWidth = 0xDEFU;
constexpr uint32_t kSaltHowdahLength = 0xEF0U;
constexpr uint32_t kSaltHowdahHeight = 0xF01U;
constexpr uint32_t kSaltIdleBob = 0x102U;
constexpr uint32_t kSaltMoveBob = 0x213U;

} // namespace ElephantDimensionRange

namespace ElephantVariantConstants {

constexpr float kSkinBaseR = 0.65F;
constexpr float kSkinBaseG = 0.61F;
constexpr float kSkinBaseB = 0.58F;

constexpr float kSkinVariationMin = 0.85F;
constexpr float kSkinVariationMax = 1.15F;

constexpr float kHighlightBlend = 0.15F;
constexpr float kShadowBlend = 0.20F;

constexpr float kEarInnerR = 0.55F;
constexpr float kEarInnerG = 0.45F;
constexpr float kEarInnerB = 0.42F;

constexpr float kTuskR = 1.0F;
constexpr float kTuskG = 1.0F;
constexpr float kTuskB = 1.0F;

constexpr float kToenailR = 0.35F;
constexpr float kToenailG = 0.32F;
constexpr float kToenailB = 0.28F;

constexpr float kWoodR = 0.45F;
constexpr float kWoodG = 0.32F;
constexpr float kWoodB = 0.22F;

constexpr uint32_t kSaltSkinVariation = 0x324U;
constexpr uint32_t kSaltHighlight = 0x435U;
constexpr uint32_t kSaltShadow = 0x546U;

} // namespace ElephantVariantConstants

namespace ElephantGaitConstants {

constexpr float kCycleTimeMin = 2.20F;
constexpr float kCycleTimeMax = 2.80F;
constexpr float kFrontLegPhaseMin = 0.0F;
constexpr float kFrontLegPhaseMax = 0.10F;
constexpr float kDiagonalLeadMin = 0.48F;
constexpr float kDiagonalLeadMax = 0.52F;

constexpr float kStrideSwingMin = 0.55F;
constexpr float kStrideSwingMax = 0.75F;

constexpr float kStrideLiftMin = 0.18F;
constexpr float kStrideLiftMax = 0.26F;

constexpr uint32_t kSaltCycleTime = 0x657U;
constexpr uint32_t kSaltFrontLegPhase = 0x768U;
constexpr uint32_t kSaltDiagonalLead = 0x879U;
constexpr uint32_t kSaltStrideSwing = 0x98AU;
constexpr uint32_t kSaltStrideLift = 0xA9BU;

} // namespace ElephantGaitConstants

} // namespace Render::GL
